# import pandas as pd
# import matplotlib.pyplot as plt
# import os

# data_folder = "../data"

# for filename in os.listdir(data_folder):
#     data_path = os.path.join(data_folder, filename)
    
import os
import pandas as pd
import matplotlib.pyplot as plt

# Set data directory
data_dir = 'data'

# Filenames = machine names
file_names = [
    "8socket-ssd", "epycbox-hdd", "epycbox-nvme", "epycbox-ssd",
    "opi5-hdd", "opi-nvme", "rpi5-hdd", "rpi-nvme"
]

# Color palette for bar plots
colors = plt.cm.tab10.colors

# Load and plot
for file in file_names:
    path = os.path.join(data_dir, file)
    df = pd.read_csv(path)

    # Group by K and take average for each K
    grouped = df.groupby('K')[['THROUGHPUT(MB/S)', 'TOTAL_TIME']].mean().reset_index()

    fig, ax1 = plt.subplots(figsize=(10, 6))

    # Bar plot for Throughput
    ax1.bar(grouped['K'], grouped['THROUGHPUT(MB/S)'], color=colors[0], label='Throughput (MB/s)')
    ax1.set_xlabel('K')
    ax1.set_ylabel('Throughput (MB/s)', color=colors[0])
    ax1.tick_params(axis='y', labelcolor=colors[0])

    # Line plot for Latency (Total Time)
    ax2 = ax1.twinx()
    ax2.plot(grouped['K'], grouped['TOTAL_TIME'], color=colors[1], marker='o', label='Total Time (s)')
    ax2.set_ylabel('Total Time (s)', color=colors[1])
    ax2.tick_params(axis='y', labelcolor=colors[1])

    # Title and layout
    plt.title(f'Performance Metrics for {file}')
    fig.tight_layout()
    plt.grid(True)

    # Show plot
    plt.show()

    