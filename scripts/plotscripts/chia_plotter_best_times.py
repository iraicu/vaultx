#!/usr/bin/env python3
"""
Best k32 times for Chia plotters (ChiaPOS, Madmax, Bladebit).
Bars grouped by plotter across 4 machines. All runs on NVMe. No title.
Y-axis in 60-minute intervals. Bars annotated with time only (rounded
to the nearest whole minute); thread count and memory config are the
best for each machine/plotter and are documented in the paper instead.
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

# (machine_label, time_min)
# All runs on NVMe drives, using the best thread count and memory
# configuration found for each machine/plotter (documented in the paper).
GROUPS = [
    {
        "plotter": "VX",
        "data": [
            ("8Socket",   1.77),
            ("Epycbox",  3.77),
            ("Torus",   6.4),
            ("OPI5",    33.3),
        ],
    },
    {
        "plotter": "Bladebit",
        "data": [
            ("8Socket",   9.42),
            ("Epycbox",  73.58),
            ("Torus",   104.8),
            ("OPI5",    201.2),
        ],
    },
    {
        "plotter": "Madmax",
        "data": [
            ("8Socket",  25.72),
            ("Epycbox",  54.45),
            ("Torus",    97.33),
            ("OPI5",    154.45),
        ],
    },
    {
        "plotter": "ChiaPOS",
        "data": [
            ("8Socket",  379.45),
            ("Epycbox",  448.45),
            ("Torus",    393.28),
            ("OPI5",     630.01),
        ],
    },
]

COLORS = {
    "VX":       "#1F77B4",
    "ChiaPOS":  "#D62728",
    "Madmax":   "#FF7F0E",
    "Bladebit": "#2CA02C",
}

Y_MAX     = 660
Y_STEP    = 60
BAR_W     = 0.8
N_BARS    = 4
GROUP_GAP = 2   # empty x-units between groups


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

        for i, (_, t) in enumerate(group["data"]):
            xi  = xs[i]
            time_str = f"{round(t)}m"

            ax.bar(xi, t, color=color, width=BAR_W, zorder=3,
                   edgecolor="white", linewidth=0.5)

            if t < 120:
                # Small/medium bar: text above
                ax.text(xi, t + Y_MAX * 0.015, time_str,
                        ha="center", va="bottom", fontsize=8,
                        color="black", zorder=6)
            else:
                # Tall bar: text inside
                ax.text(xi, t * 0.5, time_str,
                        ha="center", va="center", fontsize=9,
                        color="white", fontweight="bold", zorder=5)

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
        mpatches.Patch(color=COLORS["VX"],       label="VX"),
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
