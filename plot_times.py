import pandas as pd
import matplotlib.pyplot as plt

# 1. Define the data
data = [
    {"Machine": "Bladebit", "Time": 17.92, "Memory": "416GB", "Threads": 128},
    {"Machine": "ChiaPOS", "Time": 642.0, "Memory": "128GB", "Threads": 128},
    {"Machine": "MadMax", "Time": 50.1, "Memory": "12GB", "Threads": 128},
    {"Machine": "Eightsocket", "Time": 1.74, "Memory": "50GB", "Threads": 384},
    {"Machine": "Epycbox", "Time": 3.72, "Memory": "50GB", "Threads": 128},
    {"Machine": "GPUbox", "Time": 3.01, "Memory": "50GB", "Threads": 96},
    {"Machine": "NVMEbox", "Time": 2.82, "Memory": "50GB", "Threads": 64},
    {"Machine": "Torus", "Time": 6.28, "Memory": "50GB", "Threads": 32},
    {"Machine": "Athena", "Time": 5.76, "Memory": "50GB", "Threads": 48},
    {"Machine": "FPGANODE2", "Time": 11.15, "Memory": "25GB", "Threads": 16},
    {"Machine": "OPI5", "Time": 32.94, "Memory": "25GB", "Threads": 8},
    {"Machine": "thunderx2", "Time": 5.95, "Memory": "50GB", "Threads": 224},
    {"Machine": "thunderx1", "Time": 6.2, "Memory": "50GB", "Threads": 96},
]

df = pd.DataFrame(data)

# 2. Separate and sort the data
# aarch64 machines must be at the end (right)
aarch64_names = ["OPI5", "thunderx2", "thunderx1"]

# Group 1: Non-aarch64 sorted slowest to fastest (descending time)
df_other = df[~df['Machine'].isin(aarch64_names)].sort_values(by='Time', ascending=False)

# Group 2: aarch64 sorted slowest to fastest (descending time)
df_aarch = df[df['Machine'].isin(aarch64_names)].sort_values(by='Time', ascending=False)

# Combine them
df_final = pd.concat([df_other, df_aarch]).reset_index(drop=True)

# 3. Create the plot
plt.figure(figsize=(14, 8))
colors = ['skyblue' if m not in aarch64_names else 'salmon' for m in df_final['Machine']]
bars = plt.bar(df_final['Machine'], df_final['Time'], color=colors)

# Use log scale because of the high variance in times (642 vs 1.74)
plt.yscale('log')
plt.ylabel('Time in Minutes (Log Scale)')
plt.xlabel('Machine Name')
plt.title('Machine Benchmark Times (Slowest to Fastest, AArch64 Grouped at End)')
plt.xticks(rotation=45, ha='right')

# 4. Add labels for Threads and Memory
for i, bar in enumerate(bars):
    yval = bar.get_height()
    threads = df_final.iloc[i]['Threads']
    memory = df_final.iloc[i]['Memory']
    
    # Label format: Time \n (Threads, Memory)
    label = f"{yval}m\n{threads}T, {memory}"
    plt.text(bar.get_x() + bar.get_width()/2, yval, label, 
             va='bottom', ha='center', fontsize=9, fontweight='bold')

plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.tight_layout()
plt.savefig('machine_benchmarks.png')
plt.show()