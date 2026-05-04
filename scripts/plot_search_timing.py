"""
Plots three graphs from vaultx search timing data:
  1. Total Time/Lookup (ms) per k (k27-k32), command: -S 1 -D 3 -f <kfile> -t 1 -r 1
  2. Timing breakdown per lookup per k (File Open, Disk Seek, Disk Read, Record Hashing)
  3. Record Hashing time per lookup vs -r (k32 only, -r from 1-32)
"""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

# ---------------------------------------------------------------------------
# Data from: ./vaultx -S 1 -D 3 -f <kfile> -t 1 -r 1  (per-file runs)
# Full page cache flushed via sudo drop_caches before each run.
# TIMING format: file_open  disk_seek  disk_read  record_hashing  total  total
# ---------------------------------------------------------------------------
k_values = [27, 28, 29, 30, 31, 32]

timing_data = {
    27: dict(file_open=0.1198, disk_seek=1.1029, disk_read=0.5069, record_hash=0.0185, total=1.7481),
    28: dict(file_open=0.0537, disk_seek=0.8120, disk_read=0.6749, record_hash=0.0204, total=1.5609),
    29: dict(file_open=0.1301, disk_seek=1.2818, disk_read=0.6016, record_hash=0.0328, total=2.0463),
    30: dict(file_open=0.0784, disk_seek=1.3072, disk_read=0.6640, record_hash=0.0982, total=2.1479),
    31: dict(file_open=0.0824, disk_seek=0.8706, disk_read=0.5240, record_hash=0.0908, total=1.5678),
    32: dict(file_open=0.1462, disk_seek=1.2264, disk_read=0.6036, record_hash=0.1779, total=2.1541),
}

# ---------------------------------------------------------------------------
# k32 varying -r (powers of 2: 1,2,4,8,16,32): record hashing time/lookup (ms)
# Each run evicted from page cache via posix_fadvise(DONTNEED) before run.
# TIMING[3] = record_hashing wall-clock = avg/lookup (since -S 1)
# ---------------------------------------------------------------------------
r_values = [1, 2, 4, 8, 16, 32]
rh_per_r = [0.0763, 0.3831, 0.8311, 1.8376, 2.2597, 9.1886]

# ---------------------------------------------------------------------------
# Colours
# ---------------------------------------------------------------------------
COLORS = {
    'file_open':   '#4e79a7',
    'disk_seek':   '#f28e2b',
    'disk_read':   '#59a14f',
    'record_hash': '#e15759',
    'total':       '#76b7b2',
}

# ---------------------------------------------------------------------------
# Graph 1 – Total Time/Lookup per k
# ---------------------------------------------------------------------------
fig1, ax1 = plt.subplots(figsize=(8, 5))

totals = [timing_data[k]['total'] for k in k_values]
x = np.arange(len(k_values))
bars = ax1.bar([f'k{k}' for k in k_values], totals,
               color=COLORS['total'], edgecolor='white', linewidth=0.8)

for bar, val in zip(bars, totals):
    ax1.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.02,
             f'{val:.2f}', ha='center', va='bottom', fontsize=9)

ax1.set_xlabel('Plot size (k)', fontsize=11)
ax1.set_ylabel('Avg Time / Lookup (ms)', fontsize=11)
ax1.set_title('VaultX Search — Total Time per Lookup by k\n'
              '(-S 1 -D 3 -t 1 -r 1)', fontsize=12)
ax1.set_ylim(0, max(totals) * 1.18)
ax1.grid(axis='y', linestyle=':', alpha=0.6)
ax1.spines['top'].set_visible(False)
ax1.spines['right'].set_visible(False)

fig1.tight_layout()
fig1.savefig('graphs/search_time_per_lookup_by_k.png', dpi=150)
plt.close(fig1)
print("Saved: graphs/search_time_per_lookup_by_k.png")

# ---------------------------------------------------------------------------
# Graph 2 – Stacked breakdown per k
# ---------------------------------------------------------------------------
components = ['file_open', 'disk_seek', 'disk_read', 'record_hash']
labels     = ['File Open/Close', 'Disk Seek', 'Disk Read', 'Record Hashing']

fig2, ax2 = plt.subplots(figsize=(9, 5))

k_labels = [f'k{k}' for k in k_values]
bottoms  = np.zeros(len(k_values))

for comp, label in zip(components, labels):
    vals = np.array([timing_data[k][comp] for k in k_values])
    bars = ax2.bar(k_labels, vals, bottom=bottoms,
                   label=label, color=COLORS[comp], edgecolor='white', linewidth=0.5)
    # Annotate segments that are large enough to be readable
    for bar, v, b in zip(bars, vals, bottoms):
        if v > 0.04:
            ax2.text(bar.get_x() + bar.get_width() / 2, b + v / 2,
                     f'{v:.3f}', ha='center', va='center', fontsize=7.5,
                     color='white', fontweight='bold')
    bottoms += vals

ax2.set_xlabel('Plot size (k)', fontsize=11)
ax2.set_ylabel('Avg Time / Lookup (ms)', fontsize=11)
ax2.set_title('VaultX Search — Timing Breakdown per Lookup by k\n'
              '(-S 1 -D 3 -t 1 -r 1)', fontsize=12)
ax2.legend(loc='upper right', fontsize=9)
ax2.set_ylim(0, max(bottoms) * 1.15)
ax2.grid(axis='y', linestyle=':', alpha=0.6)
ax2.spines['top'].set_visible(False)
ax2.spines['right'].set_visible(False)

fig2.tight_layout()
fig2.savefig('graphs/search_timing_breakdown_by_k.png', dpi=150)
plt.close(fig2)
print("Saved: graphs/search_timing_breakdown_by_k.png")

# ---------------------------------------------------------------------------
# Graph 3 – k32 Record Hashing time vs -r
# ---------------------------------------------------------------------------
fig3, ax3 = plt.subplots(figsize=(10, 5))

ax3.plot(r_values, rh_per_r, 'o-', color=COLORS['record_hash'],
         linewidth=1.8, markersize=5, label='Record Hashing (wall-clock)')

# Mark r=1 baseline
ax3.axhline(rh_per_r[0], color='gray', linestyle='--', linewidth=1,
            label=f'r=1 baseline ({rh_per_r[0]:.3f} ms)')

ax3.set_xlabel('Number of record-hashing threads (-r)', fontsize=11)
ax3.set_ylabel('Record Hashing Time / Lookup (ms)', fontsize=11)
ax3.set_title('k32: Record Hashing Time per Lookup vs -r\n'
              '(-S 1 -D 3 -t 1, k32 file only)', fontsize=12)
ax3.set_xticks(r_values)
ax3.set_xticklabels([str(r) for r in r_values], fontsize=10)
ax3.legend(fontsize=9)
ax3.grid(linestyle=':', alpha=0.6)
ax3.spines['top'].set_visible(False)
ax3.spines['right'].set_visible(False)

fig3.tight_layout()
fig3.savefig('graphs/k32_record_hashing_vs_r.png', dpi=150)
plt.close(fig3)
print("Saved: graphs/k32_record_hashing_vs_r.png")
