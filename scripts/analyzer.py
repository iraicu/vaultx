#!/usr/bin/env python3

import argparse
import subprocess
import threading
import queue
import re
import sys
from datetime import datetime, timedelta
import pandas as pd

def stream_reader(pipe, q, label):
    """Reads a process's output stream line by line and puts it into a queue."""
    try:
        for line in iter(pipe.readline, ''):
            q.put((label, line))
    finally:
        pipe.close()

def parse_vaultx_log(lines, exp_start_time):
    """Parses vaultx output for stage markers and PID."""
    stage_starts = {}
    stages = {}
    vaultx_pid = None
    
    pid_pattern = re.compile(r"VAULTX_PID: (\d+)")
    marker_pattern = re.compile(r"VAULTX_STAGE_MARKER: (START|END) (\S+) ([\d.]+)")

    for line in lines:
        pid_match = pid_pattern.match(line)
        if pid_match:
            vaultx_pid = int(pid_match.group(1))
            print(f"--> Captured vaultx PID: {vaultx_pid}")
            continue

        marker_match = marker_pattern.match(line)
        if marker_match:
            event, name, elapsed_str = marker_match.groups()
            elapsed_seconds = float(elapsed_str)
            timestamp = exp_start_time + timedelta(seconds=elapsed_seconds)

            if event == "START":
                stage_starts[name] = timestamp
                print(f"--> Stage '{name}' started at {timestamp.strftime('%H:%M:%S')}")
            elif event == "END" and name in stage_starts:
                stages[name] = {
                    "start": stage_starts[name],
                    "end": timestamp,
                    "duration_s": (timestamp - stage_starts[name]).total_seconds()
                }
                print(f"--> Stage '{name}' ended at {timestamp.strftime('%H:%M:%S')} (Duration: {stages[name]['duration_s']:.2f}s)")
                del stage_starts[name]
    
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

def main():
    parser = argparse.ArgumentParser(
        description="Run vaultx, monitor with pidstat, and analyze resource usage per stage.",
        epilog="Example: ./analyzer.py -- ./vaultx -K 28 -m 512 -f ./plots/ -g ./plots/ -j ./plots/",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument('command', nargs=argparse.REMAINDER, help="The vaultx command to run. Must be preceded by '--'.")
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

    # Correlate and analyze
    results = analyze(stages, pidstat_data)

    # Print report
    df = pd.DataFrame.from_dict(results, orient='index')
    df.index.name = "Stage"
    df = df.rename(columns={
        "duration_s": "Duration (s)",
        "avg_cpu_total": "Avg CPU (%)",
        "avg_mem_rss_kb": "Avg RSS (KB)",
        "avg_io_read_kb_s": "Avg Read (KB/s)",
        "avg_io_write_kb_s": "Avg Write (KB/s)"
    })

    print("\n--- Resource Usage Report ---")
    with pd.option_context('display.max_rows', None,
                           'display.max_columns', None,
                           'display.width', 120,
                           'display.float_format', '{:,.2f}'.format):
        print(df)

if __name__ == "__main__":
    main()