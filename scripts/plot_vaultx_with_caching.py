import pandas as pd
import matplotlib.pyplot as plt
import os
import matplotlib.colors as mcolors

# Constants
input_folder = "data/"
output_folder = "figures/"
os.makedirs(output_folder, exist_ok=True)

drive_types = ["hdd", "sata", "nvme", "caching"] 
drive_colors = {
    "hdd": "#ea9999ff",  # Light Red
    "sata": "#b6d7a8ff",  # Light Green
    "nvme": "#a4c2f4ff",  # Light Blue
    "caching": "#ffe599ff"  # Light Yellow
}

# New colors for latency lines
latency_colors = {
    "hdd": "#e06666ff",   # Red
    "sata": "#6aa84fff",   # Green
    "nvme": "#3c78d8ff",   # Blue
    "caching": "#ffd966ff" # Yellow
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

    latency_legend_added = False  # Flag to ensure latency is added only once in the legend

    for i, dtype in enumerate(drive_types_to_plot):
        throughput_y_vals = []
        throughput_x_vals = []
        latency_y_vals = []
        latency_x_vals = []
        
        # For throughput and latency
        for k in k_values:
            subset = machine_df[(machine_df["DRIVE_TYPE"] == dtype) & (machine_df["K"] == k)]
            if not subset.empty:
                throughput = subset["THROUGHPUT(MH/S)"].values[0]
                latency = subset["TOTAL_TIME"].values[0]
                
                # Positioning for throughput bars
                x_throughput = k_values.index(k) + i * bar_width
                throughput_x_vals.append(x_throughput)
                throughput_y_vals.append(throughput)
                
                # Positioning for latency lines
                x_latency = k_values.index(k) + 1.5 * bar_width
                latency_x_vals.append(x_latency)
                latency_y_vals.append(latency)
        
        # Plot throughput bars (specified colors)
        if throughput_x_vals and throughput_y_vals:
            ax1.bar(throughput_x_vals, throughput_y_vals, bar_width, label=f"{dtype}", 
                    color=drive_colors[dtype])

        # Plot latency lines with specific colors for each drive type (only once for latency)
        if latency_x_vals and latency_y_vals:
            if not latency_legend_added:  # Check if latency has already been labeled
                ax2.plot(latency_x_vals, latency_y_vals, marker="o", label="Latency", 
                         color="black", linestyle="--")  # Black line for latency in the legend
                latency_legend_added = True  # Set the flag to True after adding the latency label
            # Plot the latency with the specific drive color for each
            ax2.plot(latency_x_vals, latency_y_vals, marker="o", color=latency_colors[dtype], linestyle="--")

    ax1.set_xlabel("K value")
    ax1.set_ylabel("Throughput (MH/s)")
    ax2.set_ylabel("Latency (s)")

    xtick_positions = [i + 1.5 * bar_width for i in range(len(k_values))]
    ax1.set_xticks(xtick_positions)
    ax1.set_xticklabels([str(k) for k in k_values])

    fig.suptitle(f"VAULTX Performance on {machine}")
    fig.legend(loc="upper left", bbox_to_anchor=(0.1, 0.95))
    fig.tight_layout()

    plt.savefig(os.path.join(output_folder, f"vaultx_{machine}.svg"))
    plt.close()
