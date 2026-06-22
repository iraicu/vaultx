#!/usr/bin/env python3
"""
Task 6: Compare merge-to-k32 times on s8 vs opi5, per drive type.
Source drive is always NVME (nvme-raid0 for s8, data-fast for opi5).
Destination drive varies per plot (NVME, HDD, SSD, CEPH).
X-axis: K value being merged (k27–k31) with number of files needed (32 → 2).
4 individual plots + 1 combined.
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Results")
IMAGES_DIR      = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
BASE_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(BASE_IMAGES_DIR, exist_ok=True)

# Naming: source-destination (temp drive - final drive)
# s8 source: nvme-raid0   |  opi5 source: data-fast
DRIVE_CSVS = {
    "NVME": {
        "s8":   "s8/k27-k32merge_nvme-raid0-nvme-raid0.csv",
        "opi5": "opi5/k27-k32merge_data-fast-data-fast.csv",
    },
    "HDD": {
        "s8":   "s8/k27-k32merge_nvme-raid0-nfs_hdd.csv",    # nfs_hdd faster than data-i
        "opi5": "opi5/k27-k32merge_data-fast-data-a.csv",    # data-a faster than nfs_hdd
    },
    "SSD": {
        "s8":   "s8/k27-k32merge_nvme-raid0-ssd-raid0.csv",
        # opi5 has no SSD
    },
    "CEPH_HDD": {
        "s8":   "s8/k27-k32merge_nvme-raid0-ceph.csv",
        "opi5": "opi5/k27-k32merge_data-fast-ceph.csv",
    },
}

DRIVE_ORDER  = ["HDD", "CEPH_HDD", "SSD", "NVME"]
DRIVE_TITLES = {
    "HDD":      "NVME → HDD",
    "CEPH_HDD": "NVME → CEPH",
    "SSD":      "NVME → SSD",
    "NVME":     "NVME → NVME",
}

# X-axis order: from fewest files to most files (K=31 → K=27)
K_ORDER = [31, 30, 29, 28, 27]
N_MAP   = {31: 2, 30: 4, 29: 8, 28: 16, 27: 32}

MACHINE_COLORS = {"s8": "#1F77B4", "opi5": "#2CA02C"}  # blue, green
MACHINE_LABELS = {"s8": "s8 (384T / 770GB)", "opi5": "opi5 (8T / 32GB)"}


def load_df(rel_path: str) -> pd.DataFrame | None:
    full = os.path.join(RESULTS_DIR, rel_path)
    return pd.read_csv(full) if os.path.exists(full) else None


def get_times(df: pd.DataFrame) -> list[float]:
    """Return total_time_min for each K in K_ORDER; 0 if missing."""
    times = []
    for k in K_ORDER:
        row = df[df["K"] == k]
        times.append(float(row["total_time_min"].values[0]) if len(row) else 0.0)
    return times


def plot_drive(drive: str, ax: plt.Axes, show_legend: bool = True):
    machine_dfs = {}
    for machine, rel_path in DRIVE_CSVS.get(drive, {}).items():
        df = load_df(rel_path)
        if df is not None:
            machine_dfs[machine] = df

    x      = np.arange(len(K_ORDER))
    n_mach = len(machine_dfs)
    bw     = 0.7 / max(n_mach, 1)
    max_t  = 0.0

    for i, (machine, df) in enumerate(machine_dfs.items()):
        times  = get_times(df)
        offset = (i - (n_mach - 1) / 2) * bw
        ax.bar(x + offset, times, width=bw * 0.9,
               color=MACHINE_COLORS[machine], label=MACHINE_LABELS[machine],
               zorder=3)
        max_t = max(max_t, max(times))

    xtick_labels = [f"k{k}\n({N_MAP[k]} files)" for k in K_ORDER]
    ax.set_xticks(x)
    ax.set_xticklabels(xtick_labels, fontsize=8)
    ax.set_xlabel("Merge source K (files needed for k32)", fontsize=9)
    ax.set_ylabel("Total time (min)", fontsize=9)
    ax.set_title(DRIVE_TITLES[drive], fontsize=11, fontweight="bold")
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=6.5, ha="right", va="top",
            color="gray", style="italic")
    if show_legend and machine_dfs:
        ax.legend(fontsize=8)

    if not machine_dfs:
        ax.text(0.5, 0.5, "No data", transform=ax.transAxes,
                ha="center", va="center", color="gray")


def save_individual(drive: str):
    fig, ax = plt.subplots(figsize=(7, 5))
    plot_drive(drive, ax, show_legend=True)
    fig.tight_layout()
    out = os.path.join(IMAGES_DIR,
                       f"merge_k27k32_{drive.lower()}_s8_vs_opi5.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


def save_combined():
    fig, axes = plt.subplots(1, 4, figsize=(24, 6))
    for ax, drive in zip(axes, DRIVE_ORDER):
        plot_drive(drive, ax, show_legend=False)

    import matplotlib.patches as mpatches
    handles = [mpatches.Patch(color=MACHINE_COLORS[m], label=MACHINE_LABELS[m])
               for m in ["s8", "opi5"]]
    fig.legend(handles=handles, loc="lower center", ncol=2,
               fontsize=10, frameon=True, bbox_to_anchor=(0.5, -0.06))
    fig.suptitle("Merge-to-k32 Time (s8 vs opi5, per destination drive)",
                 fontsize=13, fontweight="bold")
    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, "merge_k27k32_combined_s8_vs_opi5.svg")
    fig.savefig(out, bbox_inches="tight")
    out_png = os.path.join(IMAGES_DIR, "merge_k27k32_combined_s8_vs_opi5.png")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    out_base = os.path.join(BASE_IMAGES_DIR, "merge_k27k32_combined_s8_vs_opi5.png")
    fig.savefig(out_base, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_base}")


if __name__ == "__main__":
    print("Generating merge k27-k32 comparison plots …")
    for drive in DRIVE_ORDER:
        save_individual(drive)
    save_combined()
    print("Done.")
