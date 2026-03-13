#!/usr/bin/env python3
"""
Simulate a circuit ideally and with an IBMQ backend noise model, then compute
the process fidelity between the two channels.

Process fidelity F_proc is related to average state fidelity F_avg (over
computational basis states) by:
  F_avg = (d * F_proc + 1) / (d + 1)  =>  F_proc = ((d+1)*F_avg - 1) / d
where d = 2^n is the Hilbert space dimension.

Usage:
  python simulate_ideal_vs_noisy_fidelity.py circuit.qasm --backend fake_manila
  python simulate_ideal_vs_noisy_fidelity.py circuit.qasm --backend ibmq_manila  # real backend, needs IBMQ token

For connectivity-preserving gate nativization and optimization, run
  nativize_and_optimize_qasm.py circuit.qasm --backend <name> -o circuit.opt.qasm
first, then pass the resulting .qasm to this script.
"""

import argparse
import sys
from pathlib import Path

import numpy as np
from qiskit import QuantumCircuit, qasm2
from qiskit.circuit.library import PhaseGate, SXGate, SXdgGate, UGate
from qiskit_aer import AerSimulator
from qiskit_aer.noise import NoiseModel
from qiskit.quantum_info import Statevector, state_fidelity, Operator, Choi, process_fidelity

# Optional: use project's get_backend for real/fake backend
SCRIPT_DIR = Path(__file__).resolve().parent
if (SCRIPT_DIR / "get_backend.py").exists():
    sys.path.insert(0, str(SCRIPT_DIR))
    try:
        from get_backend import get_fake_backend, get_real_backend
        HAS_GET_BACKEND = True
    except ImportError:
        HAS_GET_BACKEND = False
else:
    HAS_GET_BACKEND = False

try:
    import qiskit_ibm_runtime.fake_provider as fake_provider
    HAS_FAKE_PROVIDER = True
except ImportError:
    HAS_FAKE_PROVIDER = False


def load_circuit(path: str) -> QuantumCircuit:
    """Load a circuit from a .qasm file."""
    # Qiskit's qasm2 uses the paper (arXiv) qelib1.inc, which does not define 'p', 'sx', or 'u'.
    # Add them as custom builtins so OpenQASM 2 from IBM backends and nativized output load.
    custom = [
        qasm2.CustomInstruction("p", 1, 1, PhaseGate, builtin=True),
        qasm2.CustomInstruction("sx", 0, 1, SXGate, builtin=True),
        qasm2.CustomInstruction("sxdg", 0, 1, SXdgGate, builtin=True),
        qasm2.CustomInstruction("u", 3, 1, UGate, builtin=True),
    ]
    return qasm2.load(path, custom_instructions=custom)


def get_noise_model_from_backend(backend) -> NoiseModel:
    """Build a NoiseModel from an IBM backend (real or fake)."""
    return NoiseModel.from_backend(backend)


def noise_model_for_active_qubits(full_noise_model: NoiseModel, active_indices: list[int]) -> NoiseModel:
    """
    Build a NoiseModel that applies only to the given backend qubit indices, with qubits
    remapped to 0..len(active_indices)-1. This preserves per-qubit and per-link fidelities
    (e.g. CX(21,34) keeps the noise for backend qubits 21 and 34, not 0 and 1).
    """
    old_to_new = {old: new for new, old in enumerate(active_indices)}
    active_set = set(active_indices)
    n_active = len(active_indices)
    new_model = NoiseModel()

    # Copy basis gates so the reduced circuit can be simulated
    for gate in full_noise_model.basis_gates:
        new_model.add_basis_gates([gate])

    # Copy local quantum errors for (gate, qubits) where all qubits are in active_indices
    local_errors = getattr(full_noise_model, "_local_quantum_errors", None) or {}
    for inst_name, qubit_dict in local_errors.items():
        for qubits, error in qubit_dict.items():
            qtuple = qubits if isinstance(qubits, tuple) else (qubits,)
            if all(q in active_set for q in qtuple):
                new_qubits = tuple(old_to_new[q] for q in qtuple)
                new_model.add_quantum_error(error, [inst_name], list(new_qubits), warnings=False)

    # Copy local readout errors for active qubits
    local_readout = getattr(full_noise_model, "_local_readout_errors", None) or {}
    for qubits, error in local_readout.items():
        qtuple = qubits if isinstance(qubits, tuple) else (qubits,)
        if all(q in active_set for q in qtuple):
            new_qubits = tuple(old_to_new[q] for q in qtuple)
            new_model.add_readout_error(error, list(new_qubits), warnings=False)

    # Default all-qubit errors: apply to our active qubits only (so each gets the same
    # default error; not per-backend-qubit but better than nothing when no local errors exist)
    default_errors = getattr(full_noise_model, "_default_quantum_errors", None) or {}
    for inst_name, error in default_errors.items():
        num_qubits = error.num_qubits
        if num_qubits == 1:
            for i in range(n_active):
                new_model.add_quantum_error(error, [inst_name], [i], warnings=False)
        # 2-qubit default: add for every ordered pair (control/target order can matter)
        elif num_qubits == 2:
            for i in range(n_active):
                for j in range(n_active):
                    if i != j:
                        new_model.add_quantum_error(error, [inst_name], [i, j], warnings=False)

    default_readout = getattr(full_noise_model, "_default_readout_error", None)
    if default_readout is not None:
        for i in range(n_active):
            new_model.add_readout_error(default_readout, [i], warnings=False)

    return new_model


def get_active_qubit_indices(circuit: QuantumCircuit) -> list[int]:
    """Return sorted list of qubit indices that appear in at least one instruction."""
    active = set()
    qubit_list = circuit.qubits
    for instr in circuit.data:
        for q in instr.qubits:
            active.add(qubit_list.index(q))
    return sorted(active)


def circuit_to_active_subcircuit(circuit: QuantumCircuit, active_indices: list[int]) -> QuantumCircuit:
    """
    Build a circuit containing only the given qubits, with indices remapped to 0..len(active_indices)-1.
    Instructions that only touch these qubits are included; others are dropped (should not occur if
    active_indices is the full set of qubits that appear in the circuit).
    """
    n_active = len(active_indices)
    old_to_new = {old: new for new, old in enumerate(active_indices)}
    out = QuantumCircuit(n_active)
    qc = circuit.remove_final_measurements(inplace=False)
    qubit_list = qc.qubits
    for instr in qc.data:
        qubit_indices = [qubit_list.index(q) for q in instr.qubits]
        if not all(i in old_to_new for i in qubit_indices):
            continue
        new_qubits = [old_to_new[i] for i in qubit_indices]
        out.append(instr.operation, new_qubits, instr.clbits)
    return out


def _density_matrix_from_result(result, experiment_index: int, dim: int):
    """Extract density matrix from Aer result (saved via save_density_matrix)."""
    data = result.data(experiment_index)
    # Try common keys; Aer may use different labels across versions
    for key in ("density_matrix", "DensityMatrix"):
        if key in data:
            dm = np.asarray(data[key])
            break
    else:
        # Use the first (and typically only) saved snapshot
        dm = np.asarray(list(data.values())[0])
    if dm.size == dim * dim and dm.ndim != 2:
        dm = np.reshape(dm, (dim, dim))
    return dm


def ideal_output_states(circuit: QuantumCircuit, n_qubits: int, basis_indices: list[int] | None = None):
    """
    Run ideal (statevector) simulation for each computational basis state.
    Yields (k, Statevector) for each basis index k.
    """
    # Remove measurements for statevector simulation
    qc = circuit.remove_final_measurements(inplace=False)
    dim = 2**n_qubits

    if basis_indices is None:
        basis_indices = list(range(dim))
    for k in basis_indices:
        # Prepare |k⟩ on the first n_qubits
        state = np.zeros(dim, dtype=complex)
        state[k] = 1.0
        sv = Statevector(state)
        out = sv.evolve(qc)
        yield k, out


def noisy_output_density_matrices(
    circuit: QuantumCircuit,
    noise_model: NoiseModel,
    n_qubits: int,
    basis_indices: list[int] | None = None,
):
    """
    Run noisy simulation for each computational basis state and estimate
    the output density matrix via repeated execution.
    Yields (k, density_matrix) for each basis index k.
    """
    dim = 2**n_qubits
    sim_dm = AerSimulator(noise_model=noise_model, method="density_matrix")
    qc = circuit.remove_final_measurements(inplace=False)

    if basis_indices is None:
        basis_indices = list(range(dim))
    for k in basis_indices:
        qc_init = QuantumCircuit(n_qubits)
        for q in range(n_qubits):
            if (k >> q) & 1:
                qc_init.x(q)
        full = qc_init.compose(qc, front=False)
        full.save_density_matrix()
        job = sim_dm.run(full, shots=1)
        r = job.result()
        dm = _density_matrix_from_result(r, 0, dim)
        yield k, dm


def average_state_fidelity_and_process_fidelity(
    circuit: QuantumCircuit,
    noise_model: NoiseModel,
    n_qubits: int,
    basis_indices: list[int] | None = None,
) -> tuple[float, float]:
    """
    Compute average state fidelity over computational basis states, then
    process fidelity using F_proc = ((d+1)*F_avg - 1) / d.
    Returns (average_state_fidelity, process_fidelity).
    """
    dim = 2**n_qubits
    ideal = list(ideal_output_states(circuit, n_qubits, basis_indices=basis_indices))
    noisy = list(noisy_output_density_matrices(circuit, noise_model, n_qubits, basis_indices=basis_indices))
    f_sum = 0.0
    for (k, sv_ideal), (_, rho_noisy) in zip(ideal, noisy):
        f_sum += state_fidelity(sv_ideal, rho_noisy)
    denom = dim if basis_indices is None else len(basis_indices)
    f_avg = f_sum / denom
    f_proc = ((dim + 1) * f_avg - 1) / dim
    return f_avg, f_proc


def state_fidelity_for_all_zero_input(
    circuit: QuantumCircuit,
    noise_model: NoiseModel,
    n_qubits: int,
) -> float:
    """Compute state fidelity between ideal and noisy output for input |0...0>."""
    (k_ideal, sv_ideal) = next(ideal_output_states(circuit, n_qubits, basis_indices=[0]))
    assert k_ideal == 0
    (k_noisy, rho_noisy) = next(noisy_output_density_matrices(circuit, noise_model, n_qubits, basis_indices=[0]))
    assert k_noisy == 0
    return float(state_fidelity(sv_ideal, rho_noisy))


def process_fidelity_via_choi(
    circuit: QuantumCircuit,
    noise_model: NoiseModel,
    n_qubits: int,
) -> float:
    """
    Compute process fidelity by building the Choi representation of both
    channels and using qiskit.quantum_info.process_fidelity.
    More expensive (d^2 runs for noisy channel) but exact.
    """
    from qiskit.quantum_info import Choi, Operator

    qc = circuit.remove_final_measurements(inplace=False)
    dim = 2**n_qubits
    # Ideal channel: unitary from circuit
    op = Operator(qc)
    ideal_choi = Choi(op)

    # Noisy channel: run for each |i⟩⟨j| to build Choi
    # Choi J has blocks J_ij = Φ(|i⟩⟨j|). We need to simulate Φ on |i⟩⟨j|.
    # Φ(|i⟩⟨j|) can be obtained from linear combinations of outputs of
    # Φ on (|i⟩+|j⟩)(⟨i|+⟨j|), (|i⟩-|j⟩)(⟨i|-⟨j|), (|i⟩+i|j⟩)(⟨i|-i⟨j|), (|i⟩-i|j⟩)(⟨i|+i⟨j|).
    sim_dm = AerSimulator(noise_model=noise_model, method="density_matrix")

    def run_noisy(init_statevector: np.ndarray):
        """Run circuit with given initial statevector, return output density matrix (dim, dim)."""
        full = QuantumCircuit(n_qubits)
        full.initialize(init_statevector, list(range(n_qubits)))
        full = full.compose(qc, list(range(n_qubits)))
        full.save_density_matrix()
        job = sim_dm.run(full, shots=1)
        r = job.result()
        return _density_matrix_from_result(r, 0, dim)

    # Build Choi of noisy channel: J = sum_{i,j} |i><j| ⊗ Φ(|i><j|).
    # We compute Φ(|k><k|) for each k (d runs), then for i!=j we need linear combos.
    # Full Choi requires d^2 blocks. For each (i,j), Φ(|i><j|) = (1/4)[ Phi(++) - Phi(--) - i Phi(+i-) + i Phi(-i+) ] etc.
    choi_blocks = np.zeros((dim, dim, dim, dim), dtype=complex)
    for k in range(dim):
        e_k = np.zeros(dim, dtype=complex)
        e_k[k] = 1.0
        choi_blocks[k, k, :, :] = run_noisy(e_k)

    for i in range(dim):
        for j in range(i + 1, dim):
            e_i = np.zeros(dim, dtype=complex)
            e_i[i] = 1.0
            e_j = np.zeros(dim, dtype=complex)
            e_j[j] = 1.0
            # Φ(|i><j|) = (1/4)[ Φ((|i>+|j>)(<i|+<j|)) - Φ((|i>-|j>)(<i|-<j|)) - i Φ((|i>+i|j>)(<i|-i<j|)) + i Φ((|i>-i|j>)(<i|+i<j|)) ]
            choi_blocks[i, j, :, :] = 0.0
            for coef, vec in [
                (0.25, (e_i + e_j) / np.sqrt(2)),
                (-0.25, (e_i - e_j) / np.sqrt(2)),
                (-0.25j, (e_i + 1j * e_j) / np.sqrt(2)),
                (0.25j, (e_i - 1j * e_j) / np.sqrt(2)),
            ]:
                out = run_noisy(vec)
                choi_blocks[i, j, :, :] += coef * out
            choi_blocks[j, i, :, :] = np.conj(choi_blocks[i, j, :, :].T)

    # Choi matrix: J = sum_{i,j} |i><j| ⊗ Φ(|i><j|)  shape (d*d, d*d)
    J_noisy = np.zeros((dim * dim, dim * dim), dtype=complex)
    for i in range(dim):
        for j in range(dim):
            J_noisy[i * dim : (i + 1) * dim, j * dim : (j + 1) * dim] = choi_blocks[i, j, :, :]
    noisy_choi = Choi(J_noisy)
    return float(process_fidelity(noisy_choi, ideal_choi))


def main():
    parser = argparse.ArgumentParser(
        description="Compare ideal vs noisy simulation and output process fidelity",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "circuit",
        type=str,
        help="Path to circuit file (.qasm)",
    )
    parser.add_argument(
        "--backend",
        type=str,
        default="fake_manila",
        help="Backend name: 'fake_manila', 'fake_oslo', or real 'ibmq_manila' etc.",
    )
    parser.add_argument(
        "--use-real-backend",
        action="store_true",
        help="Use real IBM Quantum backend (requires IBMQ_API_KEY)",
    )
    parser.add_argument(
        "--exact-choi",
        action="store_true",
        help="Compute process fidelity via full Choi (slower, d^2 runs)",
    )
    parser.add_argument(
        "--preserve-backend-qubits",
        action="store_true",
        help="Remap noise so backend qubit fidelities match the circuit's physical qubit indices. "
        "Requires backend to have at least as many qubits as the circuit's max qubit index; otherwise a warning is printed and backend noise on 0..n-1 is used.",
    )
    parser.add_argument(
        "--input-all-zero",
        action="store_true",
        help="Only simulate the |0...0> input state (no averaging over basis inputs).",
    )
    args = parser.parse_args()

    circuit_path = Path(args.circuit)
    if not circuit_path.exists():
        print(f"Error: circuit file not found: {circuit_path}", file=sys.stderr)
        return 1

    try:
        circuit = load_circuit(str(circuit_path))
    except Exception as e:
        print(f"Error loading circuit: {e}", file=sys.stderr)
        return 1

    n_qubits = circuit.num_qubits
    if n_qubits == 0:
        print("Error: circuit has no qubits", file=sys.stderr)
        return 1

    # Reduce to active qubits only (those that appear in at least one gate) to avoid
    # simulating 2^(total qubits) when many qubits are idle.
    active_indices = get_active_qubit_indices(circuit)
    reduced = len(active_indices) < n_qubits
    if reduced:
        circuit = circuit_to_active_subcircuit(circuit, active_indices)
        n_qubits = len(active_indices)
        print(f"Reduced to {n_qubits} active qubits for simulation (idle qubits skipped).")
    if n_qubits > 24:
        print(
            f"Error: {n_qubits} active qubits exceeds simulation limit (24). "
            "State dimension 2^n would be too large.",
            file=sys.stderr,
        )
        return 1

    # Get backend and noise model
    if args.use_real_backend and HAS_GET_BACKEND:
        backend = get_real_backend(args.backend, verbose=True)
    else:
        if HAS_GET_BACKEND:
            backend = get_fake_backend(args.backend, verbose=True)
        elif HAS_FAKE_PROVIDER:
            parts = args.backend.replace("fake_", "").replace("_", " ").title().replace(" ", "")
            try:
                backend = getattr(fake_provider, f"Fake{parts}V2")()
            except AttributeError:
                backend = getattr(fake_provider, f"Fake{parts}")()
        else:
            print("Error: need qiskit_ibm_runtime (fake provider) or get_backend.py", file=sys.stderr)
            return 1

    if backend is None:
        print("Error: could not get backend", file=sys.stderr)
        return 1

    try:
        noise_model = get_noise_model_from_backend(backend)
    except Exception as e:
        print(f"Error building noise model: {e}", file=sys.stderr)
        return 1

    # Optionally preserve backend qubit mapping (only when the backend has those qubit indices).
    backend_num_qubits = getattr(
        getattr(backend, "configuration", lambda: None)(), "n_qubits", None
    ) or getattr(backend, "num_qubits", None)
    if reduced and args.preserve_backend_qubits:
        if backend_num_qubits is not None and max(active_indices) >= backend_num_qubits:
            print(
                f"Warning: circuit uses qubit indices up to {max(active_indices)} but backend "
                f"({backend.name}) has only {backend_num_qubits} qubits. "
                "Cannot preserve backend qubits; using backend noise on logical 0..n-1.",
                file=sys.stderr,
            )
        else:
            noise_model = noise_model_for_active_qubits(noise_model, active_indices)
            print(f"Preserving backend qubits: simulating with noise for backend qubits {active_indices} (logical 0..{n_qubits - 1}).")
    elif args.preserve_backend_qubits and not reduced:
        print("Preserving backend qubits: circuit uses contiguous 0..n-1; backend noise applied to those qubits.")

    print(f"Circuit: {circuit_path} ({n_qubits} qubits simulated)")
    print(f"Backend: {backend.name}")
    print()

    if args.input_all_zero:
        f0 = state_fidelity_for_all_zero_input(circuit, noise_model, n_qubits)
        print(f"State fidelity for |0...0> input: {f0:.6f}")
        if args.exact_choi and n_qubits <= 3:
            try:
                f_proc = process_fidelity_via_choi(circuit, noise_model, n_qubits)
                print(f"Process fidelity (exact Choi):   {f_proc:.6f}")
            except Exception as e:
                print(f"Exact Choi failed: {e}", file=sys.stderr)
        return 0

    if args.exact_choi and n_qubits <= 3:
        try:
            f_proc = process_fidelity_via_choi(circuit, noise_model, n_qubits)
            print(f"Process fidelity (exact Choi): {f_proc:.6f}")
        except Exception as e:
            print(f"Exact Choi failed: {e}", file=sys.stderr)
            args.exact_choi = False
    if not args.exact_choi or n_qubits > 3:
        f_avg, f_proc = average_state_fidelity_and_process_fidelity(circuit, noise_model, n_qubits)
        print(f"Average state fidelity (comp. basis): {f_avg:.6f}")
        print(f"Process fidelity:                    {f_proc:.6f}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
