import pandas as pd
# from scipy.stats import spearmanr
import matplotlib.pyplot as plt
import subprocess
import sys
import os

csv_file = "proxy_evaluation.csv"

if os.path.exists(csv_file):
    os.remove(csv_file)
with open(csv_file, "w") as f:
    f.write("ProxyCost,TrueCNOTs\n")

cli_commands = f"device fetch -f fake_oslo; fham read benchmark/fham/electron-4.fham; fham eval-proxy -s 20 -o {csv_file}"

target_samples = 100
current_samples = 0

print(f"Goal: Collect {target_samples} valid quantum circuits.")

while current_samples < target_samples:
    
    process = subprocess.Popen(
        ["./qsyn", "-c", cli_commands], 
        stdout=subprocess.PIPE, 
        stderr=subprocess.STDOUT, 
        text=True
    )
    
    for line in process.stdout:
        sys.stdout.write(line)
        sys.stdout.flush()
        
        # Terminate when this round of evaluate_proxy_cost() ends
        if "Evaluation saved" in line:
            process.terminate()
            break
            
    process.wait()
    
    # Check progress
    try:
        with open(csv_file, "r") as f:
            lines = f.readlines()
        current_samples = max(0, len(lines) - 1)
        print(f"\n---> PROGRESS: {current_samples} / {target_samples} valid trees saved.\n")
    except Exception as e:
        print(f"\nFATAL PYTHON ERROR: {e}")
        sys.exit(1)

# Graph results
try:
    df = pd.read_csv(csv_file)
    df = df.dropna().head(target_samples) # Trim exactly to 100

    # rho, p_value = spearmanr(df['ProxyCost'], df['TrueCNOTs'])
    # print(f"Spearman's Rank Correlation (ρ): {rho:.4f}")

    plt.figure(figsize=(10, 6))
    plt.scatter(df['ProxyCost'], df['TrueCNOTs'], alpha=0.5, color='#2c3e50', edgecolors='w', s=60)
    plt.title(f"Proxy Cost (number of two-qubit tree interactions) vs Actual Number of CNOTs", fontsize=14, fontweight='bold')
    plt.xlabel("Fast Tree Proxy Cost (Hops)", fontsize=12)
    plt.ylabel("True Physical CNOT Count", fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)

    import numpy as np
    if df['ProxyCost'].nunique() > 1:
        z = np.polyfit(df['ProxyCost'], df['TrueCNOTs'], 1)
        p = np.poly1d(z)
        plt.plot(df['ProxyCost'], p(df['ProxyCost']), color='#e74c3c', linestyle='-', linewidth=2, label="Linear Trend")
        plt.legend()

    plt.tight_layout()
    plt.show()

except Exception as e:
    print(f"\nFAILED TO GRAPH DATA: {e}")