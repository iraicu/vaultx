#!/usr/bin/env python3
"""
Task 7: Compare OOM plotting vs merge approach for producing a k32, per drive type.
For a given N (rounds / files):
  - OOM approach:   plot one k32 with limited memory → N internal rounds
  - Merge approach: plot N smaller k-files then merge into k32

Covers s8 and opi5 on all available drive types (NVME, HDD, CEPH, SSD).
N pairing (same for both machines):
  N=2  → memory≈26 GB, merge K=31 (2 k31 files)
  N=4  → memory≈14 GB, merge K=30 (4 k30 files)
  N=8  → memory≈ 8 GB, merge K=29 (8 k29 files)
  N=16 → memory≈ 5 GB, merge K=28 (16 k28 files)
  N=32 → memory≈ 3 GB, merge K=27 (32 k27 files)
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Results")
IMAGES_DIR  = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)

# {drive_type: {machine: (memory_csv, merge_csv)}}
DRIVE_CSVS = {
    "NVME": {
        "s8":   ("s8/varying_memory_k32_CP384_IO1_nvme-raid0.csv",
                 "s8/k27-k32merge_nvme-raid0-nvme-raid0.csv"),
        "opi5": ("opi5/varying_memory_k32_CP8_IO1_data-fast.csv",
                 "opi5/k27-k32merge_data-fast-data-fast.csv"),
    },
    "HDD": {
        "s8":   ("s8/varying_memory_k32_CP384_IO1_nfs_hdd.csv",
                 "s8/k27-k32merge_nvme-raid0-nfs_hdd.csv"),
        "opi5": ("opi5/varying_memory_k32_CP8_IO1_data-a.csv",
                 "opi5/k27-k32merge_data-fast-data-a.csv"),
    },
    "SSD": {
        "s8":   ("s8/varying_memory_k32_CP384_IO1_ssd-raid0.csv",
                 "s8/k27-k32merge_nvme-raid0-ssd-raid0.csv"),
        # opi5 has no SSD
    },
    "CEPH": {
        "s8":   ("s8/varying_memory_k32_CP384_IO1_ceph.csv",
                 "s8/k27-k32merge_nvme-raid0-ceph.csv"),
        "opi5": ("opi5/varying_memory_k32_CP8_IO1_ceph.csv",
                 "opi5/k27-k32merge_data-fast-ceph.csv"),
    },
}

DRIVE_ORDER = ["HDD", "CEPH", "SSD", "NVME"]
DRIVE_TITLES = {
    "HDD":  "HDD",
    "CEPH": "CEPH",
    "SSD":  "SSD",
    "NVME": "NVME",
}

N_PAIRING = {
    2:  (26, 31),
    4:  (14, 30),
    8:  (8,  29),
    16: (5,  28),
    32: (3,  27),
}
N_VALUES = sorted(N_PAIRING.keys())

# Colors and hatches
MACHINE_BASE = {"s8": "#1F77B4", "opi5": "#2CA02C"}   # blue, green
MACHINE_LABELS = {"s8": "s8", "opi5": "opi5"}
OOM_ALPHA   = 1.0
MERGE_ALPHA = 0.55  # lighter shade for merge bars
MAX_MEMORY_GB = 26


def load_memory_time(csv_path: str, mem_gb: float) -> float | None:
    if not os.path.exists(csv_path):
        return None
    df = pd.read_csv(csv_path)
    df = df[df["memory_gb"] <= MAX_MEMORY_GB]
    if df.empty:
        return None
    idx = (df["memory_gb"].astype(float) - mem_gb).abs().idxmin()
    return float(df.loc[idx, "total_time_min"])


def load_merge_time(csv_path: str, k: int) -> float | None:
    if not os.path.exists(csv_path):
        return None
    df = pd.read_csv(csv_path)
    row = df[df["K"] == k]
    return float(row["total_time_min"].values[0]) if len(row) else None


def collect_data(drive: str) -> dict:
    """Return {machine: {n: (oom_time, merge_time)}} for a drive."""
    result = {}
    for machine, (mem_csv, merge_csv) in DRIVE_CSVS.get(drive, {}).items():
        full_mem   = os.path.join(RESULTS_DIR, mem_csv)
        full_merge = os.path.join(RESULTS_DIR, merge_csv)
        pts = {}
        for n, (mem_gb, k_src) in N_PAIRING.items():
            oom   = load_memory_time(full_mem,   mem_gb)
            merge = load_merge_time(full_merge, k_src)
            if oom is not None or merge is not None:
                pts[n] = (oom, merge)
        if pts:
            result[machine] = pts
    return result


def plot_drive(drive: str, ax: plt.Axes, show_legend: bool = True):
    data = collect_data(drive)
    if not data:
        ax.text(0.5, 0.5, "No data", transform=ax.transAxes,
                ha="center", va="center", color="gray")
        ax.set_title(DRIVE_TITLES.get(drive, drive), fontsize=11, fontweight="bold")
        return

    machines = list(data.keys())
    n_mach   = len(machines)
    # 2 bars per machine (OOM, merge), grouped per N value
    n_groups = 2 * n_mach
    bw       = 0.8 / n_groups
    x        = np.arange(len(N_VALUES))
    max_t    = 0.0

    for m_idx, machine in enumerate(machines):
        color = MACHINE_BASE[machine]
        for app_idx, (app_key, alpha, hatch, label_sfx) in enumerate(
                [("oom",   OOM_ALPHA,   "",    "OOM"),
                 ("merge", MERGE_ALPHA, "//",  "Merge")]):
            offset = ((m_idx * 2 + app_idx) - (n_groups - 1) / 2) * bw
            times  = []
            for n in N_VALUES:
                oom_t, merge_t = data[machine].get(n, (None, None))
                t = oom_t if app_key == "oom" else merge_t
                times.append(t if t is not None else 0.0)
                max_t = max(max_t, t or 0.0)
            ax.bar(x + offset, times, width=bw * 0.9,
                   color=color, alpha=alpha, hatch=hatch, zorder=3,
                   label=f"{MACHINE_LABELS[machine]} {label_sfx}",
                   edgecolor="white" if not hatch else color)

    ax.set_xticks(x)
    xlabels = [f"N={n}\n({N_PAIRING[n][0]}GB\n/ k{N_PAIRING[n][1]})" for n in N_VALUES]
    ax.set_xticklabels(xlabels, fontsize=7)
    ax.set_xlabel("N (rounds / files)", fontsize=9)
    ax.set_ylabel("Time (min)", fontsize=9)
    ax.set_title(DRIVE_TITLES.get(drive, drive), fontsize=11, fontweight="bold")
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=6.5, ha="right", va="top",
            color="gray", style="italic")
    if show_legend:
        ax.legend(fontsize=7, loc="upper left", ncol=2)


def save_individual(drive: str):
    fig, ax = plt.subplots(figsize=(9, 5))
    plot_drive(drive, ax)
    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, f"oom_vs_merge_{drive.lower()}.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


def save_combined():
    fig, axes = plt.subplots(1, 4, figsize=(28, 6))
    for ax, drive in zip(axes, DRIVE_ORDER):
        plot_drive(drive, ax, show_legend=False)

    # Shared legend
    legend_items = []
    for machine in ["s8", "opi5"]:
        color = MACHINE_BASE[machine]
        legend_items.append(mpatches.Patch(color=color, alpha=OOM_ALPHA,
                                            label=f"{machine} OOM"))
        legend_items.append(mpatches.Patch(color=color, alpha=MERGE_ALPHA,
                                            label=f"{machine} Merge", hatch="//"))
    fig.legend(handles=legend_items, loc="lower center", ncol=4,
               fontsize=9, frameon=True, bbox_to_anchor=(0.5, -0.08))
    fig.suptitle("OOM Approach vs Merge Approach — s8 & opi5 (per drive type)",
                 fontsize=13, fontweight="bold")
    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, "oom_vs_merge_combined.svg")
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


if __name__ == "__main__":
    print("Generating OOM vs merge comparison plots …")
    for drive in DRIVE_ORDER:
        save_individual(drive)
    save_combined()
    print("Done.")
