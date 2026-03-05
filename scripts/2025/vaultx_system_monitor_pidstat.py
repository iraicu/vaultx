#!/usr/bin/env python3

import argparse
import subprocess
import threading
import time
import re
import sys
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
    sys.exit(1)

def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(description='Monitor VaultX vs System-wide performance using pidstat')
    parser.add_argument('--plot-file', type=str, default='vaultx_system_comparison_pidstat.png',
                       help='Output plot file name (default: vaultx_system_comparison_pidstat.png)')
    parser.add_argument('--interval', type=int, default=1,
                       help='Monitoring interval in seconds (default: 1)')
    parser.add_argument('--csv-output', type=str,
                       help='Save raw data to CSV file')
    parser.add_argument('vaultx_args', nargs=argparse.REMAINDER,
                       help='VaultX command and arguments')
    
    args = parser.parse_args()
    
    # Remove '--' separator if present at the beginning of vaultx_args
    if args.vaultx_args and args.vaultx_args[0] == '--':
        args.vaultx_args = args.vaultx_args[1:]
    
    return args

def extract_vaultx_pid(output_line):
    """Extract VaultX PID from output line."""
    pid_match = re.search(r'VAULTX_PID:\s*(\d+)', output_line)
    if pid_match:
        return int(pid_match.group(1))
    return None

def parse_stage_marker(output_line, experiment_start_time):
    """Parse stage marker from VaultX output."""
    pattern = r'\[([0-9.]+)\] VAULTX_STAGE_MARKER: (START|END) (\w+)'
    match = re.search(pattern, output_line)
    
    if match:
        timestamp_sec = float(match.group(1))
        action = match.group(2)
        stage_name = match.group(3)
        
        timestamp = experiment_start_time + timedelta(seconds=timestamp_sec)
        
        return {
            'timestamp': timestamp,
            'action': action,
            'stage': stage_name,
            'relative_time': timestamp_sec
        }
    
    return None

def monitor_vaultx_output(process, experiment_start_time):
    """Monitor VaultX output for stage markers."""
    stage_events = []
    
    print("--- Monitoring VaultX output for stage markers ---")
    
    for line in iter(process.stdout.readline, ''):
        if not line:
            break
            
        line = line.strip()
        print(f"[vaultx] {line}")
        
        stage_event = parse_stage_marker(line, experiment_start_time)
        if stage_event:
            stage_events.append(stage_event)
            print(f"--> Stage '{stage_event['stage']}' {stage_event['action'].lower()}ed at {stage_event['timestamp'].strftime('%H:%M:%S')} (t={stage_event['relative_time']:.2f}s)")
    
    return stage_events

def run_combined_pidstat_all(vaultx_pid, interval, experiment_start_time, stop_event):
    """Run single pidstat -p ALL to monitor all processes and extract both system and VaultX data."""
    print(f"--- Starting combined pidstat -p ALL monitoring ---")
    print(f"VaultX PID: {vaultx_pid}, will extract VaultX data from system-wide collection")
    print("This ensures perfect time alignment between system and VaultX metrics")
    
    # Get number of CPU cores for reference
    num_cores = os.cpu_count()
    
    combined_data = []
    
    try:
        # Use pidstat -p ALL for all processes
        cmd = ['pidstat', '-u', '-r', '-d', '-p', 'ALL', str(interval)]
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                                 text=True, bufsize=1)
        
        current_section = None
        current_timestamp = None
        timestamp_data = {}
        
        while not stop_event.is_set():
            line = process.stdout.readline()
            if not line:
                break
                
            line = line.strip()
            if not line or line.startswith('Linux'):
                continue
                
            # Skip Average lines
            if 'Average' in line:
                continue
                
            # Detect section headers
            if '%usr' in line and '%system' in line:
                current_section = 'cpu'
                continue
            elif 'minflt/s' in line and 'majflt/s' in line:
                current_section = 'memory'
                continue
            elif 'kB_rd/s' in line and 'kB_wr/s' in line:
                current_section = 'io'
                continue
            elif 'UID' in line and 'PID' in line:
                continue
                
            # Parse data lines
            parts = line.split()
            if len(parts) >= 8:
                try:
                    time_str = parts[0]
                    pid = int(parts[2])
                    
                    # Extract timestamp from pidstat output (HH:MM:SS format)
                    # Use the timestamp from pidstat rather than system time for consistency
                    current_time = datetime.now().replace(microsecond=0)
                    
                    # Initialize timestamp data if new
                    if current_time != current_timestamp:
                        # Process previous timestamp if we have data
                        if current_timestamp and timestamp_data:
                            combined_data.append(aggregate_combined_data(timestamp_data, current_timestamp, experiment_start_time, vaultx_pid))
                        
                        # Reset for new timestamp
                        current_timestamp = current_time
                        timestamp_data = {
                            'system_cpu_user': 0.0,
                            'system_cpu_system': 0.0,
                            'system_cpu_total': 0.0,
                            'system_mem_rss': 0.0,
                            'system_io_read': 0.0,
                            'system_io_write': 0.0,
                            'system_process_count': 0,
                            'vaultx_cpu_user': 0.0,
                            'vaultx_cpu_system': 0.0,
                            'vaultx_cpu_total': 0.0,
                            'vaultx_mem_rss': 0.0,
                            'vaultx_mem_percent': 0.0,
                            'vaultx_io_read': 0.0,
                            'vaultx_io_write': 0.0,
                            'vaultx_found': False
                        }
                    
                    # Aggregate data based on current section
                    if current_section == 'cpu' and len(parts) >= 8:
                        cpu_user = float(parts[3])
                        cpu_system = float(parts[4])
                        cpu_total = float(parts[7])
                        
                        # Add to system totals
                        timestamp_data['system_cpu_user'] += cpu_user
                        timestamp_data['system_cpu_system'] += cpu_system
                        timestamp_data['system_cpu_total'] += cpu_total
                        timestamp_data['system_process_count'] += 1
                        
                        # Check if this is VaultX process
                        if pid == vaultx_pid:
                            timestamp_data['vaultx_cpu_user'] = cpu_user
                            timestamp_data['vaultx_cpu_system'] = cpu_system
                            timestamp_data['vaultx_cpu_total'] = cpu_total
                            timestamp_data['vaultx_found'] = True
                            
                    elif current_section == 'memory' and len(parts) >= 8:
                        mem_rss = float(parts[6])  # RSS in KB
                        mem_percent = float(parts[7])
                        
                        # Add to system totals
                        timestamp_data['system_mem_rss'] += mem_rss
                        
                        # Check if this is VaultX process
                        if pid == vaultx_pid:
                            timestamp_data['vaultx_mem_rss'] = mem_rss
                            timestamp_data['vaultx_mem_percent'] = mem_percent
                            
                    elif current_section == 'io' and len(parts) >= 7:
                        io_read = float(parts[3])   # kB/s
                        io_write = float(parts[4])  # kB/s
                        
                        # Add to system totals
                        timestamp_data['system_io_read'] += io_read
                        timestamp_data['system_io_write'] += io_write
                        
                        # Check if this is VaultX process
                        if pid == vaultx_pid:
                            timestamp_data['vaultx_io_read'] = io_read
                            timestamp_data['vaultx_io_write'] = io_write
                        
                except (ValueError, IndexError):
                    continue
        
        # Process final timestamp data
        if current_timestamp and timestamp_data:
            combined_data.append(aggregate_combined_data(timestamp_data, current_timestamp, experiment_start_time, vaultx_pid))
        
        process.terminate()
        process.wait()
        
    except Exception as e:
        print(f"Error running combined pidstat: {e}")
    
    return combined_data

def aggregate_combined_data(timestamp_data, current_time, experiment_start_time, vaultx_pid):
    """Aggregate data from all processes for a single timestamp, including VaultX-specific data."""
    result = {
        'time': current_time,
        'relative_time': (current_time - experiment_start_time).total_seconds(),
        'system_cpu_user': timestamp_data['system_cpu_user'],
        'system_cpu_system': timestamp_data['system_cpu_system'],
        'system_cpu_total': timestamp_data['system_cpu_total'],
        'system_mem_rss_total': timestamp_data['system_mem_rss'],  # Total RSS from all processes
        'system_io_read': timestamp_data['system_io_read'],
        'system_io_write': timestamp_data['system_io_write'],
        'system_process_count': timestamp_data['system_process_count']
    }
    
    # Add VaultX-specific data if found
    if timestamp_data['vaultx_found']:
        result.update({
            'vaultx_cpu_user': timestamp_data['vaultx_cpu_user'],
            'vaultx_cpu_system': timestamp_data['vaultx_cpu_system'],
            'vaultx_cpu_total': timestamp_data['vaultx_cpu_total'],
            'vaultx_mem_rss': timestamp_data['vaultx_mem_rss'],
            'vaultx_mem_percent': timestamp_data['vaultx_mem_percent'],
            'vaultx_io_read': timestamp_data['vaultx_io_read'],
            'vaultx_io_write': timestamp_data['vaultx_io_write'],
            'pid': vaultx_pid
        })
    
    return result

def get_system_memory_info():
    """Get total system memory from /proc/meminfo."""
    try:
        with open('/proc/meminfo', 'r') as f:
            for line in f:
                if line.startswith('MemTotal:'):
                    # Extract memory in KB
                    return int(line.split()[1])
    except Exception:
        pass
    return None

def process_stage_events(stage_events):
    """Process stage events into start/end pairs."""
    stages = {}
    stage_starts = {}
    
    for event in stage_events:
        stage_name = event['stage']
        
        if event['action'] == 'START':
            stage_starts[stage_name] = event['timestamp']
        elif event['action'] == 'END':
            if stage_name in stage_starts:
                stages[stage_name] = {
                    'start': stage_starts[stage_name],
                    'end': event['timestamp'],
                    'duration': (event['timestamp'] - stage_starts[stage_name]).total_seconds()
                }
                del stage_starts[stage_name]
    
    return stages

def merge_data(combined_data):
    """Process combined data that already has perfect time alignment."""
    if not combined_data:
        return []
    
    print(f"Debug: Combined data points: {len(combined_data)}")
    
    # Data is already aligned, just sort by time
    combined_data.sort(key=lambda x: x['time'])
    
    return combined_data

def create_comparison_plot(stages, merged_data, plot_file, total_memory_kb=None):
    """Create comparison plots showing system vs VaultX resource usage on the same plots."""
    if not merged_data:
        print("No data available for plotting")
        return

    # Get K value for title if available
    k_value = getattr(create_comparison_plot, 'k_value', None)
    num_cores = os.cpu_count()

    df = pd.DataFrame(merged_data)
    
    # Clean and validate data
    print(f"Raw data points: {len(df)}")
    
    # Remove rows with invalid timestamps
    df = df.dropna(subset=['time'])
    df['time'] = pd.to_datetime(df['time'])
    
    # Remove duplicate timestamps, keeping the last occurrence
    df = df.drop_duplicates(subset=['time'], keep='last')
    
    # Sort by time
    df = df.sort_values('time')
    
    # Clean CPU data - system values from pidstat can go higher than vmstat
    max_cpu = num_cores * 100
    for col in df.columns:
        if 'cpu' in col:
            df[col] = df[col].clip(0, max_cpu * 2)  # Allow for some overhead in pidstat aggregation
    
    print(f"Cleaned data points: {len(df)}")
    
    if len(df) == 0:
        print("No valid data points after cleaning")
        return
    
    df.set_index('time', inplace=True)
    
    # Create figure with 3 subplots (CPU, Memory, I/O)
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(16, 12))
    
    title = 'System vs VaultX Resource Usage Comparison (pidstat -p ALL)'
    if k_value is not None:
        title += f' (K={k_value})'
    title += f' - {num_cores} cores'
    fig.suptitle(title, fontsize=16)
    
    # CPU Usage Comparison (System vs VaultX on same plot)
    ax1.set_title('CPU Usage Comparison (pidstat aggregated from all processes)')
    
    # System CPU metrics (solid lines) - aggregated from all processes
    if 'system_cpu_total' in df.columns and not df['system_cpu_total'].isna().all():
        ax1.plot(df.index, df['system_cpu_total'], color='#e74c3c', linewidth=2.5, 
                label='System Total CPU % (all procs)', linestyle='-', alpha=0.8)
    if 'system_cpu_user' in df.columns and not df['system_cpu_user'].isna().all():
        ax1.plot(df.index, df['system_cpu_user'], color='#3498db', linewidth=2, 
                label='System User CPU % (all procs)', linestyle='-', alpha=0.7)
    if 'system_cpu_system' in df.columns and not df['system_cpu_system'].isna().all():
        ax1.plot(df.index, df['system_cpu_system'], color='#f39c12', linewidth=2, 
                label='System Kernel CPU % (all procs)', linestyle='-', alpha=0.7)
    
    # VaultX CPU metrics (dashed lines) - raw per-core values
    if 'vaultx_cpu_total' in df.columns and not df['vaultx_cpu_total'].isna().all():
        ax1.plot(df.index, df['vaultx_cpu_total'], color='#c0392b', linewidth=2.5, 
                label='VaultX Total CPU % (raw)', linestyle='--', alpha=0.8)
    if 'vaultx_cpu_user' in df.columns and not df['vaultx_cpu_user'].isna().all():
        ax1.plot(df.index, df['vaultx_cpu_user'], color='#2980b9', linewidth=2, 
                label='VaultX User CPU % (raw)', linestyle='--', alpha=0.7)
    if 'vaultx_cpu_system' in df.columns and not df['vaultx_cpu_system'].isna().all():
        ax1.plot(df.index, df['vaultx_cpu_system'], color='#d68910', linewidth=2, 
                label='VaultX Kernel CPU % (raw)', linestyle='--', alpha=0.7)
    
    ax1.set_ylabel('CPU Usage (%)')
    ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax1.grid(True, alpha=0.3)
    
    # Set y-axis limits based on actual data range
    cpu_columns = ['system_cpu_total', 'system_cpu_user', 'system_cpu_system', 
                   'vaultx_cpu_total', 'vaultx_cpu_user', 'vaultx_cpu_system']
    cpu_data = []
    for col in cpu_columns:
        if col in df.columns and not df[col].isna().all():
            cpu_data.extend(df[col].dropna().tolist())
    
    if cpu_data:
        min_cpu = max(0, min(cpu_data) - 5)  # Add 5% padding below, but don't go below 0
        max_cpu_actual = max(cpu_data) + 10  # Add 10% padding above
        ax1.set_ylim(min_cpu, max_cpu_actual)
    
    # Memory Usage Comparison
    ax2.set_title('Memory Usage Comparison')
    
    # System memory (solid line) - total RSS from all processes
    if 'system_mem_rss_total' in df.columns and not df['system_mem_rss_total'].isna().all():
        ax2.plot(df.index, df['system_mem_rss_total'] / 1024, color='#9b59b6', linewidth=2.5, 
                label='System Total RSS (all procs) (MB)', linestyle='-', alpha=0.8)
    
    # VaultX memory (dashed line)
    if 'vaultx_mem_rss' in df.columns and not df['vaultx_mem_rss'].isna().all():
        ax2.plot(df.index, df['vaultx_mem_rss'] / 1024, color='#8e44ad', linewidth=2.5, 
                label='VaultX RSS Memory (MB)', linestyle='--', alpha=0.8)
    
    ax2.set_ylabel('Memory Usage (MB)')
    ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax2.grid(True, alpha=0.3)
    
    # I/O Usage Comparison
    ax3.set_title('I/O Usage Comparison')
    
    # System I/O (solid lines) - aggregated from all processes
    if 'system_io_read' in df.columns and not df['system_io_read'].isna().all():
        ax3.plot(df.index, df['system_io_read'], color='#27ae60', linewidth=2.5, 
                label='System Read (all procs) (kB/s)', linestyle='-', alpha=0.8)
    if 'system_io_write' in df.columns and not df['system_io_write'].isna().all():
        ax3.plot(df.index, df['system_io_write'], color='#e67e22', linewidth=2.5, 
                label='System Write (all procs) (kB/s)', linestyle='-', alpha=0.8)
    
    # VaultX I/O (dashed lines)
    if 'vaultx_io_read' in df.columns and not df['vaultx_io_read'].isna().all():
        ax3.plot(df.index, df['vaultx_io_read'], color='#229954', linewidth=2.5, 
                label='VaultX Read (kB/s)', linestyle='--', alpha=0.8)
    if 'vaultx_io_write' in df.columns and not df['vaultx_io_write'].isna().all():
        ax3.plot(df.index, df['vaultx_io_write'], color='#d35400', linewidth=2.5, 
                label='VaultX Write (kB/s)', linestyle='--', alpha=0.8)
    
    ax3.set_ylabel('I/O Rate (kB/s)')
    ax3.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax3.grid(True, alpha=0.3)
    ax3.set_xlabel('Time')
    
    # Add stage markers to all subplots
    if stages:
        stage_colors = {
            'Table1Gen': '#a4c2f4ff',
            'WriteTable1': '#3c78d8ff',
            'InMemoryTable2Gen': '#ea9999ff',
            'OutOfMemoryTable2Gen': '#ea9999ff',
            'ReadTable1': '#df4242ff',
            'WriteTable2': '#e06666ff',
            'Table2Shuffle': '#ffe599ff',
            'FinalizePlotFile': '#ffff00ff',
            'Verification': '#ffd4aa'
        }
        
        axes = [ax1, ax2, ax3]
        stage_handles = {}
        
        for stage_name, stage_info in stages.items():
            color = stage_colors.get(stage_name, '#CCCCCC')
            alpha_value = 0.25
            
            # Add to first subplot only for legend
            span = ax1.axvspan(stage_info['start'], stage_info['end'], 
                             alpha=alpha_value, color=color, label=stage_name)
            stage_handles[stage_name] = span
            
            # Add to other subplots without labels
            for ax in axes[1:]:
                ax.axvspan(stage_info['start'], stage_info['end'], 
                          alpha=alpha_value, color=color)
        
        # Create stage legend
        if stage_handles:
            legend_handles = []
            legend_labels = []
            for stage_name, handle in stage_handles.items():
                legend_handles.append(handle)
                legend_labels.append(f'{stage_name} ({stages[stage_name]["duration"]:.1f}s)')
            
            fig.legend(handles=legend_handles, labels=legend_labels, 
                      loc='center right', bbox_to_anchor=(1.2, 0.5))
    
    # Add explanatory text about data collection method
    explanation_text = f'Note: System and VaultX metrics collected from single pidstat -p ALL.\nEnsures perfect time alignment. Monitoring overhead typically <1% CPU.'
    ax3.text(0.02, 0.98, explanation_text, 
             transform=ax3.transAxes, fontsize=10, verticalalignment='top',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
    
    plt.tight_layout()
    plt.subplots_adjust(right=0.82)
    plt.savefig(plot_file, dpi=300, bbox_inches='tight')
    print(f"Comparison plot saved to: {plot_file}")

def calculate_resource_differences(merged_data):
    """Calculate and display resource usage differences."""
    if not merged_data:
        return
    
    df = pd.DataFrame(merged_data)
    num_cores = os.cpu_count()
    
    print("\n--- Resource Usage Analysis ---")
    print(f"Note: System and VaultX values collected from single pidstat -p ALL for perfect alignment")
    
    # CPU Analysis
    if 'system_cpu_total' in df.columns and 'vaultx_cpu_total' in df.columns:
        system_cpu_avg = df['system_cpu_total'].mean()  # Aggregated from all processes
        vaultx_cpu_avg = df['vaultx_cpu_total'].mean()  # Raw per-core
        other_cpu_avg = max(0, system_cpu_avg - vaultx_cpu_avg)  # Ensure non-negative
        
        print(f"Average CPU Usage:")
        print(f"  System Total (all):  {system_cpu_avg:.1f}% (aggregated)")
        print(f"  VaultX (raw):        {vaultx_cpu_avg:.1f}%")
        print(f"  Other Procs:         {other_cpu_avg:.1f}%")
        
        # Calculate VaultX utilization
        if system_cpu_avg > 0:
            vaultx_share = (vaultx_cpu_avg / system_cpu_avg) * 100
            other_share = (other_cpu_avg / system_cpu_avg) * 100
            print(f"  VaultX Share:        {vaultx_share:.1f}% of total system CPU")
            print(f"  Other Share:         {other_share:.1f}% of total system CPU")
    
    # Memory Analysis
    if 'system_mem_rss_total' in df.columns and 'vaultx_mem_rss' in df.columns:
        system_mem_avg_mb = df['system_mem_rss_total'].mean() / 1024
        vaultx_mem_avg_mb = df['vaultx_mem_rss'].mean() / 1024
        vaultx_mem_max_mb = df['vaultx_mem_rss'].max() / 1024
        other_mem_avg_mb = max(0, system_mem_avg_mb - vaultx_mem_avg_mb)
        
        print(f"Memory Usage:")
        print(f"  System Total RSS:    {system_mem_avg_mb:.1f} MB (avg)")
        print(f"  VaultX RSS (avg):    {vaultx_mem_avg_mb:.1f} MB")
        print(f"  VaultX RSS (peak):   {vaultx_mem_max_mb:.1f} MB")
        print(f"  Other Processes:     {other_mem_avg_mb:.1f} MB (avg)")
        
        if system_mem_avg_mb > 0:
            vaultx_mem_share = (vaultx_mem_avg_mb / system_mem_avg_mb) * 100
            print(f"  VaultX Share:        {vaultx_mem_share:.1f}% of total system RSS")
    
    # I/O Analysis
    if 'system_io_read' in df.columns and 'vaultx_io_read' in df.columns:
        sys_read_total = df['system_io_read'].sum()
        vaultx_read_total = df['vaultx_io_read'].sum()
        sys_write_total = df['system_io_write'].sum() if 'system_io_write' in df.columns else 0
        vaultx_write_total = df['vaultx_io_write'].sum() if 'vaultx_io_write' in df.columns else 0
        
        print(f"I/O Activity (total kB):")
        print(f"  System Read:  {sys_read_total:.0f} kB")
        print(f"  VaultX Read:  {vaultx_read_total:.0f} kB")
        print(f"  System Write: {sys_write_total:.0f} kB")
        print(f"  VaultX Write: {vaultx_write_total:.0f} kB")
        
        if sys_read_total > 0:
            vaultx_read_share = (vaultx_read_total / sys_read_total) * 100
            print(f"  VaultX Read Share:   {vaultx_read_share:.1f}%")
        if sys_write_total > 0:
            vaultx_write_share = (vaultx_write_total / sys_write_total) * 100
            print(f"  VaultX Write Share:  {vaultx_write_share:.1f}%")
    
    # Process count analysis
    if 'system_process_count' in df.columns:
        avg_processes = df['system_process_count'].mean()
        max_processes = df['system_process_count'].max()
        min_processes = df['system_process_count'].min()
        
        print(f"Process Count:")
        print(f"  Average: {avg_processes:.1f}")
        print(f"  Range:   {min_processes:.0f} - {max_processes:.0f}")

def main():
    """Main function."""
    args = parse_arguments()
    
    if not args.vaultx_args:
        print("Error: No VaultX command specified")
        sys.exit(1)
    
    experiment_start_time = datetime.now()
    print(f"--- Experiment Starting at {experiment_start_time.strftime('%Y-%m-%d %H:%M:%S')} ---")
    
    # Get system memory info
    total_memory_kb = get_system_memory_info()
    if total_memory_kb:
        print(f"System memory: {total_memory_kb / 1024:.1f} MB")
    
    # Start VaultX process
    vaultx_cmd = args.vaultx_args
    print(f"Running command: {' '.join(vaultx_cmd)}")
    
    try:
        vaultx_process = subprocess.Popen(vaultx_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        text=True, bufsize=1)
    except FileNotFoundError:
        print(f"Error: VaultX executable not found: {vaultx_cmd[0]}")
        sys.exit(1)
    
    vaultx_pid = vaultx_process.pid
    print(f"--- VaultX started with PID: {vaultx_pid} ---")
    
    # Prepare data containers and stop event
    combined_data = []
    stage_events = []
    stop_event = threading.Event()
    
    # Define monitoring functions
    def monitor_combined_resources():
        nonlocal combined_data
        combined_data = run_combined_pidstat_all(vaultx_pid, args.interval, experiment_start_time, stop_event)
    
    def monitor_output():
        nonlocal stage_events
        stage_events = monitor_vaultx_output(vaultx_process, experiment_start_time)
    
    # Start monitoring threads
    combined_thread = threading.Thread(target=monitor_combined_resources)
    output_thread = threading.Thread(target=monitor_output)
    
    combined_thread.start()
    output_thread.start()
    
    # Wait for VaultX to complete
    vaultx_process.wait()
    print("\n--- VaultX execution completed ---")
    
    # Signal monitoring threads to stop
    stop_event.set()
    
    # Close VaultX stdout to signal EOF to output thread
    try:
        if vaultx_process.stdout:
            vaultx_process.stdout.close()
    except Exception:
        pass
    
    # Wait for all threads to finish
    output_thread.join()
    combined_thread.join(timeout=5)
    
    # Process results
    stages = process_stage_events(stage_events)
    merged_data = merge_data(combined_data)
    
    print(f"\n--- Performance Summary ---")
    print(f"Total execution time: {(datetime.now() - experiment_start_time).total_seconds():.2f} seconds")
    print(f"Stages detected: {len(stages)}")
    print(f"Combined data points: {len(combined_data)}")
    print(f"Final data points: {len(merged_data)}")
    
    if stages:
        for stage_name, stage_info in stages.items():
            print(f"  {stage_name}: {stage_info['duration']:.2f}s")
    
    # Parse K value from vaultx_args for plot title
    k_value = None
    for i, arg in enumerate(args.vaultx_args):
        if arg in ('-K', '--K') and i + 1 < len(args.vaultx_args):
            try:
                k_value = int(args.vaultx_args[i + 1])
            except ValueError:
                pass
    create_comparison_plot.k_value = k_value
    
    # Create comparison plot
    if merged_data:
        create_comparison_plot(stages, merged_data, args.plot_file, total_memory_kb)
        calculate_resource_differences(merged_data)
        
        # Save to CSV if requested
        if args.csv_output:
            df = pd.DataFrame(merged_data)
            df.to_csv(args.csv_output, index=False)
            print(f"Raw data saved to: {args.csv_output}")
    else:
        print("Insufficient data for plotting")

if __name__ == "__main__":
    main()
