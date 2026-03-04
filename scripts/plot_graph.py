import matplotlib.pyplot as plt
import numpy as np
import os

# --- Setup Directory ---
# Create the directory if it doesn't exist
output_dir = './graphs'
if not os.path.exists(output_dir):
    os.makedirs(output_dir)

# --- Data Preparation ---
k_values = [27, 28, 29, 30, 31, 32]

# Data for In Memory (-t 40)
in_memory_hdd = [22.28, 39.37, 76.93, 145.57, 279.16, 561.67]
in_memory_ssd = [19.16, 41.08, 55.76, 110.11, 199.49, 395.19]
in_memory_nvme = [17.12, 29.80, 54.21, 102.86, 198.13, 406.03]

# Data for Out of Memory (2 rounds, -t 40)
oom_hdd = [37.90, 72.33, 130.87, 255.43, 462.74, 854.04]
oom_ssd = [35.47, 65.95, 94.37, 152.03, 279.67, 541.51]
oom_nvme = [26.97, 47.74, 81.69, 154.47, 285.58, 559.23]

# --- Plotting Configuration ---
x = np.arange(len(k_values))  # Label locations
width = 0.25  # Width of the bars

# Create two subplots side by side (1 row, 2 columns)
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

# --- Plot 1: In Memory ---
rects1 = ax1.bar(x - width, in_memory_hdd, width, label='HDD', color='#1f77b4')
rects2 = ax1.bar(x, in_memory_ssd, width, label='SSD', color='#ff7f0e')
rects3 = ax1.bar(x + width, in_memory_nvme, width, label='NVME', color='#2ca02c')

ax1.set_ylabel('Time (Seconds)')
ax1.set_title('IN MEMORY (-t 40)')
ax1.set_xticks(x)
ax1.set_xticklabels(k_values)
ax1.set_xlabel('K Value')
ax1.legend()
ax1.grid(axis='y', linestyle='--', alpha=0.7)

# --- Plot 2: Out of Memory ---
rects4 = ax2.bar(x - width, oom_hdd, width, label='HDD', color='#1f77b4')
rects5 = ax2.bar(x, oom_ssd, width, label='SSD', color='#ff7f0e')
rects6 = ax2.bar(x + width, oom_nvme, width, label='NVME', color='#2ca02c')

ax2.set_ylabel('Time (Seconds)')
ax2.set_title('OUT OF MEMORY (2 rounds, -t 40)')
ax2.set_xticks(x)
ax2.set_xticklabels(k_values)
ax2.set_xlabel('K Value')
ax2.legend()
ax2.grid(axis='y', linestyle='--', alpha=0.7)

# Adjust layout to prevent overlap
plt.tight_layout()

# Save the plot
save_path = os.path.join(output_dir, 'memory_performance_comparison.png')
plt.savefig(save_path)

print(f"Graph saved successfully to: {save_path}")
plt.show()