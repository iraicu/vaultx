#!/usr/bin/env python3
"""
OOM approach vs Merge approach comparison: fpganode2, opi5, rpi5 per drive type.
For each N (rounds/files), solid bar = OOM time, hatched bar = merge time.
rpi5 OOM limited to ≤5 GB; rpi5 merge limited to k29 (k30/k31 OOM-generated).
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

# {drive: {machine: (memory_csv, merge_csv)}}
# fpganode2: HDD=nfs_hdd, SSD=ssd-raid0, NVME=nfs_nvme; temp drive is nfs_nvme
# opi5:      HDD=data-a,  no SSD;         temp drive is data-fast
# rpi5:      HDD=data-a,  no SSD;         temp drive is data-fast
DRIVE_CSVS = {
    "HDD": {
        "fpganode2": ("fpganode2/varying_memory_k32_CP16_IO1_nfs_hdd.csv",
                      "fpganode2/k27-k32merge_nfs_nvme-nfs_hdd.csv"),
        "opi5":      ("opi5/varying_memory_k32_CP8_IO1_data-a.csv",
                      "opi5/k27-k32merge_data-fast-data-a.csv"),
        "rpi5":      ("rpi5/varying_memory_k32_CP4_IO1_data-a.csv",
                      "rpi5/k27-k32merge_data-fast-data-a.csv"),
    },
    "SSD": {
        "fpganode2": ("fpganode2/varying_memory_k32_CP16_IO1_ssd-raid0.csv",
                      "fpganode2/k27-k32merge_nfs_nvme-ssd-raid0.csv"),
        # opi5 and rpi5 have no SSD
    },
    "NVME": {
        "fpganode2": ("fpganode2/varying_memory_k32_CP16_IO1_nfs_nvme.csv",
                      "fpganode2/k27-k32merge_nfs_nvme-nfs_nvme.csv"),
        "opi5":      ("opi5/varying_memory_k32_CP8_IO1_data-fast.csv",
                      "opi5/k27-k32merge_data-fast-data-fast.csv"),
        "rpi5":      ("rpi5/varying_memory_k32_CP4_IO1_data-fast.csv",
                      "rpi5/k27-k32merge_data-fast-data-fast.csv"),
    },
    "CEPH": {
        "fpganode2": ("fpganode2/varying_memory_k32_CP16_IO1_ceph.csv",
                      "fpganode2/k27-k32merge_nfs_nvme-ceph.csv"),
        "opi5":      ("opi5/varying_memory_k32_CP8_IO1_ceph.csv",
                      "opi5/k27-k32merge_data-fast-ceph.csv"),
        "rpi5":      ("rpi5/varying_memory_k32_CP4_IO1_ceph.csv",
                      "rpi5/k27-k32merge_data-fast-ceph.csv"),
    },
}

DRIVE_ORDER  = ["HDD", "SSD", "NVME", "CEPH"]
DRIVE_TITLES = {"HDD": "HDD", "SSD": "SSD", "NVME": "NVME", "CEPH": "CEPH"}

# N (rounds/files) ↔ (approx_mem_gb, merge_k_source)
N_PAIRING = {
    2:  (26, 31),
    4:  (14, 30),
    8:  (8,  29),
    16: (5,  28),
    32: (3,  27),
}
N_VALUES = sorted(N_PAIRING.keys())

MACHINE_ORDER  = ["fpganode2", "opi5", "rpi5"]
MACHINE_COLORS = {
    "fpganode2": "#BCBD22",
    "opi5":      "#17BECF",
    "rpi5":      "#AEC7E8",
}
MACHINE_LABELS = {
    "fpganode2": "fpganode2",
    "opi5":      "opi5",
    "rpi5":      "rpi5",
}

RPI5_MAX_MEM_GB = 8    # rpi5 8GB row = k29 / N=8 data point
RPI5_MAX_K      = 29
OOM_ALPHA       = 1.0
MERGE_ALPHA     = 0.55


def load_memory_time(csv_path: str, mem_gb: float, machine: str) -> float | None:
    if not os.path.exists(csv_path):
        return None
    if machine == "rpi5" and mem_gb > RPI5_MAX_MEM_GB:
        return None
    df = pd.read_csv(csv_path)
    df["total_time_min"] = pd.to_numeric(df["total_time_min"], errors="coerce")
    df["memory_gb"]      = pd.to_numeric(df["memory_gb"],      errors="coerce")
    df = df.dropna(subset=["total_time_min", "memory_gb"])
    if machine == "rpi5":
        df = df[df["memory_gb"] <= RPI5_MAX_MEM_GB]
    if df.empty:
        return None
    idx = (df["memory_gb"] - mem_gb).abs().idxmin()
    if abs(float(df.loc[idx, "memory_gb"]) - mem_gb) > 1.5:
        return None  # nearest point too far away — skip
    return float(df.loc[idx, "total_time_min"])


def load_merge_time(csv_path: str, k: int, machine: str) -> float | None:
    if not os.path.exists(csv_path):
        return None
    if machine == "rpi5" and k > RPI5_MAX_K:
        return None
    df = pd.read_csv(csv_path)
    row = df[df["K"] == k]
    return float(row["total_time_min"].values[0]) if not row.empty else None


def collect_data(drive: str) -> dict:
    """Return {machine: {n: (oom_time_or_None, merge_time_or_None)}}."""
    result = {}
    for machine, (mem_csv, merge_csv) in DRIVE_CSVS.get(drive, {}).items():
        full_mem   = os.path.join(RESULTS_DIR, mem_csv)
        full_merge = os.path.join(RESULTS_DIR, merge_csv)
        pts = {}
        for n, (mem_gb, k_src) in N_PAIRING.items():
            oom   = load_memory_time(full_mem,   mem_gb, machine)
            merge = load_merge_time(full_merge, k_src,  machine)
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

    machines  = [m for m in MACHINE_ORDER if m in data]
    n_mach    = len(machines)
    n_groups  = 2 * n_mach   # OOM + merge per machine
    bw        = 0.8 / n_groups
    x         = np.arange(len(N_VALUES))

    for m_idx, machine in enumerate(machines):
        color = MACHINE_COLORS[machine]
        for app_idx, (app_key, alpha, hatch, label_sfx) in enumerate([
                ("oom",   OOM_ALPHA,   "",   "OOM"),
                ("merge", MERGE_ALPHA, "//", "Merge"),
        ]):
            offset = ((m_idx * 2 + app_idx) - (n_groups - 1) / 2) * bw
            times  = []
            for n in N_VALUES:
                oom_t, merge_t = data[machine].get(n, (None, None))
                t = oom_t if app_key == "oom" else merge_t
                times.append(t if t is not None else 0.0)
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
    out = os.path.join(IMAGES_DIR,
                       f"oom_vs_merge_{drive.lower()}_fpganode2_opi5_rpi5.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


def save_combined():
    # 2×2 square layout: HDD top-left, SSD top-right, NVME bottom-left, CEPH bottom-right
    COMBINED_ORDER = ["HDD", "SSD", "NVME", "CEPH"]
    fig, axes = plt.subplots(2, 2, figsize=(16, 14))
    axes_flat = axes.flatten()
    for ax, drive in zip(axes_flat, COMBINED_ORDER):
        plot_drive(drive, ax, show_legend=False)

    # Synchronise y-axis so drives can be compared directly
    max_y = max(ax.get_ylim()[1] for ax in axes_flat)
    for ax in axes_flat:
        ax.set_ylim(0, max_y)

    legend_items = []
    for machine in MACHINE_ORDER:
        color = MACHINE_COLORS[machine]
        legend_items.append(mpatches.Patch(color=color, alpha=OOM_ALPHA,
                                            label=f"{machine} OOM"))
        legend_items.append(mpatches.Patch(color=color, alpha=MERGE_ALPHA,
                                            label=f"{machine} Merge", hatch="//"))
    fig.legend(handles=legend_items, loc="lower center", ncol=6,
               fontsize=9, frameon=True, bbox_to_anchor=(0.5, -0.03))
    fig.tight_layout(rect=[0, 0.04, 1, 1])
    out = os.path.join(IMAGES_DIR,
                       "oom_vs_merge_combined_fpganode2_opi5_rpi5.svg")
    fig.savefig(out, bbox_inches="tight")
    out_png = os.path.join(IMAGES_DIR, "oom_vs_merge_combined_fpganode2_opi5_rpi5.png")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    out_paper = os.path.join(PAPER_IMAGES_DIR, "oom_vs_merge_combined_fpganode2_opi5_rpi5.png")
    fig.savefig(out_paper, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_paper}")


if __name__ == "__main__":
    print("Generating OOM vs merge plots (fpganode2, opi5, rpi5) …")
    for drive in DRIVE_ORDER:
        save_individual(drive)
    save_combined()
    print("Done.")
