## USE: Evaluates termwise contribution to overall circuit fidelity, as well as termwise proxy fidelity
## Change fham_cmd.cpp eval func to use evaluate_proxy_termwise_fidelity

import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import sys
import os
import numpy as np

csv_file = "proxy_eval_termwise_fidelity.csv"
target_samples = 50
backend = "fake_torino"
benchmark = "benchmark/fham/electron-4.fham"

if os.path.exists(csv_file):
    os.remove(csv_file)
with open(csv_file, "w") as f:
    f.write("TermIndex,TermLogProxyInfidelity,ActualLogInfidelity\n")

cli_commands = f"device fetch -f {backend}; fham read {benchmark}; fham eval-proxy -s {target_samples} -o {csv_file}"

print(f"Compiling {target_samples} trees for {benchmark} on {backend}...")

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

print("\nEvaluation complete. Graphing data...")

# Graph
try:
    df = pd.read_csv(csv_file)
 
    plt.figure(figsize=(10, 6))
    
    # alpha: transparancy; s: size
    plt.scatter(df['TermLogProxyInfidelity'], df['ActualLogInfidelity'], alpha=0.1, color='blue', s=40)
    
    plt.title(f"Termwise Log Infidelity ({benchmark}, {backend})", fontsize=14, fontweight='bold')
    plt.xlabel("Individual Term Proxy Infidelity (Log Scale)", fontsize=12)
    plt.ylabel("Actual Term Proxy Infidelity (Log Scale)", fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)

    if df['TermLogProxyInfidelity'].nunique() > 1:
        z = np.polyfit(df['TermLogProxyInfidelity'], df['ActualLogInfidelity'], 1)
        p = np.poly1d(z)
        # plt.plot(df['TermProxyCost'], p(df['TermProxyCost']), color='red', linestyle='-', linewidth=2, label="Linear Trend")
        plt.legend()

    plt.tight_layout()
    plt.show()

except Exception as e:
    print(f"\nFAILED TO GRAPH DATA: {e}")