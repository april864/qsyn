#!/usr/bin/env python3
"""
Simulate a mapped OpenQASM 2 circuit with Qiskit Aer using the ``superop`` method.

The circuit is loaded as-is (no layout, routing, or transpilation). For scalability,
only qubits that appear in at least one gate are kept; idle register lines are dropped
and backend noise is restricted to that subdevice (physical indices preserved, then
remapped to logical 0..n-1).

With ``--backend``, noise is taken from an IBM Quantum device (``IBMQ_API_KEY`` in
``.env`` via ``load_dotenv``) or a ``fake_*`` backend for local testing.

With ``--ibmq-calibration``, noise comes from JSON written by ``qsyn device write --ibmq``
(typically after ``fham treespile --logical-index``). The circuit must use logical qubit
indices 0..n-1 matching that export.

Usage:
  python aer_superop_qasm.py circuit.qasm
  python aer_superop_qasm.py circuit.qasm -o channel_superop.npy
  python aer_superop_qasm.py circuit.qasm --backend ibm_fez
  python aer_superop_qasm.py circuit.qasm --backend fake_manila -o noisy_superop.npy
  python aer_superop_qasm.py circuit.qasm --ibmq-calibration wip/torino_sub.json
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
from qiskit import QuantumCircuit, qasm2
from qiskit.circuit.library import PhaseGate, SXGate, SXdgGate, UGate
from qiskit_aer import AerSimulator
from qiskit_aer.library import save_superop
from qiskit_aer.noise import NoiseModel

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from get_backend import get_fake_backend, get_real_backend
from ibmq_sliced_backend import load_calibration, load_sliced_backend, noise_model_from_calibration
from simulate_ideal_vs_noisy_fidelity import (
    circuit_to_active_subcircuit,
    get_active_qubit_indices,
    get_noise_model_from_backend,
    noise_model_for_active_qubits,
)


def resolve_noise_backend(
    backend_name: str | None,
    calibration_path: str | Path | None,
):
    """Return (backend, noise_model, parent_physical_qubits | None)."""
    if backend_name is not None and calibration_path is not None:
        raise ValueError("use only one of --backend and --ibmq-calibration")

    if calibration_path is None:
        backend = resolve_backend(backend_name)
        if backend is None:
            raise RuntimeError(f"could not load backend {backend_name!r}")
        return backend, get_noise_model_from_backend(backend), None

    bundle = load_calibration(calibration_path)
    backend = load_sliced_backend(calibration_path)
    return backend, noise_model_from_calibration(calibration_path), bundle.physical_qubits

# Superoperator matrix has shape (4^n, 4^n); memory grows as 16^n complex numbers.
DEFAULT_MAX_QUBITS = 6


def load_circuit(path: str) -> QuantumCircuit:
    """Load a circuit from a .qasm file (IBM / nativized OpenQASM 2)."""
    custom = [
        qasm2.CustomInstruction("p", 1, 1, PhaseGate, builtin=True),
        qasm2.CustomInstruction("sx", 0, 1, SXGate, builtin=True),
        qasm2.CustomInstruction("sxdg", 0, 1, SXdgGate, builtin=True),
        qasm2.CustomInstruction("u", 3, 1, UGate, builtin=True),
    ]
    return qasm2.load(path, custom_instructions=custom)


def backend_num_qubits(backend) -> int | None:
    config = getattr(backend, "configuration", lambda: None)()
    if config is not None:
        return getattr(config, "n_qubits", None)
    return getattr(backend, "num_qubits", None)


def resolve_backend(name: str):
    """Real IBM backend (IBMQ_API_KEY via dotenv) or ``fake_*`` local device."""
    if name.startswith("fake_"):
        return get_fake_backend(name, verbose=True)
    return get_real_backend(name, verbose=True)


def prepare_subdevice(
    circuit: QuantumCircuit,
    backend_name: str | None,
    calibration_path: str | Path | None = None,
) -> tuple[QuantumCircuit, list[int], int, NoiseModel | None, object | None, list[int] | None]:
    """
    Reduce to active qubits and build a noise model on that subdevice only.

    With ``backend_name``, backend physical indices in the QASM are remapped to
    logical 0..n-1. With ``calibration_path``, the QASM must already use logical
    indices 0..n-1 from a qsyn sliced export.
    """
    active_indices = get_active_qubit_indices(circuit)
    if not active_indices:
        raise ValueError("circuit has no gate operations on any qubit")

    n_register = circuit.num_qubits
    if len(active_indices) < n_register:
        circuit = circuit_to_active_subcircuit(circuit, active_indices)

    if backend_name is None and calibration_path is None:
        return circuit, active_indices, n_register, None, None, None

    if backend_name is not None and calibration_path is not None:
        raise ValueError("use only one of --backend and --ibmq-calibration")

    if calibration_path is not None:
        bundle = load_calibration(calibration_path)
        if max(active_indices) >= bundle.num_qubits:
            raise ValueError(
                f"circuit uses logical qubit indices up to {max(active_indices)}, "
                f"but calibration export has only {bundle.num_qubits} qubits"
            )

        backend, full_noise, parent_physical = resolve_noise_backend(None, calibration_path)
        sub_noise = noise_model_for_active_qubits(full_noise, active_indices)
        return (
            circuit,
            active_indices,
            n_register,
            sub_noise,
            backend,
            parent_physical,
        )

    backend, full_noise, _ = resolve_noise_backend(backend_name, None)
    n_backend = backend_num_qubits(backend)
    if n_backend is not None and max(active_indices) >= n_backend:
        raise ValueError(
            f"circuit uses backend qubit indices up to {max(active_indices)}, "
            f"but {backend.name} has only {n_backend} qubits"
        )

    sub_noise = noise_model_for_active_qubits(full_noise, active_indices)
    return circuit, active_indices, n_register, sub_noise, backend, None


def simulate_superop(
    circuit: QuantumCircuit,
    label: str = "superop",
    noise_model: NoiseModel | None = None,
) -> np.ndarray:
    """Run ``circuit`` on Aer (superop) and return the channel as a (4^n, 4^n) array."""
    qc = circuit.remove_final_measurements(inplace=False)
    save_superop(qc, label=label)

    sim = AerSimulator(method="superop", noise_model=noise_model)
    result = sim.run(qc, shots=1).result()
    data = result.data(0)
    if label not in data:
        raise KeyError(
            f"Result missing key {label!r}; available keys: {list(data.keys())}"
        )
    return np.asarray(data[label], dtype=complex)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Simulate a mapped .qasm circuit with Qiskit Aer (superop method)",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "circuit",
        type=str,
        help="Path to input circuit (.qasm), already mapped to backend qubit indices",
    )
    parser.add_argument(
        "--backend",
        type=str,
        default=None,
        help="IBM Quantum backend name (uses IBMQ_API_KEY from .env) or fake_* device",
    )
    parser.add_argument(
        "--ibmq-calibration",
        type=str,
        default=None,
        metavar="PATH",
        help="qsyn device write --ibmq JSON file (logical qubits 0..n-1; exclusive with --backend)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default=None,
        help="Write superoperator matrix to this .npy file",
    )
    parser.add_argument(
        "--label",
        type=str,
        default="superop",
        help="Label for save_superop / result key (default: superop)",
    )
    parser.add_argument(
        "--max-qubits",
        type=int,
        default=DEFAULT_MAX_QUBITS,
        help=f"Refuse simulation above this many active qubits (default: {DEFAULT_MAX_QUBITS})",
    )
    args = parser.parse_args()

    circuit_path = Path(args.circuit)
    if not circuit_path.exists():
        print(f"Error: circuit file not found: {circuit_path}", file=sys.stderr)
        return 1

    try:
        circuit = load_circuit(str(circuit_path))
    except Exception as exc:
        print(f"Error loading circuit: {exc}", file=sys.stderr)
        return 1

    if circuit.num_qubits == 0:
        print("Error: circuit has no qubits", file=sys.stderr)
        return 1

    try:
        circuit, active_indices, n_register, noise_model, backend, parent_physical = (
            prepare_subdevice(circuit, args.backend, args.ibmq_calibration)
        )
    except Exception as exc:
        print(f"Error preparing subdevice: {exc}", file=sys.stderr)
        return 1

    n_qubits = circuit.num_qubits
    if n_qubits > args.max_qubits:
        dim = 4**n_qubits
        print(
            f"Error: {n_qubits} active qubits implies a {dim}×{dim} superoperator "
            f"(>{args.max_qubits} qubits limit). Raise --max-qubits if intended.",
            file=sys.stderr,
        )
        return 1

    try:
        superop = simulate_superop(circuit, label=args.label, noise_model=noise_model)
    except Exception as exc:
        print(f"Error during Aer superop simulation: {exc}", file=sys.stderr)
        return 1

    print(f"Circuit: {circuit_path}")
    if parent_physical is not None:
        parent_by_logical = [
            parent_physical[i] if i < len(parent_physical) else "?"
            for i in active_indices
        ]
        print(
            f"Subdevice: logical {active_indices} -> parent physical {parent_by_logical}"
        )
    else:
        print(
            f"Subdevice: backend qubits {active_indices} -> logical 0..{n_qubits - 1}"
        )
    if n_register > n_qubits:
        print(f"  ({n_register}-line QASM register; {n_register - n_qubits} idle line(s) dropped)")
    if backend is not None:
        source = "qsyn IBM JSON export" if parent_physical is not None else "live backend"
        print(f"Backend: {backend.name} ({source}, noisy channel)")
    else:
        print("Backend: none (ideal channel)")
    print(f"Superoperator shape: {superop.shape}")

    if args.output:
        out_path = Path(args.output)
        np.save(out_path, superop)
        print(f"Wrote {out_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
