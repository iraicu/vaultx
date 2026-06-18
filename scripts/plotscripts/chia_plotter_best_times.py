#!/usr/bin/env python3
"""
Best k32 times for Chia plotters only (ChiaPOS, Madmax, Bladebit).
Bars sorted from lowest to highest time. No title.
Y-axis in 60-minute intervals to span from ~9 min to ~11 hrs.
Annotated with plotter, threads, peak memory.
"""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)

# (x_label, time_min, machine, threads, peak_mem_mb, plotter_type, drive_label)
# Sources:
#   s8 Bladebit ramplot:       bladebit_ramplot_nvme.csv t128 → 9.42 min, 426031 MB
#   s8 Madmax nvme-raid0:      varying_threads_k32_madmax_s8.csv t32 → 25.72 min, 25577 MB
#   epycbox Madmax nfs_nvme:   varying_threads_k32_madmax.csv t64 → 54.45 min, 42329 MB
#   epycbox Bladebit diskplot: bladebit_varying_memory_k32.csv 48G nfs_nvme → 73.58 min, 44116 MB
#   torus Madmax ssd-raid0:    varying_threads_k32_madmax_nvme.csv t32 → 97.33 min, 25231 MB
#   thunderx1 Madmax nfs_nvme: varying_threads_k32_madmax.csv t16 → 121.30 min, 13012 MB
#   s8 ChiaPOS nvme-raid0:     chiaposresults_nvme.txt → 6h19m45s = 379.75 min, 4288 MB, 192T
#   opi5 ChiaPOS data-fast:    posresults1.txt → 10h30m01s = 630.02 min, 3625 MB, 8T
# Drive labels per context.md (ssd-raid0 on torus = NVMe; data-fast on opi5 = NVME)
DATA = [
    ("Bladebit ramplot\n(s8)",        9.42,   "s8",        128, 426031, "Bladebit", "NVME"),
    ("Madmax\n(s8)",                 25.72,   "s8",         32,  25577, "Madmax",   "NVME"),
    ("Madmax\n(epycbox)",            54.45,   "epycbox",    64,  42329, "Madmax",   "NVME"),
    ("Bladebit diskplot\n(epycbox)", 73.58,   "epycbox",   128,  44116, "Bladebit", "NVME"),
    ("Madmax\n(torus)",              97.33,   "torus",      32,  25231, "Madmax",   "NVME"),
    ("Madmax\n(thunderx1)",         121.30,   "thunderx1",  16,  13012, "Madmax",   "NVME"),
    ("ChiaPOS\n(s8)",               379.75,   "s8",        192,   4288, "ChiaPOS",  "NVME"),
    ("ChiaPOS\n(opi5)",             630.02,   "opi5",        8,   3625, "ChiaPOS",  "NVME"),
]

COLORS = {
    "ChiaPOS":  "#D62728",   # red
    "Madmax":   "#FF7F0E",   # orange
    "Bladebit": "#2CA02C",   # green
}

Y_MAX  = 660   # 11 hrs in minutes
Y_STEP = 60


def main():
    n = len(DATA)
    x = np.arange(n)

    fig, ax = plt.subplots(figsize=(14, 6))

    for i, (lbl, t, machine, threads, mem_mb, plotter, drive) in enumerate(DATA):
        color  = COLORS[plotter]
        mem_gb = mem_mb / 1024.0
        ann    = f"{threads}T / {mem_gb:.0f}GB / {drive}"

        ax.bar(x[i], t, color=color, width=0.7, zorder=3,
               edgecolor="white", linewidth=0.5)

        if t < 30:
            # Very small bar: annotations above
            ax.text(x[i], t + Y_MAX * 0.013,
                    f"{t:.2f}m\n{ann}",
                    ha="center", va="bottom", fontsize=7, color="black",
                    zorder=6, linespacing=1.4)
        elif t < 150:
            # Medium bar: time above, specs inside
            ax.text(x[i], t + Y_MAX * 0.013, f"{t:.2f}m",
                    ha="center", va="bottom", fontsize=7, color="black", zorder=6)
            ax.text(x[i], t * 0.48, ann,
                    ha="center", va="center", fontsize=7,
                    color="white", fontweight="bold", zorder=5)
        else:
            # Tall bar: all inside
            ax.text(x[i], t * 0.5, f"{t:.1f}m\n{ann}",
                    ha="center", va="center", fontsize=7.5,
                    color="white", fontweight="bold", zorder=5)

    ax.set_xticks(x)
    ax.set_xticklabels([d[0] for d in DATA], fontsize=8.5, rotation=10, ha="right")
    yticks = np.arange(0, Y_MAX + Y_STEP, Y_STEP)
    ax.set_yticks(yticks)
    ax.set_ylim(0, Y_MAX)
    ax.set_ylabel("Time (minutes)", fontsize=11)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=8, ha="right", va="top",
            color="gray", style="italic")

    legend_patches = [
        mpatches.Patch(color=COLORS["ChiaPOS"],  label="ChiaPOS"),
        mpatches.Patch(color=COLORS["Madmax"],   label="Madmax"),
        mpatches.Patch(color=COLORS["Bladebit"], label="Bladebit"),
    ]
    ax.legend(handles=legend_patches, fontsize=9, loc="upper left")

    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, "chia_plotter_best_times.svg")
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


if __name__ == "__main__":
    print("Generating Chia plotter best times …")
    main()
    print("Done.")
