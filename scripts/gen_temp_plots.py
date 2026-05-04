#!/usr/bin/env python3
"""
Temporary script to generate presentation-quality figures from REAL (validated) data only.
All output files are prefixed with 'temp_'.
Delete this script after use.

Usage: python3 scripts/gen_temp_plots.py
"""
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'Paper', 'images')
os.makedirs(OUTPUT_DIR, exist_ok=True)

plt.rcParams.update({
    'font.size': 11,
    'font.family': 'serif',
    'axes.labelsize': 11,
    'axes.titlesize': 12,
    'xtick.labelsize': 10,
    'ytick.labelsize': 10,
    'legend.fontsize': 9,
    'figure.dpi': 300,
    'savefig.bbox': 'tight',
    'savefig.pad_inches': 0.08,
})

COLORS = {
    'nvme':    '#2ca02c',
    'ssd':     '#ff7f0e',
    'hdd':     '#1f77b4',
    'nfs_hdd': '#9467bd',
    'ceph':    '#8c564b',
    'nfs_nvme':'#17becf',
}

MC = {
    '8Socket': '#1f77b4',
    'EpycBox': '#ff7f0e',
    'GPUBox':  '#2ca02c',
    'NVMeBox': '#d62728',
    'Athena':  '#9467bd',
    'Torus':   '#8c564b',
    'OPI5':    '#e377c2',
    'RPI5':    '#7f7f7f',
}

K_VALUES = [27, 28, 29, 30, 31, 32]


# REAL data (16 threads in-memory, minutes)

IM16 = {
    '8Socket': {
        'SSD':     [0.41, 0.69, 1.25, 2.32, 4.49,  9.16],
        'NVMe':    [0.39, 0.70, 1.22, 2.29, 4.64,  9.08],
        'NFS_HDD': [0.49, 0.84, 1.50, 2.96, 5.71, 12.29],
    },
    'EpycBox': {
        'HDD':     [0.61, 1.12, 2.23, 4.17,  8.15, 16.19],
        'SSD':     [0.51, 0.98, 1.78, 3.53,  6.76, 13.88],
        'NVMe':    [0.50, 0.92, 1.67, 3.31,  6.49, 12.54],
        'NFS_HDD': [0.61, 1.10, 2.07, 4.09,  7.93, 15.81],
        'Ceph':    [0.54, 0.96, 1.79, 3.52,  6.78, 13.39],
        'NFS_NVMe':[0.54, 0.97, 1.77, 3.47,  6.69, 13.10],
    },
    'GPUBox': {
        'HDD': [0.38, 0.69, 1.27, 2.39, 4.65, 9.17],
        'SSD': [0.32, 0.57, 1.01, 1.94, 3.76, 7.60],
    },
    'NVMeBox': {
        'HDD':  [0.37, 0.67, 1.28, 2.50, 4.95, 9.64],
        'SSD':  [0.36, 0.62, 1.12, 2.19, 4.29, 8.32],
        'NVMe': [0.31, 0.53, 0.96, 1.83, 3.62, 7.29],
    },
    'Torus': {
        'HDD':  [0.47, 0.85, 1.54, 2.99, 5.79, 11.43],
        'NVMe': [0.45, 0.77, 1.36, 2.53, 4.91, 10.04],
    },
    'Athena': {
        'HDD':  [0.42, 0.75, 1.41, 2.66, 5.25, 10.47],
        'NVMe': [0.35, 0.61, 1.10, 2.16, 4.21,  8.76],
    },
    'OPI5': {
        'HDD': [1.33, 2.46, 4.53, 8.55, 16.81, None],  # K32 not measured
    },
    'RPI5': {
        'HDD': [1.57, 2.89, 5.33, None, None, None],   # K30+ out of RAM
        'SSD': [1.54, 2.80, 5.13, None, None, None],
    },
}

# 32-thread in-memory data
IM32 = {
    '8Socket': {
        'SSD':     [0.26, 0.44, 0.74, 1.37, 2.61, 5.27],
        'NVMe':    [0.40, 0.68, 0.96, 1.91, 3.49, 5.14],
        'NFS_HDD': [0.52, 0.79, 1.38, 2.66, 4.71, 8.93],
    },
    'EpycBox': {
        'HDD':  [0.42, 0.75, 1.45, 2.85,  5.30, 10.38],
        'SSD':  [0.33, 0.57, 1.04, 1.96,  4.01,  8.08],
        'NVMe': [0.32, 0.56, 1.00, 1.94,  3.73,  7.48],
    },
    'GPUBox': {
        'HDD': [0.27, 0.47, 0.85, 1.61, 3.05, 6.08],
        'SSD': [0.21, 0.36, 0.63, 1.17, 2.18, 4.44],
    },
    'NVMeBox': {
        'HDD':  [0.27, 0.47, 0.86, 1.78, 3.55, 6.70],
        'SSD':  [0.23, 0.42, 0.75, 1.45, 2.77, 5.36],
        'NVMe': [0.20, 0.35, 0.60, 1.11, 2.13, 4.33],
    },
    'Torus': {
        'HDD':  [0.33, 0.59, 1.08, 2.13, 4.04, 7.83],
        'NVMe': [0.27, 0.46, 0.84, 1.63, 3.23, 6.50],
    },
    'Athena': {
        'HDD':  [0.31, 0.55, 1.02, 1.95, 3.75, 8.00],
        'NVMe': [0.25, 0.43, 0.73, 1.41, 2.84, 6.45],
    },
}

# K=32 thread scaling (IM, minutes)
THREAD_SCALING = {
    '8Socket (SSD)':   {'threads': [1,2,4,8,16,32,64,96,128,192,256,384],
                        'times':   [89.14,48.95,24.64,16.87,9.28,5.24,3.22,2.52,2.32,1.99,2.02,1.78]},
    '8Socket (NVMe)':  {'threads': [1,2,4,8,16,32,64,96,128,192,256,384],
                        'times':   [89.48,49.23,27.72,16.82,8.94,5.14,3.21,2.66,2.42,2.02,1.95,1.74]},
    'EpycBox (SSD)':   {'threads': [1,2,4,8,16,32,64,96,128],
                        'times':   [122.41,65.32,38.76,22.98,13.39,7.85,4.79,3.93,3.96]},
    'EpycBox (HDD)':   {'threads': [1,2,4,8,16,32,64,96,128],
                        'times':   [123.60,69.13,41.73,26.52,18.26,10.53,7.44,6.62,6.31]},
    'GPUBox (SSD)':    {'threads': [1,2,4,8,16,32,64,96],
                        'times':   [107.22,54.02,29.54,14.41,7.67,4.49,3.30,3.01]},
    'NVMeBox (NVMe)':  {'threads': [1,2,4,8,16,32,64],
                        'times':   [90.67,49.33,24.71,14.05,7.18,4.28,2.82]},
    'NVMeBox (HDD)':   {'threads': [1,2,4,8,16,32,64],
                        'times':   [93.59,52.18,27.04,16.52,9.68,6.60,5.22]},
    'Torus (NVMe)':    {'threads': [1,2,4,8,16,32],
                        'times':   [132.82,64.30,33.02,17.18,9.95,6.28]},
    'Athena (NVMe)':   {'threads': [1,2,4,8,16,32,48],
                        'times':   [127.16,62.10,32.21,15.64,8.80,6.21,5.76]},
}

# Search latency (ms) on EpycBox, K=32
SEARCH = {
    'VaultX':  {'NVMe SSD': (1.25, 2.31, 3.71),   'SATA SSD': (2.10, 31.86, 155.08),
                'SATA HDD': (33.83, 195.57, 393.89)},
    'ChiaPoS': {'NVMe SSD': (8.00, 17.14, 118.00), 'SATA SSD': (10.00, 87.28, 346.00),
                'SATA HDD': (224.00, 310.66, 411.00)},
}

# ──────────────────────────────────────────────────────────────────────────────
# FIGURE T1: K-value scaling — HDD only (fair comparison)
# ──────────────────────────────────────────────────────────────────────────────
def temp_kvalue_hdd():
    # Machines with HDD data; 8Socket uses NFS_HDD as HDD proxy
    hdd_data = {
        '8Socket*':  IM16['8Socket']['NFS_HDD'],
        'EpycBox':   IM16['EpycBox']['HDD'],
        'GPUBox':    IM16['GPUBox']['HDD'],
        'NVMeBox':   IM16['NVMeBox']['HDD'],
        'Athena':    IM16['Athena']['HDD'],
        'Torus':     IM16['Torus']['HDD'],
        'OPI5':      IM16['OPI5']['HDD'],
        'RPI5':      IM16['RPI5']['HDD'],
    }
    machines = list(hdd_data.keys())
    colors = [MC.get(m.rstrip('*'), '#333333') for m in machines]
    n_machines = len(machines)
    x = np.arange(len(K_VALUES))
    width = 0.10
    offsets = np.linspace(-(n_machines-1)/2, (n_machines-1)/2, n_machines) * width

    fig, ax = plt.subplots(figsize=(7, 3.5))
    for i, (mach, color) in enumerate(zip(machines, colors)):
        vals = hdd_data[mach]
        x_plot = []
        y_plot = []
        for ki, v in enumerate(vals):
            if v is not None:
                x_plot.append(x[ki] + offsets[i])
                y_plot.append(v)
        ax.bar(x_plot, y_plot, width, label=mach, color=color, alpha=0.85)

    ax.set_ylabel('Time (minutes)')
    ax.set_xlabel('K Value')
    ax.set_xticks(x)
    ax.set_xticklabels(K_VALUES)
    ax.set_title('K=27–32 In-Memory Generation: HDD (16 threads)\n* 8Socket uses NFS-HDD')
    ax.legend(ncol=4, fontsize=8, loc='upper left')
    ax.grid(axis='y', linestyle='--', alpha=0.35)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'temp_kvalue_hdd_16t.png'))
    plt.close()
    print("Saved: temp_kvalue_hdd_16t.png")


# ──────────────────────────────────────────────────────────────────────────────
# FIGURE T2: K-value scaling — best fast storage (NVMe/SSD)
# ──────────────────────────────────────────────────────────────────────────────
def temp_kvalue_fast():
    fast_data = {
        '8Socket (NVMe)':  IM16['8Socket']['NVMe'],
        'EpycBox (NVMe)':  IM16['EpycBox']['NVMe'],
        'GPUBox (SSD)':    IM16['GPUBox']['SSD'],
        'NVMeBox (NVMe)':  IM16['NVMeBox']['NVMe'],
        'Athena (NVMe)':   IM16['Athena']['NVMe'],
        'Torus (NVMe)':    IM16['Torus']['NVMe'],
    }
    machines = list(fast_data.keys())
    base_mc = {'8Socket (NVMe)': MC['8Socket'], 'EpycBox (NVMe)': MC['EpycBox'],
               'GPUBox (SSD)': MC['GPUBox'], 'NVMeBox (NVMe)': MC['NVMeBox'],
               'Athena (NVMe)': MC['Athena'], 'Torus (NVMe)': MC['Torus']}
    n = len(machines)
    x = np.arange(len(K_VALUES))
    width = 0.13
    offsets = np.linspace(-(n-1)/2, (n-1)/2, n) * width

    fig, ax = plt.subplots(figsize=(7, 3.5))
    for i, mach in enumerate(machines):
        vals = fast_data[mach]
        ax.bar(x + offsets[i], vals, width, label=mach, color=base_mc[mach], alpha=0.85)

    ax.set_ylabel('Time (minutes)')
    ax.set_xlabel('K Value')
    ax.set_xticks(x)
    ax.set_xticklabels(K_VALUES)
    ax.set_title('K=27–32 In-Memory Generation: NVMe/SSD (16 threads)')
    ax.legend(ncol=3, fontsize=8, loc='upper left')
    ax.grid(axis='y', linestyle='--', alpha=0.35)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'temp_kvalue_fast_16t.png'))
    plt.close()
    print("Saved: temp_kvalue_fast_16t.png")


# ──────────────────────────────────────────────────────────────────────────────
# FIGURE T3: K=32 Thread Scaling (log-log)
# ──────────────────────────────────────────────────────────────────────────────
def temp_thread_scaling():
    # Show key machines: 8Socket, EpycBox, GPUBox, NVMeBox (one drive each for clarity)
    show = {
        '8Socket (SSD)':  THREAD_SCALING['8Socket (SSD)'],
        'EpycBox (SSD)':  THREAD_SCALING['EpycBox (SSD)'],
        'GPUBox (SSD)':   THREAD_SCALING['GPUBox (SSD)'],
        'NVMeBox (NVMe)': THREAD_SCALING['NVMeBox (NVMe)'],
        'Torus (NVMe)':   THREAD_SCALING['Torus (NVMe)'],
        'Athena (NVMe)':  THREAD_SCALING['Athena (NVMe)'],
    }
    colors_map = {
        '8Socket (SSD)': MC['8Socket'], 'EpycBox (SSD)': MC['EpycBox'],
        'GPUBox (SSD)': MC['GPUBox'], 'NVMeBox (NVMe)': MC['NVMeBox'],
        'Torus (NVMe)': MC['Torus'], 'Athena (NVMe)': MC['Athena'],
    }
    markers = ['o', 's', '^', 'D', 'v', 'P']

    fig, ax = plt.subplots(figsize=(5, 4))
    for i, (label, d) in enumerate(show.items()):
        ax.plot(d['threads'], d['times'], marker=markers[i], color=colors_map[label],
                label=label, markersize=4, linewidth=1.4)

    # Ideal reference
    t0 = 100
    threads_ref = [1, 2, 4, 8, 16, 32, 64, 128, 256, 384]
    ax.plot(threads_ref, [t0/t for t in threads_ref], 'k--', alpha=0.25, linewidth=1,
            label='Ideal scaling')

    ax.set_xscale('log', base=2)
    ax.set_yscale('log')
    ax.set_xlabel('Compute Threads')
    ax.set_ylabel('Time (minutes)')
    ax.set_title('K=32 In-Memory: Thread Scaling')
    ax.legend(fontsize=7.5, loc='upper right')
    ax.grid(True, which='both', linestyle='--', alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'temp_thread_scaling_k32.png'))
    plt.close()
    print("Saved: temp_thread_scaling_k32.png")


# ──────────────────────────────────────────────────────────────────────────────
# FIGURE T4: Parallel Speedup K=32
# ──────────────────────────────────────────────────────────────────────────────
def temp_speedup():
    show = {
        '8Socket (384c)':  THREAD_SCALING['8Socket (SSD)'],
        'EpycBox (128c)':  THREAD_SCALING['EpycBox (SSD)'],
        'GPUBox (96c)':    THREAD_SCALING['GPUBox (SSD)'],
        'NVMeBox (64c)':   THREAD_SCALING['NVMeBox (NVMe)'],
    }
    colors_map = {
        '8Socket (384c)': MC['8Socket'], 'EpycBox (128c)': MC['EpycBox'],
        'GPUBox (96c)': MC['GPUBox'], 'NVMeBox (64c)': MC['NVMeBox'],
    }
    markers = ['o', 's', '^', 'D']

    fig, ax = plt.subplots(figsize=(4.5, 3.8))
    for i, (label, d) in enumerate(show.items()):
        t0 = d['times'][0]
        speedup = [t0 / t for t in d['times']]
        ax.plot(d['threads'], speedup, marker=markers[i], color=colors_map[label],
                label=label, markersize=4, linewidth=1.4)

    max_t = 400
    thr_ideal = [1, 2, 4, 8, 16, 32, 64, 128, 256, 384]
    ax.plot(thr_ideal, thr_ideal, 'k--', alpha=0.25, linewidth=1, label='Ideal')

    ax.set_xscale('log', base=2)
    ax.set_yscale('log', base=2)
    ax.set_xlabel('Compute Threads')
    ax.set_ylabel('Speedup')
    ax.set_title('K=32 In-Memory: Parallel Speedup')
    ax.legend(fontsize=8, loc='upper left')
    ax.grid(True, which='both', linestyle='--', alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'temp_speedup_k32.png'))
    plt.close()
    print("Saved: temp_speedup_k32.png")


# ──────────────────────────────────────────────────────────────────────────────
# FIGURE T5: Per-machine storage media comparison (K=27-32, 16t)
# ──────────────────────────────────────────────────────────────────────────────
def temp_storage_machine(machine, drives_dict, title, fname):
    x = np.arange(len(K_VALUES))
    drive_styles = {
        'HDD':     ('o-', COLORS['hdd']),
        'SSD':     ('s-', COLORS['ssd']),
        'NVMe':    ('^-', COLORS['nvme']),
        'NFS_HDD': ('v--', COLORS['nfs_hdd']),
        'Ceph':    ('D--', COLORS['ceph']),
        'NFS_NVMe':('P--', COLORS['nfs_nvme']),
    }
    fig, ax = plt.subplots(figsize=(5, 3.6))
    for drive, vals in drives_dict.items():
        style, color = drive_styles.get(drive, ('o-', '#333333'))
        # Filter out None values
        x_valid = [x[i] for i, v in enumerate(vals) if v is not None]
        y_valid = [v for v in vals if v is not None]
        kv_valid = [K_VALUES[i] for i, v in enumerate(vals) if v is not None]
        if x_valid:
            ax.plot(x_valid, y_valid, style, color=color, label=drive, markersize=5, linewidth=1.5)

    ax.set_xlabel('K Value')
    ax.set_ylabel('Time (minutes)')
    ax.set_title(title)
    ax.set_xticks(x)
    ax.set_xticklabels(K_VALUES)
    ax.legend(fontsize=8)
    ax.grid(axis='y', linestyle='--', alpha=0.35)
    ax.set_yscale('log')
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, fname))
    plt.close()
    print(f"Saved: {fname}")


# ──────────────────────────────────────────────────────────────────────────────
# FIGURE T6: K=32 machine comparison bar chart (16t and 32t side-by-side)
# ──────────────────────────────────────────────────────────────────────────────
def temp_machine_comparison_k32():
    # K=32 generation times, best available drive per machine
    # 16 threads
    k32_16t = {
        '8Socket':  9.08,   # NVMe
        'EpycBox':  12.54,  # NVMe
        'GPUBox':   7.60,   # SSD
        'NVMeBox':  7.29,   # NVMe
        'Athena':   8.76,   # NVMe
        'Torus':    10.04,  # NVMe
    }
    # 32 threads
    k32_32t = {
        '8Socket':  5.14,   # NVMe
        'EpycBox':  7.48,   # NVMe
        'GPUBox':   4.44,   # SSD
        'NVMeBox':  4.33,   # NVMe
        'Athena':   6.45,   # NVMe
        'Torus':    6.50,   # NVMe
    }

    machines = list(k32_16t.keys())
    x = np.arange(len(machines))
    width = 0.35
    colors = [MC[m] for m in machines]

    fig, ax = plt.subplots(figsize=(6, 4))
    bars1 = ax.bar(x - width/2, [k32_16t[m] for m in machines], width,
                   label='16 threads', alpha=0.85, color=colors, edgecolor='white', linewidth=0.5)
    bars2 = ax.bar(x + width/2, [k32_32t[m] for m in machines], width,
                   label='32 threads', alpha=0.55, color=colors, edgecolor='black', linewidth=0.8,
                   hatch='//')

    # Value labels
    for bar in bars1:
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.15,
                f'{bar.get_height():.1f}', ha='center', va='bottom', fontsize=7.5)
    for bar in bars2:
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.15,
                f'{bar.get_height():.1f}', ha='center', va='bottom', fontsize=7.5)

    ax.set_ylabel('Time (minutes)')
    ax.set_xticks(x)
    ax.set_xticklabels(machines, rotation=15, ha='right')
    ax.set_title('K=32 In-Memory Generation: Best Drive (16 vs 32 threads)')
    ax.legend(fontsize=9)
    ax.grid(axis='y', linestyle='--', alpha=0.35)
    ax.set_ylim(0, 15)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'temp_machine_comparison_k32.png'))
    plt.close()
    print("Saved: temp_machine_comparison_k32.png")


# ──────────────────────────────────────────────────────────────────────────────
# FIGURE T7: VaultX vs BladeBit K=32 (only validated data)
# ──────────────────────────────────────────────────────────────────────────────
def temp_vaultx_vs_chia():
    # Only validated data: EpycBox HDD
    machines = ['EpycBox (HDD)']
    vaultx  = [6.31]   # 128 threads, IM
    bladebit= [17.92]  # BladeBit IM

    x = np.arange(len(machines))
    width = 0.3

    fig, ax = plt.subplots(figsize=(4, 3.5))
    ax.bar(x - width/2, vaultx,   width, label='VaultX (128t, IM)',    color='#2ca02c')
    ax.bar(x + width/2, bladebit, width, label='BladeBit (IM)',        color='#d62728')

    for xi, (v, b) in enumerate(zip(vaultx, bladebit)):
        ax.text(xi - width/2, v + 0.3, f'{v:.2f}', ha='center', fontsize=9)
        ax.text(xi + width/2, b + 0.3, f'{b:.2f}', ha='center', fontsize=9)
        ax.annotate(f'{b/v:.1f}×\nfaster', xy=(xi, (v+b)/2),
                    ha='center', fontsize=9, color='black',
                    arrowprops=None)

    ax.set_ylabel('Time (minutes)')
    ax.set_xticks(x)
    ax.set_xticklabels(machines)
    ax.set_title('K=32 Generation: VaultX vs. BladeBit')
    ax.legend(fontsize=8)
    ax.grid(axis='y', linestyle='--', alpha=0.35)
    ax.set_ylim(0, 22)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'temp_vaultx_vs_bladebit.png'))
    plt.close()
    print("Saved: temp_vaultx_vs_bladebit.png")


# ──────────────────────────────────────────────────────────────────────────────
# FIGURE T8: Search latency comparison (VaultX vs Chia)
# ──────────────────────────────────────────────────────────────────────────────
def temp_search_latency():
    drives = ['NVMe SSD', 'SATA SSD', 'SATA HDD']
    vaultx_avg = [SEARCH['VaultX'][d][1]  for d in drives]
    chia_avg   = [SEARCH['ChiaPoS'][d][1] for d in drives]

    x = np.arange(len(drives))
    width = 0.32

    fig, ax = plt.subplots(figsize=(5, 3.8))
    ax.bar(x - width/2, vaultx_avg, width, label='VaultX',  color='#2ca02c')
    ax.bar(x + width/2, chia_avg,   width, label='ChiaPoS', color='#d62728')

    for i, (v, c) in enumerate(zip(vaultx_avg, chia_avg)):
        ax.text(i - width/2, v * 1.25, f'{v:.1f}', ha='center', fontsize=8)
        ax.text(i + width/2, c * 1.25, f'{c:.1f}', ha='center', fontsize=8)

    ax.set_ylabel('Avg Latency (ms)')
    ax.set_xticks(x)
    ax.set_xticklabels(drives, fontsize=9)
    ax.set_title('K=32 Search Latency: VaultX vs. ChiaPoS (EpycBox)')
    ax.legend(fontsize=9)
    ax.grid(axis='y', linestyle='--', alpha=0.4)
    ax.set_yscale('log')
    ax.set_ylim(top=1000)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'temp_search_latency.png'))
    plt.close()
    print("Saved: temp_search_latency.png")


# ──────────────────────────────────────────────────────────────────────────────
# FIGURE T9: HDD vs NVMe across K-values (16t) — 2 representative machines
# ──────────────────────────────────────────────────────────────────────────────
def temp_hdd_vs_nvme_epyc_nvmebox():
    fig, axes = plt.subplots(1, 2, figsize=(8, 3.5))

    for ax, machine, title in [
        (axes[0], 'EpycBox', 'EpycBox: Storage Media (16t IM)'),
        (axes[1], 'NVMeBox', 'NVMeBox: Storage Media (16t IM)'),
    ]:
        drives = IM16[machine]
        x = np.arange(len(K_VALUES))
        kv = K_VALUES
        for drive, vals in drives.items():
            style_map = {
                'HDD': ('o-', COLORS['hdd']),
                'SSD': ('s-', COLORS['ssd']),
                'NVMe': ('^-', COLORS['nvme']),
                'NFS_HDD': ('v--', COLORS['nfs_hdd']),
                'Ceph': ('D--', COLORS['ceph']),
                'NFS_NVMe': ('P--', COLORS['nfs_nvme']),
            }
            s, c = style_map.get(drive, ('o-', '#333'))
            x_v = [x[i] for i, v in enumerate(vals) if v is not None]
            y_v = [v for v in vals if v is not None]
            if x_v:
                ax.plot(x_v, y_v, s, color=c, label=drive, markersize=4, linewidth=1.4)
        ax.set_xlabel('K Value')
        ax.set_ylabel('Time (minutes)')
        ax.set_title(title)
        ax.set_xticks(x)
        ax.set_xticklabels(kv)
        ax.legend(fontsize=7.5)
        ax.grid(axis='y', linestyle='--', alpha=0.35)
        ax.set_yscale('log')

    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'temp_storage_epyc_nvmebox.png'))
    plt.close()
    print("Saved: temp_storage_epyc_nvmebox.png")


if __name__ == '__main__':
    print(f"Generating temp figures to: {os.path.abspath(OUTPUT_DIR)}")

    temp_kvalue_hdd()
    temp_kvalue_fast()
    temp_thread_scaling()
    temp_speedup()

    # Per-machine storage comparison
    temp_storage_machine('EpycBox',
        {k: IM16['EpycBox'][k] for k in ['HDD','SSD','NVMe','NFS_HDD','Ceph','NFS_NVMe']},
        'EpycBox K=27–32: Storage Media (16t, IM)',
        'temp_storage_epycbox.png')
    temp_storage_machine('GPUBox',
        IM16['GPUBox'],
        'GPUBox K=27–32: HDD vs SSD (16t, IM)',
        'temp_storage_gpubox.png')
    temp_storage_machine('NVMeBox',
        IM16['NVMeBox'],
        'NVMeBox K=27–32: HDD vs SSD vs NVMe (16t, IM)',
        'temp_storage_nvmebox.png')
    temp_storage_machine('Torus',
        IM16['Torus'],
        'Torus K=27–32: HDD vs NVMe (16t, IM)',
        'temp_storage_torus.png')
    temp_storage_machine('Athena',
        IM16['Athena'],
        'Athena K=27–32: HDD vs NVMe (16t, IM)',
        'temp_storage_athena.png')

    temp_machine_comparison_k32()
    temp_vaultx_vs_chia()
    temp_search_latency()
    temp_hdd_vs_nvme_epyc_nvmebox()

    print("\nAll temp figures generated. Delete this script when done.")
