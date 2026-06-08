import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import sys
import os
import numpy as np

csv_file = "proxy_evaluation.csv"
target_samples = 100

if os.path.exists(csv_file):
    os.remove(csv_file)
with open(csv_file, "w") as f:
    f.write("TermProxyCost,IsolatedCNOTs,CNOTDiff\n")

cli_commands = f"device fetch -f fake_oslo; fham read benchmark/fham/electron-4.fham; fham eval-proxy -s {target_samples} -o {csv_file}"

print("Compiling circuits and extracting term data in C++...")

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

print("\nEvaluation complete. Graphing...")

try:
    df = pd.read_csv(csv_file)
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6), sharey=True)

    # CNOTs from each term
    ax1.scatter(df['TermProxyCost'], df['IsolatedCNOTs'], alpha=0.05, color='#3498db', s=40)
    if df['TermProxyCost'].nunique() > 1:
        z1 = np.polyfit(df['TermProxyCost'], df['IsolatedCNOTs'], 1)
        ax1.plot(df['TermProxyCost'], np.poly1d(z1)(df['TermProxyCost']), color='#2980b9', lw=2)
        
    ax1.set_title(f"2-Qubit Gates from Each Term", fontsize=12, fontweight='bold')
    ax1.set_xlabel("Term Proxy Cost (Hops)", fontsize=11)
    ax1.set_ylabel("Physical 2Q Gates Generated", fontsize=11)
    ax1.grid(True, linestyle='--', alpha=0.7)

    # Termwise contribution
    ax2.scatter(df['TermProxyCost'], df['CNOTDiff'], alpha=0.05, color='#e74c3c', s=40)
    if df['TermProxyCost'].nunique() > 1:
        z2 = np.polyfit(df['TermProxyCost'], df['CNOTDiff'], 1)
        ax2.plot(df['TermProxyCost'], np.poly1d(z2)(df['TermProxyCost']), color='#c0392b', lw=2)
        
    ax2.set_title(f"Termwise 2Q GateContribution", fontsize=12, fontweight='bold')
    ax2.set_xlabel("Term Proxy Cost (Hops)", fontsize=11)
    ax2.grid(True, linestyle='--', alpha=0.7)

    plt.suptitle("Term-Level Contributions", fontsize=16, fontweight='bold')
    plt.tight_layout()
    plt.show()

except Exception as e:
    print(f"\nFAILED TO GRAPH DATA: {e}")