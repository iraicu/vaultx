#!/usr/bin/env python3
"""
Best k32 times for Chia plotters (ChiaPOS, Madmax, Bladebit).
Bars grouped by plotter across 4 machines. All runs on NVMe. No title.
Y-axis in 60-minute intervals. Annotated with thread count and peak memory.
"""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.transforms as mtransforms
import numpy as np

SCRIPT_DIR       = os.path.dirname(os.path.abspath(__file__))
IMAGES_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "images")
PAPER_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

# (machine_label, time_min, threads, mem_gb)
# All runs on NVMe drives.
# Threads: full core count of the machine, or the count that gave the best time.
# Memory: configurable for Bladebit; reflects thread-count-dependent usage for Madmax/ChiaPOS.
GROUPS = [
    {
        "plotter": "Bladebit",
        "data": [
            ("8Socket",   9.42,  128, 416),
            ("Epycbox",  73.58,  128,  48),
            ("Torus",   104.8,    32,  48),
            ("OPI5",    201.2,     8,  30),
        ],
    },
    {
        "plotter": "Madmax",
        "data": [
            ("8Socket",  25.72,  32, 25.0),
            ("Epycbox",  54.45,  64, 41.0),
            ("Torus",    97.33,  32, 25.0),
            ("OPI5",    154.45,   4,  5.36),
        ],
    },
    {
        "plotter": "ChiaPOS",
        "data": [
            ("8Socket",  379.45, 192, 4.19),
            ("Epycbox",  448.45, 128, 3.92),
            ("Torus",    393.28,  32, 3.62),
            ("OPI5",     630.01,   8, 3.54),
        ],
    },
]

COLORS = {
    "ChiaPOS":  "#D62728",
    "Madmax":   "#FF7F0E",
    "Bladebit": "#2CA02C",
}

Y_MAX     = 660
Y_STEP    = 60
BAR_W     = 0.8
N_BARS    = 4
GROUP_GAP = 2   # empty x-units between groups


def fmt_mem(mem_gb):
    if mem_gb >= 10:
        return f"{round(mem_gb):d}GB"
    return f"{mem_gb:.1f}GB"


def main():
    # Build x positions per group
    positions     = []
    group_centers = []
    offset        = 0
    for _ in GROUPS:
        xs = list(range(offset, offset + N_BARS))
        positions.append(xs)
        group_centers.append(float(np.mean(xs)))
        offset += N_BARS + GROUP_GAP

    fig, ax = plt.subplots(figsize=(15, 6))

    for g_idx, group in enumerate(GROUPS):
        plotter = group["plotter"]
        color   = COLORS[plotter]
        xs      = positions[g_idx]

        for i, (machine, t, threads, mem_gb) in enumerate(group["data"]):
            xi  = xs[i]
            # Epycbox's 3-digit thread counts make "NNNT / MMGB" wider than the
            # bar itself, so stack thread count and memory on their own lines.
            stack_ann = (machine == "Epycbox")
            ann = f"{threads}T\n{fmt_mem(mem_gb)}" if stack_ann else f"{threads}T / {fmt_mem(mem_gb)}"
            time_str = f"{t:.2f}m" if t < 100 else f"{t:.1f}m"

            ax.bar(xi, t, color=color, width=BAR_W, zorder=3,
                   edgecolor="white", linewidth=0.5)

            if t < 50:
                # Small bar: all text above
                ax.text(xi, t + Y_MAX * 0.015,
                        f"{time_str}\n{ann}",
                        ha="center", va="bottom", fontsize=6.5,
                        color="black", zorder=6, linespacing=1.4)
            elif t < 120:
                # Medium bar: time above, specs inside
                ax.text(xi, t + Y_MAX * 0.013, time_str,
                        ha="center", va="bottom", fontsize=6.5,
                        color="black", zorder=6)
                ax.text(xi, t * 0.45, ann,
                        ha="center", va="center", fontsize=6.5,
                        color="white", fontweight="bold", zorder=5, linespacing=1.3)
            else:
                # Tall bar: everything inside
                ax.text(xi, t * 0.5, f"{time_str}\n{ann}",
                        ha="center", va="center", fontsize=7,
                        color="white", fontweight="bold", zorder=5, linespacing=1.5)

    # X-axis: machine names under each bar
    all_xs     = [x for grp_xs in positions for x in grp_xs]
    all_labels = [d[0] for grp in GROUPS for d in grp["data"]]
    ax.set_xticks(all_xs)
    ax.set_xticklabels(all_labels, fontsize=8.5)

    # Plotter group labels below x-axis tick labels
    trans = mtransforms.blended_transform_factory(ax.transData, ax.transAxes)
    for g_idx, group in enumerate(GROUPS):
        ax.text(group_centers[g_idx], -0.13, group["plotter"],
                ha="center", va="top", fontsize=9.5, fontweight="bold",
                color=COLORS[group["plotter"]], transform=trans, clip_on=False)

    ax.set_xlim(-0.7, max(all_xs) + 0.7)
    yticks = np.arange(0, Y_MAX + Y_STEP, Y_STEP)
    ax.set_yticks(yticks)
    ax.set_ylim(0, Y_MAX)
    ax.set_ylabel("Time (minutes)", fontsize=11)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=8, ha="right", va="top",
            color="gray", style="italic")

    legend_patches = [
        mpatches.Patch(color=COLORS["Bladebit"], label="Bladebit"),
        mpatches.Patch(color=COLORS["Madmax"],   label="Madmax"),
        mpatches.Patch(color=COLORS["ChiaPOS"],  label="ChiaPOS"),
    ]
    ax.legend(handles=legend_patches, fontsize=9, loc="upper left")

    fig.subplots_adjust(bottom=0.16)
    out     = os.path.join(IMAGES_DIR, "chia_plotter_best_times.svg")
    out_png = os.path.join(IMAGES_DIR, "chia_plotter_best_times.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "chia_plotter_best_times.png")
    fig.savefig(out,     bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating Chia plotter best times …")
    main()
    print("Done.")
