#!/usr/bin/env python3
"""
Merge approach (k27→k32) time comparison: fpganode2, opi5, rpi5 per drive type.
X-axis: source K (k27–k31), with number of files needed.
rpi5 only shown up to k29 (k30/k31 were OOM-generated before merge → absurd times).
No title.
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments")
IMAGES_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "images")
PAPER_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

# Source (temp) drive is NFS_NVME for fpganode2; data-fast for opi5 and rpi5.
# Destination drive varies per subplot.
# rpi5 SSD: none → rpi5 absent from SSD subplot.
DRIVE_CSVS = {
    "HDD": {
        "fpganode2": "fpganode2/k27-k32merge_nfs_nvme-nfs_hdd.csv",
        "opi5":      "opi5/k27-k32merge_data-fast-data-a.csv",
        "rpi5":      "rpi5/k27-k32merge_data-fast-data-a.csv",
    },
    "SSD": {
        "fpganode2": "fpganode2/k27-k32merge_nfs_nvme-ssd-raid0.csv",
        # opi5 and rpi5 have no SSD
    },
    "NVME": {
        "fpganode2": "fpganode2/k27-k32merge_nfs_nvme-nfs_nvme.csv",
        "opi5":      "opi5/k27-k32merge_data-fast-data-fast.csv",
        "rpi5":      "rpi5/k27-k32merge_data-fast-data-fast.csv",
    },
    "CEPH": {
        "fpganode2": "fpganode2/k27-k32merge_nfs_nvme-ceph.csv",
        "opi5":      "opi5/k27-k32merge_data-fast-ceph.csv",
        "rpi5":      "rpi5/k27-k32merge_data-fast-ceph.csv",
    },
}

DRIVE_ORDER  = ["HDD", "SSD", "NVME", "CEPH"]
DRIVE_TITLES = {
    "HDD":  "NVME → HDD",
    "SSD":  "NVME → SSD",
    "NVME": "NVME → NVME",
    "CEPH": "NVME → CEPH",
}

# X-axis order: fewest files first (K=31 → K=27)
K_ORDER = [31, 30, 29, 28, 27]
N_MAP   = {31: 2, 30: 4, 29: 8, 28: 16, 27: 32}

RPI5_MAX_K = 29   # k30 and k31 for rpi5 were OOM-generated → exclude

MACHINE_ORDER  = ["fpganode2", "opi5", "rpi5"]
MACHINE_COLORS = {
    "fpganode2": "#BCBD22",
    "opi5":      "#17BECF",
    "rpi5":      "#AEC7E8",
}
MACHINE_LABELS = {
    "fpganode2": "fpganode2 (16T/32GB)",
    "opi5":      "opi5 (8T/32GB)",
    "rpi5":      "rpi5 (4T/8GB)",
}


def load_df(rel_path: str) -> pd.DataFrame | None:
    full = os.path.join(RESULTS_DIR, rel_path)
    return pd.read_csv(full) if os.path.exists(full) else None


def get_time(df: pd.DataFrame, machine: str, k: int) -> float:
    if machine == "rpi5" and k > RPI5_MAX_K:
        return 0.0
    row = df[df["K"] == k]
    return float(row["total_time_min"].values[0]) if not row.empty else 0.0


def plot_drive(drive: str, ax: plt.Axes, show_legend: bool = True):
    machine_dfs = {}
    for machine in MACHINE_ORDER:
        rel = DRIVE_CSVS.get(drive, {}).get(machine)
        if rel:
            df = load_df(rel)
            if df is not None:
                machine_dfs[machine] = df

    x      = np.arange(len(K_ORDER))
    n_mach = len(machine_dfs)
    bw     = 0.7 / max(n_mach, 1)

    for i, machine in enumerate(m for m in MACHINE_ORDER if m in machine_dfs):
        df     = machine_dfs[machine]
        offset = (i - (n_mach - 1) / 2) * bw
        times  = [get_time(df, machine, k) for k in K_ORDER]
        ax.bar(x + offset, times, width=bw * 0.9,
               color=MACHINE_COLORS[machine],
               label=MACHINE_LABELS[machine], zorder=3)

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
                       f"merge_k27k32_{drive.lower()}_fpganode2_opi5_rpi5.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


def save_combined():
    # 2×2 square layout: HDD top-left, SSD top-right, NVME bottom-left, CEPH bottom-right
    COMBINED_ORDER = ["HDD", "SSD", "NVME", "CEPH"]
    fig, axes = plt.subplots(2, 2, figsize=(14, 14))
    axes_flat = axes.flatten()
    for ax, drive in zip(axes_flat, COMBINED_ORDER):
        plot_drive(drive, ax, show_legend=False)

    # Synchronise y-axis so drives can be compared directly
    max_y = max(ax.get_ylim()[1] for ax in axes_flat)
    for ax in axes_flat:
        ax.set_ylim(0, max_y)

    handles = [mpatches.Patch(color=MACHINE_COLORS[m], label=MACHINE_LABELS[m])
               for m in MACHINE_ORDER]
    fig.legend(handles=handles, loc="lower center", ncol=3,
               fontsize=10, frameon=True, bbox_to_anchor=(0.5, -0.03))
    fig.tight_layout(rect=[0, 0.04, 1, 1])
    out = os.path.join(IMAGES_DIR,
                       "merge_k27k32_combined_fpganode2_opi5_rpi5.svg")
    fig.savefig(out, bbox_inches="tight")
    out_png = os.path.join(IMAGES_DIR, "merge_k27k32_combined_fpganode2_opi5_rpi5.png")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    out_paper = os.path.join(PAPER_IMAGES_DIR, "merge_k27k32_combined_fpganode2_opi5_rpi5.png")
    fig.savefig(out_paper, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_paper}")


if __name__ == "__main__":
    print("Generating merge k27-k32 plots (fpganode2, opi5, rpi5) …")
    for drive in DRIVE_ORDER:
        save_individual(drive)
    save_combined()
    print("Done.")
