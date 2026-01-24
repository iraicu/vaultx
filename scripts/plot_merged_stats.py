import os
import re
import matplotlib.pyplot as plt

def parse_log_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # Parse Memory Size per Batch
    mem_match = re.search(r'Memory Size per Batch:\s*(\d+)(MB|GB)', content)
    memory_mb = 0
    if mem_match:
        val = float(mem_match.group(1))
        unit = mem_match.group(2)
        if unit == 'GB':
            memory_mb = val * 1024
        else:
            memory_mb = val
    else:
        print(f"Warning: Could not find Memory Size per Batch in {filepath}")

    # Parse Total Batches
    batches_match = re.search(r'Total Batches:\s*(\d+)', content)
    total_batches = 0
    if batches_match:
        total_batches = int(batches_match.group(1))
    else:
        print(f"Warning: Could not find Total Batches in {filepath}")

    # Parse Throughput
    # Look for the final summary line
    # [TIME] Completed merging ... total size SIZEGB
    summary_match = re.search(r'\[([\d\.]+)s\] Completed merging .* of total size ([\d\.]+)GB', content)
    throughput_mb_s = 0
    if summary_match:
        time_s = float(summary_match.group(1))
        size_gb = float(summary_match.group(2))
        if time_s > 0:
            throughput_mb_s = (size_gb * 1024) / time_s
    else:
        print(f"Warning: Could not find completion summary in {filepath}")

    # Parse Merge Time
    merge_time_match = re.search(r'Merge Time:\s*([\d\.]+)s', content)
    merge_time_s = 0
    if merge_time_match:
        merge_time_s = float(merge_time_match.group(1))
    else:
        # Fallback to the time in the summary match if explicit line not found
        if summary_match:
             merge_time_s = float(summary_match.group(1))
        else:
             print(f"Warning: Could not find Merge Time in {filepath}")

    return memory_mb, total_batches, throughput_mb_s, merge_time_s

def main():
    log_dir = './logs'
    k_values = range(33, 40) # 33 to 39
    
    mems = []
    batches = []
    throughputs = []
    merge_times = []
    ks = []

    for k in k_values:
        filename = f'merged_k{k}.txt'
        filepath = os.path.join(log_dir, filename)
        
        if os.path.exists(filepath):
            mem, batch, thpt, m_time = parse_log_file(filepath)
            ks.append(k)
            mems.append(mem)
            batches.append(batch)
            throughputs.append(thpt)
            merge_times.append(m_time)
            print(f"k={k}: Mem={mem}MB, Batches={batch}, Thpt={thpt:.2f}MB/s, Merge Time={m_time:.2f}s")
        else:
            print(f"File not found: {filepath}")

    if not ks:
        print("No data found.")
        return

    # Plotting
    fig, axes = plt.subplots(4, 1, figsize=(10, 20), sharex=True)
    
    # 1. Memory Size per Batch
    axes[0].bar(ks, mems, color='b', label='Memory Size per Batch (MB)')
    axes[0].set_ylabel('Memory Size (MB)')
    axes[0].set_title('Memory Size per Batch vs k')
    
    # 2. Total Batches
    axes[1].bar(ks, batches, color='g', label='Total Batches')
    axes[1].set_ylabel('Count')
    axes[1].set_title('Total Batches vs k')
    
    # 3. Average Throughput
    axes[2].bar(ks, throughputs, color='r', label='Avg Throughput (MB/s)')
    axes[2].set_ylabel('Throughput (MB/s)')
    axes[2].set_title('Average Throughput vs k')

    # 4. Merge Time
    axes[3].bar(ks, merge_times, color='m', label='Merge Time (s)')
    axes[3].set_xlabel('k')
    axes[3].set_ylabel('Merge Time (s)')
    axes[3].set_title('Merge Time vs k')

    plt.tight_layout()
    output_path = './graphs/merged_stats_k33_k39.png'
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    plt.savefig(output_path)
    print(f"Plot saved to {output_path}")

if __name__ == '__main__':
    main()
