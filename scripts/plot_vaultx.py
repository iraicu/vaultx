import pandas as pd
import matplotlib.pyplot as plt
import os
import matplotlib.colors as mcolors

# Constants
input_folder = "data/"
output_folder = "plots/"
os.makedirs(output_folder, exist_ok=True)

drive_types = ["hdd", "sata", "nvme"]
drive_colors = {
    "hdd": "#ea9999ff",  # Light Red
    "sata": "#b6d7a8ff",  # Light Green
    "nvme": "#a4c2f4ff",  # Light Blue
}

latency_colors = {
    "hdd": "#e06666ff",   # Red
    "sata": "#6aa84fff",   # Green
    "nvme": "#3c78d8ff",   # Blue
}

bar_width = 0.2

# Load all CSV files
csv_files = [f for f in os.listdir(input_folder) if f.endswith(".csv")]

# Extract machine names
machines = set()
for f in csv_files:
    parts = f.split("-")
    if len(parts) >= 2:
        machines.add(parts[1])

# For each machine, plot its data
for machine in machines:
    fig, ax1 = plt.subplots(figsize=(10, 6))
    ax2 = ax1.twinx()

    # Filter CSV files for this machine
    machine_files = [f for f in csv_files if f"-{machine}-" in f]

    all_data = []
    for f in machine_files:
        path = os.path.join(input_folder, f)
        df = pd.read_csv(path)
        dtype = f.split("-")[-1].replace(".csv", "")
        df["DRIVE_TYPE"] = dtype
        all_data.append(df)

    machine_df = pd.concat(all_data, ignore_index=True)
    machine_df = machine_df[pd.to_numeric(machine_df["K"], errors="coerce").notna()]
    machine_df["K"] = machine_df["K"].astype(int)

    k_values = sorted(machine_df["K"].unique())

    # Adjust drive_types for the current machine
    available_drive_types = machine_df["DRIVE_TYPE"].unique()
    drive_types_to_plot = [dt for dt in drive_types if dt in available_drive_types]

    latency_legend_added = False

    num_drives = len(drive_types_to_plot)
    group_width = num_drives * bar_width

    for i, dtype in enumerate(drive_types_to_plot):
        throughput_y_vals = []
        throughput_x_vals = []
        latency_y_vals = []
        latency_x_vals = []

        for k in k_values:
            subset = machine_df[(machine_df["DRIVE_TYPE"] == dtype) & (machine_df["K"] == k)]
            if not subset.empty:
                throughput = subset["THROUGHPUT(MH/S)"].values[0]
                latency = subset["TOTAL_TIME"].values[0]

                k_pos = k_values.index(k)
                center_x = k_pos  # This will be the center of the group
                offset = (i - (num_drives - 1) / 2) * bar_width

                throughput_x_vals.append(center_x + offset)
                throughput_y_vals.append(throughput)

                latency_x_vals.append(center_x)
                latency_y_vals.append(latency)

        if throughput_x_vals and throughput_y_vals:
            ax1.bar(throughput_x_vals, throughput_y_vals, bar_width, label=f"{dtype}",
                    color=drive_colors[dtype])

        if latency_x_vals and latency_y_vals:
            if not latency_legend_added:
                ax2.plot(latency_x_vals, latency_y_vals, marker="o", label="Latency",
                         color="black", linestyle="--")
                latency_legend_added = True
            ax2.plot(latency_x_vals, latency_y_vals, marker="o", color=latency_colors[dtype], linestyle="--")

    ax1.set_xlabel("K value")
    ax1.set_ylabel("Throughput (MH/s)")
    ax2.set_ylabel("Latency (s)")

    # Set x-ticks at center of each K group
    xtick_positions = [k_values.index(k) for k in k_values]
    ax1.set_xticks(xtick_positions)
    ax1.set_xticklabels([str(k) for k in k_values])

    fig.suptitle(f"VAULTX Performance on {machine}")
    fig.legend(loc="upper left", bbox_to_anchor=(0.1, 0.9))
    fig.tight_layout()

    plt.savefig(os.path.join(output_folder, f"vaultx_{machine}.svg"))
    plt.close()
