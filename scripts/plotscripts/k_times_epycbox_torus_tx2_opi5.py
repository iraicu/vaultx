#!/usr/bin/env python3
"""
K27-K32 in-memory plot times for 4 representative machines (EpycBox, Torus,
ThunderX2, OPI5), 2x2 grid, for Section IV-B (Vault Generation: Storage
Medium Comparison). A reduced view of k_times_by_drive.py's full 11-machine
figure -- same source CSVs and drive-selection rules, fewer machines.

Axis choice: all four machines share one symlog y-axis (linear 0-1, then log
1-10-100) so EpycBox/Torus/ThunderX2 (well under 10 min) and OPI5 (up to
~52 min) can be read off the same scale without OPI5 dwarfing the rest.

OPI5 K=32 caveat: OPI5 has 32GB RAM; a true K=32 in-memory run needs ~48GB
(same formula as Section III), so OPI5's K=32 point is not actually a full
in-memory run -- unlike every other bar in this figure. Evidence: its
peak_memory_mb (~25.5GB) barely moves from K=31 (~25.9GB) instead of
doubling, and its logged storage_efficiency_pct (198.90%) is impossible.
Per user direction, the data point is kept (not dropped) but visually
flagged: hatched bars + bold x-tick label + an in-panel note.
"""

import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.ticker
import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
from k_times_by_drive import (MACHINE_DRIVES, DRIVE_COLORS, DRIVE_LABELS,
                              DRIVE_ORDER, K_VALUES, load_machine_data)

IMAGES_DIR      = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
BASE_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(BASE_IMAGES_DIR, exist_ok=True)

MACHINES = ["epycbox", "torus", "thunderx2", "opi5"]
MACHINE_TITLES = {
    "epycbox": "EpycBox", "torus": "Torus",
    "thunderx2": "ThunderX2", "opi5": "OPI5",
}
YSCALE_LINTHRESH = 1.0                 # linear below 1 min, log above
YTICKS = [0, 1, 10, 100]
YLIM = (0, 100)
OPI5_MEMORY_LIMITED_K = 32            # 32GB RAM < ~48GB needed for true K=32 IM


def plot_machine(machine: str, ax: plt.Axes):
    drive_data = load_machine_data(machine)
    available_drives = [d for d in DRIVE_ORDER if d in drive_data]

    n_drives  = len(available_drives)
    n_k       = len(K_VALUES)
    bar_width = 0.8 / n_drives
    x         = np.arange(n_k)

    k32_idx = K_VALUES.index(32)
    for i, drive in enumerate(available_drives):
        df    = drive_data[drive]
        times = [df.loc[df["k"] == k, "total_time_min"].values[0]
                 if k in df["k"].values else 0.0
                 for k in K_VALUES]
        offset = (i - (n_drives - 1) / 2) * bar_width
        bars = ax.bar(x + offset, times, width=bar_width * 0.9,
                      color=DRIVE_COLORS[drive], label=DRIVE_LABELS[drive], zorder=3)

        if machine == "opi5":
            bars[k32_idx].set_hatch("xx")
            bars[k32_idx].set_edgecolor("black")
            bars[k32_idx].set_linewidth(0.6)

        k32_bar = bars[k32_idx]
        ax.text(k32_bar.get_x() + k32_bar.get_width() / 2,
                k32_bar.get_height(), f"{k32_bar.get_height():.1f}",
                ha="center", va="bottom", fontsize=5.5, zorder=4)

    ax.set_yscale("symlog", linthresh=YSCALE_LINTHRESH)
    ax.set_yticks(YTICKS)
    ax.set_yticklabels([str(t) for t in YTICKS])
    ax.set_ylim(*YLIM)
    ax.yaxis.set_minor_locator(matplotlib.ticker.NullLocator())

    ax.set_xticks(x)
    ax.set_xticklabels([f"k{k}" for k in K_VALUES], fontsize=8)
    if machine == "opi5":
        k32_idx = K_VALUES.index(OPI5_MEMORY_LIMITED_K)
        ax.get_xticklabels()[k32_idx].set_fontweight("bold")

    ax.tick_params(axis="y", labelsize=8)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.set_title(MACHINE_TITLES[machine], fontsize=12, fontweight="bold")
    ax.set_xlabel("K value", fontsize=9)
    ax.set_ylabel("Time (min)", fontsize=9)

    ax.text(0.02, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=7, ha="left", va="top",
            color="gray", style="italic")
    if machine == "opi5":
        ax.text(0.02, 0.88,
                "hatched k32: memory-limited,\nnot full in-memory (32GB RAM)",
                transform=ax.transAxes, fontsize=6.5, ha="left", va="top",
                color="black", style="italic")


def main():
    fig, axes = plt.subplots(2, 2, figsize=(11, 8.5))
    axes_flat = axes.flatten()

    for ax, machine in zip(axes_flat, MACHINES):
        plot_machine(machine, ax)

    drives_present = set()
    for machine in MACHINES:
        drives_present |= set(load_machine_data(machine).keys())
    legend_handles = [mpatches.Patch(color=DRIVE_COLORS[d], label=DRIVE_LABELS[d])
                      for d in DRIVE_ORDER if d in drives_present]
    fig.legend(handles=legend_handles, loc="lower center",
               ncol=len(legend_handles), fontsize=10, frameon=True,
               bbox_to_anchor=(0.5, -0.02))

    fig.tight_layout(rect=[0, 0.03, 1, 1])

    out_svg = os.path.join(BASE_IMAGES_DIR, "k_times_epycbox_torus_tx2_opi5.svg")
    out_png = os.path.join(BASE_IMAGES_DIR, "k_times_epycbox_torus_tx2_opi5.png")
    out_pp  = os.path.join(IMAGES_DIR, "k_times_epycbox_torus_tx2_opi5.png")
    fig.savefig(out_svg, bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out_svg}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating K27-K32 IM time comparison (EpycBox, Torus, ThunderX2, OPI5) …")
    main()
    print("Done.")
