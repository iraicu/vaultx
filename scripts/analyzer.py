#!/usr/bin/env python3

import argparse
import subprocess
import threading
import queue
import re
import sys
import select
import os
from datetime import datetime, timedelta
import pandas as pd
try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    from matplotlib.ticker import FuncFormatter
except ImportError:
    print("Error: Matplotlib is required for plotting.", file=sys.stderr)
    print("Please install it with: pip install matplotlib", file=sys.stderr)
    # We don't exit here, as the script can still run without plotting.
    plt = None
def stream_reader(pipe, q, label):
    """Reads a process's output stream line by line and puts it into a queue."""
    try:
        for line in iter(pipe.readline, ''):
            q.put((label, line))
    finally:
        pipe.close()

def read_available_output(pipe, buffer_dict, label):
    """Read available output from a pipe without blocking."""
    if pipe is None or pipe.poll() is not None:
        return []
    
    lines = []
    try:
        # Check if data is available to read
        if hasattr(select, 'select'):
            ready, _, _ = select.select([pipe.stdout], [], [], 0)
            if ready:
                data = pipe.stdout.read(8192)  # Read up to 8KB
                if data:
                    buffer_dict[label] += data
                    # Split into lines but keep incomplete line in buffer
                    parts = buffer_dict[label].split('\n')
                    lines = parts[:-1]  # All complete lines
                    buffer_dict[label] = parts[-1]  # Keep incomplete line
        else:
            # Fallback for systems without select (like Windows)
            import fcntl
            fd = pipe.stdout.fileno()
            fl = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)
            try:
                data = pipe.stdout.read(8192)
                if data:
                    buffer_dict[label] += data
                    parts = buffer_dict[label].split('\n')
                    lines = parts[:-1]
                    buffer_dict[label] = parts[-1]
            except IOError:
                pass
    except Exception as e:
        # If non-blocking read fails, fall back to readline
        try:
            line = pipe.stdout.readline()
            if line:
                lines = [line.rstrip('\n\r')]
        except:
            pass
    
    return lines

def run_single_threaded_monitoring(vaultx_proc, exp_start_time):
    """Monitor vaultx with single-threaded non-blocking I/O."""
    # Use buffers for non-blocking I/O instead of queue
    output_buffers = {"vaultx": "", "pidstat": ""}
    
    pidstat_proc = None
    vaultx_pid = None
    vaultx_log, pidstat_log = [], []

    # Main loop with single-threaded non-blocking I/O
    print("--- Processing output (single-threaded) ---")
    while vaultx_proc.poll() is None:
        # Read available output from vaultx
        vaultx_lines = read_available_output(vaultx_proc, output_buffers, "vaultx")
        for line in vaultx_lines:
            if line.strip():  # Skip empty lines
                sys.stdout.write(f"[vaultx] {line}\n")
                vaultx_log.append(line)
                
                # Check for PID and start pidstat if not already started
                if not vaultx_pid:
                    pid_match = re.match(r"VAULTX_PID: (\d+)", line)
                    if pid_match:
                        vaultx_pid = int(pid_match.group(1))
                        print(f"--- Starting pidstat for PID {vaultx_pid} ---")
                        pidstat_cmd = ['pidstat', '-h', '-r', '-u', '-d', '-p', str(vaultx_pid), '1']
                        pidstat_proc = subprocess.Popen(pidstat_cmd, stdout=subprocess.PIPE, text=True, bufsize=1)
                        output_buffers["pidstat"] = ""
        
        # Read available output from pidstat (if running)
        if pidstat_proc:
            pidstat_lines = read_available_output(pidstat_proc, output_buffers, "pidstat")
            for line in pidstat_lines:
                if line.strip():
                    pidstat_log.append(line)
        
        # Small sleep to prevent busy waiting
        import time
        time.sleep(0.01)  # 10ms sleep
    
    # Process any remaining output after vaultx finishes
    final_vaultx_lines = read_available_output(vaultx_proc, output_buffers, "vaultx")
    for line in final_vaultx_lines:
        if line.strip():
            sys.stdout.write(f"[vaultx] {line}\n")
            vaultx_log.append(line)
    
    # Process any remaining pidstat output
    if pidstat_proc:
        final_pidstat_lines = read_available_output(pidstat_proc, output_buffers, "pidstat")
        for line in final_pidstat_lines:
            if line.strip():
                pidstat_log.append(line)

    print("--- vaultx process finished ---")

    # Terminate pidstat if still running
    if pidstat_proc:
        pidstat_proc.terminate()
        pidstat_proc.wait()
    
    return vaultx_log, pidstat_log

def run_multi_threaded_monitoring(vaultx_proc, exp_start_time):
    """Monitor vaultx with multi-threaded approach (original)."""
    # Use a queue to gather output from threads
    q = queue.Queue()
    
    # Start thread to read vaultx output
    vaultx_thread = threading.Thread(target=stream_reader, args=(vaultx_proc.stdout, q, "vaultx"))
    vaultx_thread.start()
    
    pidstat_proc = None
    vaultx_pid = None
    vaultx_log, pidstat_log = [], []

    # Main loop to process queue and start pidstat once PID is found
    while vaultx_proc.poll() is None or not q.empty():
        try:
            label, line = q.get(timeout=0.1)
            if label == "vaultx":
                sys.stdout.write(f"[vaultx] {line}")
                vaultx_log.append(line)
                if not vaultx_pid:
                    pid_match = re.match(r"VAULTX_PID: (\d+)", line)
                    if pid_match:
                        vaultx_pid = int(pid_match.group(1))
                        print(f"--- Starting pidstat for PID {vaultx_pid} ---")
                        pidstat_cmd = ['pidstat', '-h', '-r', '-u', '-d', '-p', str(vaultx_pid), '1']
                        pidstat_proc = subprocess.Popen(pidstat_cmd, stdout=subprocess.PIPE, text=True, bufsize=1)
                        pidstat_thread = threading.Thread(target=stream_reader, args=(pidstat_proc.stdout, q, "pidstat"))
                        pidstat_thread.start()
            
            elif label == "pidstat":
                pidstat_log.append(line)

        except queue.Empty:
            continue

    print("--- vaultx process finished ---")

    # Wait for all processes and threads to complete
    vaultx_thread.join()
    if pidstat_proc:
        pidstat_proc.terminate()
        pidstat_thread.join()
    
    return vaultx_log, pidstat_log

def parse_vaultx_log(lines, exp_start_time):
    """Parses vaultx output for stage markers and PID."""
    stage_starts = {}
    stages = {}
    vaultx_pid = None
    stage_counters = {}  # Track how many times each stage name has appeared
    
    pid_pattern = re.compile(r"VAULTX_PID: (\d+)")
    # Updated pattern to match new format: [time] VAULTX_STAGE_MARKER: START/END StageName
    marker_pattern = re.compile(r"\[(\d+(?:\.\d+)?)\]\s*VAULTX_STAGE_MARKER:\s*(START|END)\s+(\S+)")

    for line in lines:
        pid_match = pid_pattern.search(line)
        if pid_match:
            vaultx_pid = int(pid_match.group(1))
            print(f"--> Captured vaultx PID: {vaultx_pid}")
            continue

        marker_match = marker_pattern.search(line)
        if marker_match:
            elapsed_str, event, name = marker_match.groups()
            elapsed_seconds = float(elapsed_str)
            timestamp = exp_start_time + timedelta(seconds=elapsed_seconds)

            if event == "START":
                # Create a unique stage name by adding a counter for repeated stages
                if name in stage_counters:
                    stage_counters[name] += 1
                    unique_name = f"{name}_{stage_counters[name]}"
                else:
                    stage_counters[name] = 1
                    unique_name = name
                
                stage_starts[unique_name] = timestamp
                print(f"--> Stage '{unique_name}' started at {timestamp.strftime('%H:%M:%S')}")
            elif event == "END":
                # Find the most recent START for this stage name
                matching_key = None
                for key in stage_starts.keys():
                    if key.startswith(name):
                        matching_key = key
                
                if matching_key:
                    stages[matching_key] = {
                        "start": stage_starts[matching_key],
                        "end": timestamp,
                        "duration_s": (timestamp - stage_starts[matching_key]).total_seconds()
                    }
                    print(f"--> Stage '{matching_key}' ended at {timestamp.strftime('%H:%M:%S')} (Duration: {stages[matching_key]['duration_s']:.2f}s)")
                    del stage_starts[matching_key]
    
    return vaultx_pid, stages

def parse_pidstat_log(lines, date_str):
    """Parses pidstat output for resource metrics."""
    pidstat_data = []
    header_skipped = False
    
    for line in lines:
        if line.startswith('#'):
            continue
        if not header_skipped:
            header_skipped = True
            continue
            
        parts = line.split()
        if len(parts) < 10:
            continue

        try:
            time_str, _uid, _pid, usr_cpu, sys_cpu, _guest, _wait, cpu, _cpu_id, \
            _minflt, _majflt, vsz, rss, mem, \
            kb_rd_s, kb_wr_s, _kb_ccwr_s, _iodelay = parts[0:18]

            # Combine date and time for a full datetime object
            record_time = datetime.strptime(f"{date_str} {time_str}", "%Y-%m-%d %H:%M:%S")

            pidstat_data.append({
                "time": record_time,
                "cpu_total": float(cpu),
                "mem_rss_kb": float(rss),
                "io_read_kb_s": float(kb_rd_s),
                "io_write_kb_s": float(kb_wr_s)
            })
        except (ValueError, IndexError):
            # Ignore lines that don't parse correctly
            continue
            
    return pidstat_data

def analyze(stages, pidstat_data):
    """Correlates stages with pidstat data and calculates averages."""
    analysis_results = {}

    for name, times in stages.items():
        stage_metrics = {
            "cpu_total": [], "mem_rss_kb": [], 
            "io_read_kb_s": [], "io_write_kb_s": []
        }
        
        for record in pidstat_data:
            if times["start"] <= record["time"] < times["end"]:
                for key in stage_metrics:
                    stage_metrics[key].append(record[key])
        
        # Calculate averages
        avg_metrics = {"duration_s": times["duration_s"]}
        for key, values in stage_metrics.items():
            if values:
                avg_metrics[f"avg_{key}"] = sum(values) / len(values)
            else:
                avg_metrics[f"avg_{key}"] = 0.0
        
        analysis_results[name] = avg_metrics
    
    return analysis_results

def create_performance_plot(stages, pidstat_df, output_filename):
    """Creates a plot of resource usage over time with stage markers."""
    if plt is None:
        print("Cannot create plot because matplotlib is not installed.", file=sys.stderr)
        return

    fig, axes = plt.subplots(3, 1, figsize=(18, 12), sharex=True)
    fig.suptitle('VaultX Performance Profile', fontsize=16)

    # --- Plot Metrics ---
    # CPU
    axes[0].plot(pidstat_df.index, pidstat_df['cpu_total'], color='royalblue', label='Total CPU')
    axes[0].set_ylabel('CPU Usage (%)')
    axes[0].grid(True, linestyle='--', alpha=0.6)

    # Memory (RSS)
    axes[1].plot(pidstat_df.index, pidstat_df['mem_rss_kb'] / 1024, color='forestgreen', label='RSS Memory')
    axes[1].set_ylabel('Memory RSS (MB)')
    axes[1].grid(True, linestyle='--', alpha=0.6)
    axes[1].yaxis.set_major_formatter(FuncFormatter(lambda x, p: f'{x:,.0f}'))

    # I/O
    axes[2].plot(pidstat_df.index, pidstat_df['io_read_kb_s'], color='darkorange', label='Read')
    axes[2].plot(pidstat_df.index, pidstat_df['io_write_kb_s'], color='crimson', label='Write')
    axes[2].set_ylabel('I/O (KB/s)')
    axes[2].set_xlabel('Time')
    axes[2].grid(True, linestyle='--', alpha=0.6)
    axes[2].legend(loc='upper left')

    # --- Stage descriptions for better legend ---
    stage_descriptions = {
        'Table1Gen': 'Table 1 Generation (Hash computation)',
        'WriteTable1': 'Write Table 1 (Disk I/O)',
        'OutOfMemoryTable2Gen': 'Table 2 Generation (Out of memory)',
        'Table2Shuffle': 'Table 2 Shuffle (Data reorganization)',
        'FinalizePlotFile': 'Finalize Plot File (Final I/O)',
        'Verification': 'Verification (Data validation)'
    }

    # --- Add Stage Markers ---
    stage_patches = []
    
    # Group stages by their base name for consistent coloring
    stage_groups = {}
    for name, times in stages.items():
        base_name = name.split('_')[0] if '_' in name else name
        if base_name not in stage_groups:
            stage_groups[base_name] = []
        stage_groups[base_name].append((name, times))
    
    # Create colors for each unique base stage name
    colors = plt.cm.get_cmap('viridis', len(stage_groups))
    color_map = {}
    for i, base_name in enumerate(stage_groups.keys()):
        color_map[base_name] = colors(i)
    
    # Plot stages with consistent colors for same base names
    for name, times in stages.items():
        base_name = name.split('_')[0] if '_' in name else name
        color = color_map[base_name]
        
        for ax in axes:
            ax.axvspan(times['start'], times['end'], color=color, alpha=0.2, zorder=0)
    
    # Create legend entries - one per unique base stage name
    for base_name, stage_list in stage_groups.items():
        color = color_map[base_name]
        description = stage_descriptions.get(base_name, base_name)
        
        # Calculate total duration for all instances of this stage
        total_duration = sum(times['duration_s'] for name, times in stage_list)
        
        # Show count if there are multiple instances
        if len(stage_list) > 1:
            label = f"{description} (×{len(stage_list)})\n({total_duration:.2f}s total)"
        else:
            label = f"{description}\n({total_duration:.2f}s)"
        
        stage_patches.append(mpatches.Patch(color=color, alpha=0.3, label=label))

    # Add a single legend for all stages outside the plot area
    fig.legend(handles=stage_patches, bbox_to_anchor=(0.81, 0.95), loc='upper left', 
               title="VaultX Stages & Duration", fontsize=10, title_fontsize=12)

    plt.tight_layout(rect=[0, 0, 0.82, 0.96]) # Adjust layout to make space for legend
    
    print(f"\n--- Saving performance plot to {output_filename} ---")
    plt.savefig(output_filename, dpi=150)
    plt.close()


def main():
    parser = argparse.ArgumentParser(
        description="Run vaultx, monitor with pidstat, and analyze resource usage per stage.",
        epilog="Example: ./analyzer.py -- ./vaultx -K 28 -m 512 -f ./plots/ -g ./plots/ -j ./plots/",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument('command', nargs=argparse.REMAINDER, help="The vaultx command to run. Must be preceded by '--'.")
    parser.add_argument(
        '--plot-file',
        type=str,
        help="Filename for the output performance plot (e.g., performance.png)."
    )
    parser.add_argument(
        '--single-thread',
        action='store_true',
        help="Use single-threaded non-blocking I/O instead of separate threads for monitoring (reduces CPU overhead)."
    )
    args = parser.parse_args()

    if not args.command or args.command[0] != '--':
        parser.print_help()
        sys.exit("Error: The vaultx command must be provided after '--'.")

    vaultx_cmd = args.command[1:]

    print("--- Experiment Starting ---")
    print(f"Running command: {' '.join(vaultx_cmd)}")

    exp_start_time = datetime.now()
    exp_date_str = exp_start_time.strftime('%Y-%m-%d')

    # Start vaultx process
    try:
        vaultx_proc = subprocess.Popen(
            vaultx_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, # Merge stderr with stdout
            text=True,
            bufsize=1
        )
    except FileNotFoundError:
        sys.exit(f"Error: Command not found: '{vaultx_cmd[0]}'. Make sure it's in your PATH or specify the full path.")

    vaultx_log, pidstat_log = [], []
    
    if args.single_thread:
        # Single-threaded approach with non-blocking I/O
        vaultx_log, pidstat_log = run_single_threaded_monitoring(vaultx_proc, exp_start_time)
    else:
        # Multi-threaded approach (original)
        vaultx_log, pidstat_log = run_multi_threaded_monitoring(vaultx_proc, exp_start_time)

    print("\n--- Analysis ---")

    # Parse logs
    _pid, stages = parse_vaultx_log(vaultx_log, exp_start_time)
    if not stages:
        print("No vaultx stages were parsed. Cannot generate report.")
        print("Ensure vaultx was compiled with stage markers and not in benchmark mode.")
        return

    pidstat_data = parse_pidstat_log(pidstat_log, exp_date_str)
    if not pidstat_data:
        print("No pidstat data was parsed. Cannot generate report.")
        return

    # Create a DataFrame for easier manipulation
    pidstat_df = pd.DataFrame(pidstat_data)
    if not pidstat_df.empty:
        pidstat_df.set_index('time', inplace=True)

    # Correlate and analyze
    results = analyze(stages, pidstat_data) # The original analyze function is fine

    # Print report
    print("\n--- Resource Usage Report (Averages per Stage) ---")
    df = pd.DataFrame.from_dict(results, orient='index')
    df.index.name = "Stage"
    df = df.rename(columns={
        "duration_s": "Duration (s)",
        "avg_cpu_total": "Avg CPU (%)",
        "avg_mem_rss_kb": "Avg RSS (KB)",
        "avg_io_read_kb_s": "Avg Read (KB/s)",
        "avg_io_write_kb_s": "Avg Write (KB/s)"
    })

    with pd.option_context('display.max_rows', None,
                           'display.max_columns', None,
                           'display.width', 120,
                           'display.float_format', '{:,.2f}'.format):
        print(df)

    if args.plot_file and not pidstat_df.empty:
        create_performance_plot(stages, pidstat_df, args.plot_file)

if __name__ == "__main__":
    main()