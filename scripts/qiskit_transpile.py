'''
calls qiskit to transpile a qcir circuit to a qiskit circuit
supports selecting backend and optimization levels
uses fake provider for transpilation (no IBM Quantum credentials required)
'''

import argparse
from qiskit import QuantumCircuit, transpile
from qiskit import qasm2
from typing import Optional
import qiskit_ibm_runtime.fake_provider as fake_provider

def transpile_qcir_to_qiskit(input_file: str, output_file: str, backend: Optional[str], optimization_level: int) -> bool:
    '''
    returns true if the transpilation is successful, false otherwise
    '''
    try:
        qc = QuantumCircuit.from_qasm_file(input_file)
        
        # Use fake provider for transpilation if backend is specified
        backend_obj = None
        if backend:
            try:
                # Convert backend name to fake backend class name
                # e.g., "fake_manila" -> "FakeManilaV2" or "FakeManila"
                backend_parts = backend.replace("fake_", "").replace("_", " ").title().replace(" ", "")
                
                # Try V2 version first, then fall back to V1
                backend_class_name = f"Fake{backend_parts}V2"
                try:
                    backend_class = getattr(fake_provider, backend_class_name)
                    backend_obj = backend_class()
                except AttributeError:
                    # Try without V2 suffix
                    backend_class_name = f"Fake{backend_parts}"
                    backend_class = getattr(fake_provider, backend_class_name)
                    backend_obj = backend_class()
            except (AttributeError, Exception) as e:
                print(f"Error: Failed to get backend {backend}.")
                print(f"Please provide a valid fake backend name (e.g., fake_manila, fake_oslo)")
                print("\nAvailable fake backends can be found at:")
                print("https://docs.quantum.ibm.com/api/qiskit-ibm-runtime/fake_provider")
                return False
        
        # Transpile the circuit
        # If no backend is specified, transpile without qubit routing
        transpiled_qc = transpile(qc, backend=backend_obj, optimization_level=optimization_level)
        
        # Write the transpiled circuit to file
        qasm2.dump(transpiled_qc, output_file)
        return True
    except FileNotFoundError as e:
        print(f"Error: Input file not found: {e}")
        return False
    except Exception as e:
        print(f"Error during transpilation: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description='Transpile a qcir circuit to a qiskit circuit',
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        "-input",
        type=str,
        required=True,
        help='the input .qasm file'
    )
    parser.add_argument(
        "-output",
        type=str,
        required=True,
        help='the output .qasm file'
    )
    parser.add_argument(
        "-backend",
        type=str,
        default=None,
        help='the backend to use for transpilation (optional; if omitted, transpiles without qubit routing)'
    )
    parser.add_argument(
        "-optimization_level",
        type=int,
        required=True,
        help='the optimization level to use for transpilation'
    )
    args = parser.parse_args()
    
    return transpile_qcir_to_qiskit(args.input, args.output, args.backend, args.optimization_level)

if __name__ == "__main__":
    main()