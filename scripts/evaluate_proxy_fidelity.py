## Compares proxy fidelity with final circuit fidelity

import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import sys
import os
import numpy as np

from qiskit import QuantumCircuit
from qiskit_ibm_runtime.fake_provider import FakeFez
import nativize_and_optimize_qasm as opt

csv_file = "proxy_evaluation_fidelity.csv"
target_samples = 500
backend_name = "fake_fez"
benchmark = "benchmark/fham/fermi-hubbard-18.fham"

if os.path.exists(csv_file):
    os.remove(csv_file)
    
with open(csv_file, "w") as f:
    f.write("TreeIndex,TotalProxyCost\n")

cli_commands = f"device fetch -f {backend_name}; fham read {benchmark}; fham eval-proxy -s {target_samples} -o {csv_file}"

print(f"Compiling {target_samples} circuits via C++ engine...")
process = subprocess.Popen(
    ["./qsyn", "-c", cli_commands], 
    stdout=subprocess.PIPE, 
    stderr=subprocess.STDOUT, 
    text=True
)

for line in process.stdout:
    sys.stdout.write(line)
    sys.stdout.flush()
        
process.wait()

print("\nC++ Compilation complete. Optimizing and evaluating via Qiskit...")

backend = FakeFez()
target = backend.target

actual_fidelities = []
valid_indices = []

try:
    df_proxy = pd.read_csv(csv_file)
    
    for index, row in df_proxy.iterrows():
        tree_idx = int(row['TreeIndex'])
        qasm_filename = f"sample_{tree_idx}.qasm"
        
        if not os.path.exists(qasm_filename):
            continue
            
        try:
            qc = QuantumCircuit.from_qasm_file(qasm_filename)
            
            qc_native = opt.nativize_gate_set(qc, backend)
            qc_opt = opt.post_mapping_optimize_preserve_connectivity(qc_native, backend)
            
            # ESP
            total_log_infidelity = 0.0
            for instruction in qc_opt.data:
                gate_name = instruction.operation.name
                
                if gate_name in ['barrier', 'measure', 'delay']:
                    continue
                    
                qubits = tuple(qc_opt.find_bit(q).index for q in instruction.qubits)
                
                try:
                    error = target[gate_name][qubits].error
                    if error is not None and error < 1.0:
                        total_log_infidelity += -np.log(1.0 - error)
                except KeyError:
                    pass
                    
            actual_fidelities.append(total_log_infidelity)
            valid_indices.append(tree_idx)
            
        except Exception as e:
            print(f"Failed to process {qasm_filename}: {e}")

    print("Evaluation complete. Graphing data...")

    # Graph
    df_filtered = df_proxy[df_proxy['TreeIndex'].isin(valid_indices)].copy()
    df_filtered['TotalActualCost'] = actual_fidelities
    df_filtered = df_filtered.replace([np.inf, -np.inf], np.nan).dropna()
 
    plt.figure(figsize=(10, 6))
    plt.scatter(df_filtered['TotalProxyCost'], df_filtered['TotalActualCost'], alpha=0.6, color='purple', s=50, edgecolors='black')
    
    plt.title(f"Circuit Fidelity ({benchmark}, {backend_name})", fontsize=14, fontweight='bold')
    plt.xlabel("Total Predicted Circuit Cost (Infidelity Proxy)", fontsize=12)
    plt.ylabel("Actual Cost (Post-Qiskit Infidelity)", fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)

    if df_filtered['TotalProxyCost'].nunique() > 1:
        z = np.polyfit(df_filtered['TotalProxyCost'], df_filtered['TotalActualCost'], 1)
        p = np.poly1d(z)

        correlation_matrix = np.corrcoef(df_filtered['TotalProxyCost'], df_filtered['TotalActualCost'])
        r_val = correlation_matrix[0, 1]
        r_squared = r_val ** 2

        plt.plot(df_filtered['TotalProxyCost'], p(df_filtered['TotalProxyCost']), color='red', linestyle='-', linewidth=2, label=f"Trendline (y = {z[0]:.2f}x + {z[1]:.2f})\n$R^2$={r_squared:.4f}")
        plt.legend()

    plt.tight_layout()
    plt.show()

except Exception as e:
    print(f"\nFAILED TO GRAPH DATA: {e}")