import pandas as pd
import matplotlib.pyplot as plt
import os
import sys

def plot_vaultx_performance(csv_filepath, output_dir):
    """
    Reads performance data from a CSV, generates plots for key metrics 
    against memory size, and saves the images to the output directory.
    """
    
    # --- 1. Setup Paths ---
    # The script is expected to be run from the base directory, 
    # so paths are relative to the execution location.
    
    # Ensure the output directory exists
    os.makedirs(output_dir, exist_ok=True)
    
    # --- 2. Data Loading and Validation ---
    try:
        df = pd.read_csv(csv_filepath)
    except FileNotFoundError:
        print(f"ERROR: Data file not found at '{csv_filepath}'.")
        print("Please ensure you have run the test script or created the file.")
        sys.exit(1)
    except Exception as e:
        print(f"ERROR reading CSV: {e}")
        sys.exit(1)

    # --- 3. Data Cleaning and Preparation ---
    # Convert relevant columns to numeric types, coercing errors to NaN
    numeric_cols = ['memory_mb', 'total_time', 'throughput_mh', 'io_throughput', 'storage_efficiency']
    for col in numeric_cols:
        df[col] = pd.to_numeric(df[col], errors='coerce')

    # Drop any rows where key columns failed conversion (e.g., if 'FAILED' was present)
    df.dropna(subset=['memory_mb', 'throughput_mh'], inplace=True)
    
    if df.empty:
        print("ERROR: Dataframe is empty after cleanup. Cannot plot.")
        sys.exit(1)

    # --- 4. Plot Generation ---
    
    # Define the colors and markers
    color_throughput = '#1f77b4'  # Blue
    color_time = '#ff7f0e'        # Orange
    
    # --- Plot 1: Throughput and Total Time vs. Memory ---
    fig, ax1 = plt.subplots(figsize=(10, 6))
    
    # Title and Configuration
    fig.suptitle('VaultX K32 Performance vs. Allocated RAM', fontsize=16, weight='bold')
    ax1.set_title(f'Match Factor: {df["throughput_mh"].max():.2f} Max MH/s @ {df.loc[df["throughput_mh"].idxmax(), "memory_mb"]} MB', fontsize=12)
    
    # Y-axis 1: Throughput (MH/s)
    ax1.plot(df['memory_mb'], df['throughput_mh'], color=color_throughput, marker='o', 
             linestyle='-', label='Throughput (MH/s)', linewidth=2)
    ax1.set_xlabel('Memory Allocation (MB)', fontsize=12)
    ax1.set_ylabel('Throughput (MH/s)', color=color_throughput, fontsize=12)
    ax1.tick_params(axis='y', labelcolor=color_throughput)
    
    # Y-axis 2: Total Time (s)
    ax2 = ax1.twinx()
    ax2.plot(df['memory_mb'], df['total_time'], color=color_time, marker='s', 
             linestyle='--', label='Total Time (s)', linewidth=2)
    ax2.set_ylabel('Total Time (s)', color=color_time, fontsize=12)
    ax2.tick_params(axis='y', labelcolor=color_time)
    
    # Grid, Legend, and Layout
    ax1.grid(True, linestyle='--', alpha=0.6)
    fig.tight_layout(rect=[0, 0.03, 1, 0.95]) # Adjust layout for suptitle
    
    # Add data labels for points
    for i, row in df.iterrows():
        # Label throughput on ax1
        ax1.annotate(f"{row['throughput_mh']:.2f}", (row['memory_mb'], row['throughput_mh']),
                     textcoords="offset points", xytext=(0, 5), ha='center', fontsize=8, color=color_throughput)
        # Label time on ax2
        ax2.annotate(f"{row['total_time']:.1f}s", (row['memory_mb'], row['total_time']),
                     textcoords="offset points", xytext=(0, -15), ha='center', fontsize=8, color=color_time)


    # Set X-axis to logarithmic scale for better visualization of exponential growth
    ax1.set_xscale('log', base=2)
    # Ensure only the actual memory points are labeled on the x-axis
    ax1.set_xticks(df['memory_mb'].unique())
    ax1.get_xaxis().set_major_formatter(plt.ScalarFormatter())

    # Save the plot
    output_filename = os.path.join(output_dir, 'throughput_vs_memory.png')
    plt.savefig(output_filename)
    plt.close(fig)
    
    # --- Plot 2: Storage Efficiency vs. Memory (Simple Line) ---
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(df['memory_mb'], df['storage_efficiency'], color='green', marker='^', linestyle='-', linewidth=2)
    
    ax.set_title('Storage Efficiency vs. Memory Allocation', fontsize=14)
    ax.set_xlabel('Memory Allocation (MB)', fontsize=12)
    ax.set_ylabel('Storage Efficiency (%)', fontsize=12)
    
    # Ensure X-axis is logarithmic and labels are correct
    ax.set_xscale('log', base=2)
    ax.set_xticks(df['memory_mb'].unique())
    ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())
    
    ax.grid(True, linestyle='--', alpha=0.6)
    plt.ylim(80, 101) # Set y-limit to focus on 80-100% range

    # Add data labels
    for i, row in df.iterrows():
        ax.annotate(f"{row['storage_efficiency']:.2f}%", (row['memory_mb'], row['storage_efficiency']),
                    textcoords="offset points", xytext=(0, 5), ha='center', fontsize=8, color='green')

    # Save the plot
    output_filename_eff = os.path.join(output_dir, 'storage_efficiency_vs_memory.png')
    plt.savefig(output_filename_eff)
    plt.close(fig)

    print(f"\nSUCCESS: Generated two plots:")
    print(f"- Throughput/Time saved to: {output_filename}")
    print(f"- Storage Efficiency saved to: {output_filename_eff}")
    print("Check the 'graph' folder for the images.")


if __name__ == '__main__':
    # Define paths relative to the current working directory (base folder)
    CSV_FILEPATH = './data/memory_test_results.csv'
    OUTPUT_DIR = './graph'
    
    plot_vaultx_performance(CSV_FILEPATH, OUTPUT_DIR)