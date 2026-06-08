#!/usr/bin/env python3
"""
Task 2: Compute-thread scaling for k32 plotting across all machines.
One plot per drive type (HDD, CEPH, SSD, NVME); each plot has one line per machine.
A single reference ideal-speedup line (based on s8's data) is shown per plot.
Both axes use linear scale with actual values written out.
Individual PNGs saved + one combined SVG.
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.lines as mlines
import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Results")
IMAGES_DIR  = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)

# Notes:
#  - torus ssd-raid0 = NVMe (context.md)
#  - epycbox sfatunmbi = local NVMe
#  - thunderx1 only has sfatunmbi (SSD) varying_CP data
#  - rpi5 has no varying_CP data
#  - nvmebox data-b = local HDD (context.md)

MACHINE_VARYING_CP = {
    "s8": {
        "HDD":  "s8/varying_CP_k32_nfs_hdd_IM.csv",
        "CEPH": "s8/varying_CP_k32_ceph_IM.csv",
        "SSD":  "s8/varying_CP_k32_ssd-raid0_IM.csv",
        "NVME": "s8/varying_CP_k32_nvme-raid0_IM.csv",
    },
    "epycbox": {
        "HDD":  "epycbox/varying_CP_k32_data-m_IM.csv",
        "CEPH": "epycbox/varying_CP_k32_ceph_IM.csv",
        "SSD":  "epycbox/varying_CP_k32_ssd-raid0_IM.csv",
        "NVME": "epycbox/varying_CP_k32_sfatunmbi_IM.csv",
    },
    "athena": {
        "HDD":  "athena/varying_CP_k32_HDD_IM.csv",
        "CEPH": "athena/varying_CP_k32_ceph_IM.csv",
        "SSD":  "athena/varying_CP_k32_sfatunmbi_IM.csv",
        "NVME": "athena/varying_CP_k32_NVME_RAID-0_IM.csv",
    },
    "opi5": {
        "HDD":  "opi5/varying_CP_k32_data-a_IM.csv",
        "CEPH": "opi5/varying_CP_k32_ceph_IM.csv",
        "NVME": "opi5/varying_CP_k32_data-fast_IM.csv",
        # no SSD on opi5
    },
    "rpi5": {
        # no varying_CP data yet
    },
    "fpganode2": {
        "HDD":  "fpganode2/varying_CP_k32_nfs_hdd_IM.csv",
        "CEPH": "fpganode2/varying_CP_k32_ceph_IM.csv",
        "SSD":  "fpganode2/varying_CP_k32_ssd-raid0_IM.csv",
        "NVME": "fpganode2/varying_CP_k32_nfs_nvme_IM.csv",
    },
    "gpubox": {
        "HDD":  "gpubox/varying_CP_k32_nfs_hdd_IM.csv",
        "CEPH": "gpubox/varying_CP_k32_ceph_IM.csv",
        "SSD":  "gpubox/varying_CP_k32_data-fast_IM.csv",
        "NVME": "gpubox/varying_CP_k32_nfs_nvme_IM.csv",
    },
    "nvmebox": {
        "HDD":  "nvmebox/varying_CP_k32_data-b_IM.csv",    # data-b = local HDD
        "CEPH": "nvmebox/varying_CP_k32_ceph_IM.csv",
        "SSD":  "nvmebox/varying_CP_k32_data-fast_IM.csv",
        "NVME": "nvmebox/varying_CP_k32_nfs_nvme_IM.csv",
    },
    "thunderx1": {
        "SSD":  "thunderx1/varying_CP_k32_sfatunmbi_IM.csv",  # only SSD data
    },
    "thunderx2": {
        "HDD":  "thunderx2/varying_CP_k32_nfs_hdd_IM.csv",
        "CEPH": "thunderx2/varying_CP_k32_ceph_IM.csv",
        "SSD":  "thunderx2/varying_CP_k32_sfatunmbi_IM.csv",
        "NVME": "thunderx2/varying_CP_k32_nfs_nvme_IM.csv",
    },
    "torus": {
        "HDD":  "torus/varying_CP_k32_data-c_IM.csv",
        "CEPH": "torus/varying_CP_k32_ceph_IM.csv",
        "SSD":  "torus/varying_CP_k32_sfatunmbi_IM.csv",
        "NVME": "torus/varying_CP_k32_ssd-raid0_IM.csv",  # ssd-raid0 = NVMe on torus
    },
}

MACHINE_ORDER = [
    "s8", "epycbox", "athena", "gpubox", "nvmebox",
    "thunderx1", "thunderx2", "torus", "fpganode2", "opi5", "rpi5",
]

MACHINE_COLORS = {
    "s8":        "#1F77B4",
    "epycbox":   "#FF7F0E",
    "athena":    "#2CA02C",
    "gpubox":    "#D62728",
    "nvmebox":   "#9467BD",
    "thunderx1": "#8C564B",
    "thunderx2": "#E377C2",
    "torus":     "#7F7F7F",
    "fpganode2": "#BCBD22",
    "opi5":      "#17BECF",
    "rpi5":      "#AEC7E8",
}

DRIVE_ORDER = ["HDD", "CEPH", "SSD", "NVME"]
DRIVE_TITLES = {"HDD": "HDD", "CEPH": "CEPH", "SSD": "SSD", "NVME": "NVME"}

# Reference machine for the single ideal line (largest thread range)
IDEAL_REF_MACHINE = "s8"
IDEAL_REF_DRIVE   = {
    "HDD":  "s8/varying_CP_k32_nfs_hdd_IM.csv",
    "CEPH": "s8/varying_CP_k32_ceph_IM.csv",
    "SSD":  "s8/varying_CP_k32_ssd-raid0_IM.csv",
    "NVME": "s8/varying_CP_k32_nvme-raid0_IM.csv",
}


def load_df(rel_path: str) -> pd.DataFrame | None:
    full = os.path.join(RESULTS_DIR, rel_path)
    return pd.read_csv(full) if os.path.exists(full) else None


def nice_yticks(max_val: float, step: float | None = None):
    if step is None:
        candidates = [5, 10, 20, 50, 100, 200]
        for s in candidates:
            ticks = np.arange(0, max_val * 1.25 + s, s)
            if 4 <= len(ticks) <= 10:
                return ticks
        return np.linspace(0, max_val * 1.2, 7)
    return np.arange(0, max_val * 1.25 + step, step)


def nice_xticks(threads_all: list[int]) -> list[int]:
    """Return sorted unique thread counts across all machines for this drive."""
    return sorted(set(threads_all))


def plot_drive(drive: str, ax: plt.Axes, show_legend: bool = True):
    all_threads = []
    max_time    = 0.0
    lines_drawn = []

    for machine in MACHINE_ORDER:
        cfg = MACHINE_VARYING_CP.get(machine, {})
        if drive not in cfg:
            continue
        df = load_df(cfg[drive])
        if df is None or df.empty:
            continue
        threads = df["varying_threads"].values.astype(int)
        times   = df["total_time_min"].values
        ax.plot(threads, times, "o-", color=MACHINE_COLORS[machine],
                label=machine, linewidth=1.5, markersize=4, zorder=3)
        all_threads.extend(threads.tolist())
        max_time = max(max_time, times.max())
        lines_drawn.append(machine)

    # Single reference ideal line based on s8 (or the first available machine)
    ref_path = IDEAL_REF_DRIVE.get(drive)
    ref_df   = load_df(ref_path) if ref_path else None
    if ref_df is not None and not ref_df.empty:
        ref_threads = ref_df["varying_threads"].values.astype(int)
        t1 = ref_df.loc[ref_df["varying_threads"] == ref_threads.min(),
                         "total_time_min"].values[0]
        # Ideal: extends to the max thread seen on s8
        ideal_x = ref_threads
        ideal_y = t1 / ideal_x
        ax.plot(ideal_x, ideal_y, "--", color="black", linewidth=1.2,
                alpha=0.5, zorder=2, label=f"Ideal (s8 ref)")

    # Axes: log-2 x (equal spacing per doubling) + log y (ideal appears as straight line)
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.xaxis.set_major_formatter(plt.ScalarFormatter())
    ax.xaxis.set_minor_formatter(plt.NullFormatter())
    ax.yaxis.set_major_formatter(plt.ScalarFormatter())
    ax.yaxis.set_minor_formatter(plt.NullFormatter())
    xticks = nice_xticks(all_threads)
    ax.set_xticks(xticks)
    ax.set_xticklabels([str(t) for t in xticks], fontsize=7, rotation=45, ha="right")
    ax.set_xlabel("Compute threads", fontsize=9)
    ax.set_ylabel("Time", fontsize=9)
    ax.set_title(DRIVE_TITLES.get(drive, drive), fontsize=11, fontweight="bold")
    ax.grid(True, which="both", linestyle="--", alpha=0.3, zorder=0)
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=6.5, ha="right", va="top",
            color="gray", style="italic")
    if show_legend and lines_drawn:
        ax.legend(fontsize=7, loc="upper right", ncol=2)
    if not lines_drawn:
        ax.text(0.5, 0.5, "No data", transform=ax.transAxes,
                ha="center", va="center", color="gray")


def save_individual_drive(drive: str):
    fig, ax = plt.subplots(figsize=(8, 5))
    plot_drive(drive, ax)
    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, f"thread_scaling_{drive.lower()}.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


def save_combined():
    fig, axes = plt.subplots(1, 4, figsize=(26, 6))
    for ax, drive in zip(axes, DRIVE_ORDER):
        plot_drive(drive, ax, show_legend=False)

    legend_handles = []
    for m in MACHINE_ORDER:
        has_any = any(drive in MACHINE_VARYING_CP.get(m, {}) for drive in DRIVE_ORDER)
        if has_any:
            legend_handles.append(
                mlines.Line2D([0], [0], color=MACHINE_COLORS[m], linewidth=2, label=m))
    ideal_entry = mlines.Line2D([0], [0], linestyle="--", color="black",
                                 linewidth=1.2, alpha=0.5, label="Ideal (s8 ref)")
    legend_handles.append(ideal_entry)

    fig.legend(handles=legend_handles, loc="lower center",
               ncol=6, fontsize=9, frameon=True, bbox_to_anchor=(0.5, -0.14))
    fig.suptitle("K32 Plotting Time vs Compute Threads (all machines, per drive type)",
                 fontsize=13, fontweight="bold")
    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, "thread_scaling_combined.svg")
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


if __name__ == "__main__":
    print("Generating compute-thread scaling plots …")
    for drive in DRIVE_ORDER:
        save_individual_drive(drive)
    save_combined()
    print("Done.")
