"""
Speedup scaling plots for X86 and ARM architectures (side by side).
Each subplot shows how that architecture scales with thread count
relative to its own 1-thread baseline.

Missing ARM points (1, 2, 4, 8 threads) are estimated by fitting
Amdahl's law to the available ARM measurements.
"""

import numpy as np
import matplotlib.pyplot as plt
from scipy.optimize import curve_fit

# ---------------------------------------------------------------
# Raw measurements
# ---------------------------------------------------------------
x86_threads = np.array([1, 2, 4, 8, 16, 32, 64, 96, 128, 192, 256, 384])
x86_times   = np.array([89.48, 49.23, 27.72, 16.82, 8.94, 5.14,
                        3.21, 2.66, 2.42, 2.02, 1.95, 1.74])

arm_threads_meas = np.array([16, 32, 64, 96, 128, 192, 224])
arm_times_meas   = np.array([16.84, 9.78, 7.57, 6.53, 6.28, 5.84, 5.95])

# ---------------------------------------------------------------
# Fit Amdahl's law to the measured ARM points to estimate
# T(1), T(2), T(4), T(8).
#
#   T(n) = T(1) * ( s + (1 - s) / n )
#
# where s is the serial fraction.
# ---------------------------------------------------------------
def amdahl(n, T1, s):
    return T1 * (s + (1.0 - s) / n)

popt, _ = curve_fit(amdahl, arm_threads_meas, arm_times_meas,
                    p0=[200.0, 0.3], bounds=([1.0, 0.0], [1e5, 1.0]))
T1_arm, s_arm = popt
print(f"ARM Amdahl fit: T(1) = {T1_arm:.2f} min, serial fraction s = {s_arm:.4f}")

missing_threads = np.array([1, 2, 4, 8])
missing_times   = amdahl(missing_threads, *popt)

arm_threads = np.concatenate([missing_threads, arm_threads_meas])
arm_times   = np.concatenate([missing_times,   arm_times_meas])

# ---------------------------------------------------------------
# Speedup relative to each architecture's own 1-thread baseline
# ---------------------------------------------------------------
x86_speedup = x86_times[0] / x86_times
arm_speedup = arm_times[0] / arm_times

# ---------------------------------------------------------------
# Plot side by side
# ---------------------------------------------------------------
fig, axes = plt.subplots(1, 2, figsize=(14, 6), sharey=False)

def plot_panel(ax, threads, speedup, title, color):
    ideal = threads.astype(float)  # linear y = x
    ax.plot(threads, ideal, '--', color='gray', linewidth=1.4,
            label='Ideal (linear)')
    ax.plot(threads, speedup, 'o-', color=color, linewidth=2,
            markersize=7, label='Measured speedup')

    ax.set_xscale('log', base=2)
    ax.set_yscale('log', base=2)
    ax.set_xlabel('Threads')
    ax.set_ylabel('Speedup  (T₁ / Tₙ)')
    ax.set_title(title)
    ax.grid(True, which='both', linestyle=':', alpha=0.6)
    ax.legend(loc='upper left')

    # Annotate each point with its speedup value
    for t, s in zip(threads, speedup):
        ax.annotate(f'{s:.1f}×', (t, s),
                    textcoords='offset points', xytext=(6, -10),
                    fontsize=8, color=color)

plot_panel(axes[0], x86_threads, x86_speedup,
           'X86 — Speedup vs Threads', '#1f77b4')
plot_panel(axes[1], arm_threads, arm_speedup,
           'ARM — Speedup vs Threads', '#d62728')

fig.suptitle('Per-architecture Speedup Scaling (relative to 1-thread baseline)',
             fontsize=13, y=1.02)
fig.tight_layout()

out_path = '/home/claude/scaling/speedup_scaling.png'
fig.savefig(out_path, dpi=160, bbox_inches='tight')
print(f"Saved: {out_path}")

# ---------------------------------------------------------------
# Print a small table for reference
# ---------------------------------------------------------------
print("\nX86:")
print(f"{'threads':>8} {'time(min)':>10} {'speedup':>9} {'efficiency':>11}")
for t, tm, sp in zip(x86_threads, x86_times, x86_speedup):
    print(f"{t:>8d} {tm:>10.2f} {sp:>9.2f} {sp/t:>11.2%}")

print("\nARM:")
print(f"{'threads':>8} {'time(min)':>10} {'speedup':>9} {'efficiency':>11}")
for t, tm, sp in zip(arm_threads, arm_times, arm_speedup):
    print(f"{t:>8d} {tm:>10.2f} {sp:>9.2f} {sp/t:>11.2%}")
