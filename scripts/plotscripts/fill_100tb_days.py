#!/usr/bin/env python3
"""
Time (days) to accumulate the same number of K=32 records needed to fill a
hypothetical 100 TB Proof-of-Space reservation, derived directly from the
best per-plot times in chia_plotter_best_times.py.

Each Chia CPU plotter produces an uncompressed K=32 plot of ~102 GB, so
100 TB (100,000 GB) requires ceil(100000 / 102) = ~980.4 such plots, i.e.
~980.4 * 2^32 total records.
Bars grouped by plotter across 4 machines, same colors/order as
chia_plotter_best_times.py. Log-scale y-axis (values span ~6 to ~430 days).
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

CHIA_PLOT_SIZE_GB = 102.0
RESERVATION_GB    = 100_000.0   # 100 TB
PLOTS_NEEDED      = RESERVATION_GB / CHIA_PLOT_SIZE_GB   # ~980.39
MIN_PER_DAY       = 1440.0

# Best per-plot K=32 time (minutes), identical source data to
# chia_plotter_best_times.py -- converted here to days-to-fill-100TB.
GROUPS = [
    {
        "plotter": "Bladebit",
        "data": [
            ("8Socket",   9.33),
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
    "ChiaPOS":  "#D62728",
    "Madmax":   "#FF7F0E",
    "Bladebit": "#2CA02C",
}

BAR_W     = 0.8
N_BARS    = 4
GROUP_GAP = 2   # empty x-units between groups


def days_to_fill(time_min: float) -> float:
    return time_min * PLOTS_NEEDED / MIN_PER_DAY


def main():
    positions     = []
    group_centers = []
    offset        = 0
    for _ in GROUPS:
        xs = list(range(offset, offset + N_BARS))
        positions.append(xs)
        group_centers.append(float(np.mean(xs)))
        offset += N_BARS + GROUP_GAP

    fig, ax = plt.subplots(figsize=(12, 6))
    ax.set_yscale("log")

    for g_idx, group in enumerate(GROUPS):
        plotter = group["plotter"]
        color   = COLORS[plotter]
        xs      = positions[g_idx]

        for i, (_, t_min) in enumerate(group["data"]):
            xi   = xs[i]
            days = days_to_fill(t_min)
            label = f"{days:.1f}d" if days < 10 else f"{days:.0f}d"

            ax.bar(xi, days, color=color, width=BAR_W, zorder=3,
                   edgecolor="white", linewidth=0.5)
            ax.text(xi, days * 1.12, label,
                    ha="center", va="bottom", fontsize=8,
                    color="black", zorder=6)

    all_xs     = [x for grp_xs in positions for x in grp_xs]
    all_labels = [d[0] for grp in GROUPS for d in grp["data"]]
    ax.set_xticks(all_xs)
    ax.set_xticklabels(all_labels, fontsize=8.5)

    trans = mtransforms.blended_transform_factory(ax.transData, ax.transAxes)
    for g_idx, group in enumerate(GROUPS):
        ax.text(group_centers[g_idx], -0.13, group["plotter"],
                ha="center", va="top", fontsize=9.5, fontweight="bold",
                color=COLORS[group["plotter"]], transform=trans, clip_on=False)

    ax.set_xlim(-0.7, max(all_xs) + 0.7)
    ax.set_ylim(1, 1000)
    ax.set_ylabel("Time to fill a 100 TB reservation (days, log scale)", fontsize=10.5)
    ax.grid(axis="y", which="major", linestyle="--", alpha=0.4, zorder=0)
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
    out     = os.path.join(IMAGES_DIR, "fill_100tb_days.svg")
    out_png = os.path.join(IMAGES_DIR, "fill_100tb_days.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "fill_100tb_days.png")
    fig.savefig(out,     bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating time-to-fill-100TB comparison …")
    main()
    print("Done.")
