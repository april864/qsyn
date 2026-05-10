import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
import nativize_and_optimize_qasm as nativizer
from get_backend import get_fake_backend

csv_file = "proxy_evaluation.csv"
target_samples = 100

if os.path.exists(csv_file):
    os.remove(csv_file)
with open(csv_file, "w") as f:
    f.write("Sample,ProxyCost\n")

for i in range(target_samples):
    if os.path.exists(f"sample_{i}.qasm"):
        os.remove(f"sample_{i}.qasm")

# qsyn commands
cli_commands = f"device fetch -f fake_oslo; fham read benchmark/fham/electron-6.fham; fham eval-proxy -s {target_samples} -o {csv_file}"

print(f"Compiling {target_samples} raw quantum circuits in C++...")

process = subprocess.Popen(
    ["./qsyn", "-c", cli_commands], 
    stdout=subprocess.PIPE, 
    stderr=subprocess.STDOUT, 
    text=True
)

for line in process.stdout:
    sys.stdout.write(line)
    sys.stdout.flush()
    if "Evaluation saved" in line:
        process.terminate()
        break
        
process.wait()

print("\nC++ Nativizing and optimizing circuits using Qiskit...")

# Read data
try:
    df = pd.read_csv(csv_file)
except Exception as e:
    print(f"FATAL PYTHON ERROR: {e}")
    sys.exit(1)

# Optimize and calculate depth (qiskit)
backend = get_fake_backend("fake_oslo")
true_depths = []
true_cnots = []

for i in range(target_samples):
    qasm_path = f"sample_{i}.qasm"
    
    circuit = nativizer.load_circuit(qasm_path)
    circuit = nativizer.nativize_gate_set(circuit, backend)
    circuit = nativizer.post_mapping_optimize_preserve_connectivity(circuit, backend)
    
    true_depths.append(circuit.depth())
    
    cnot_count = sum(1 for inst in circuit.data if len(inst.qubits) >= 2)
    true_cnots.append(cnot_count)
    
    os.remove(qasm_path)

df['CircuitDepth'] = true_depths
df['TrueCNOTs'] = true_cnots

print("Optimization complete. Plotting...\n")

# Graph
try:
    plt.figure(figsize=(10, 6))
    plt.scatter(df['ProxyCost'], df['CircuitDepth'], alpha=0.5, color='green', edgecolors='w', s=60)
    
    plt.title(f"Proxy Cost vs Hardware Optimized Circuit Depth", fontsize=14, fontweight='bold')
    plt.xlabel("Fast Tree Proxy Cost (Hops)", fontsize=12)
    plt.ylabel("Actual Circuit Depth", fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)

    import numpy as np
    if df['ProxyCost'].nunique() > 1:
        z = np.polyfit(df['ProxyCost'], df['CircuitDepth'], 1)
        p = np.poly1d(z)
        plt.plot(df['ProxyCost'], p(df['ProxyCost']), color='red', linestyle='-', linewidth=2, label="Linear Trend")
        plt.legend()

    plt.tight_layout()
    plt.show()

except Exception as e:
    print(f"\nFAILED TO GRAPH DATA: {e}")