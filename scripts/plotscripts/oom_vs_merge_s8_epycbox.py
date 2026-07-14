#!/usr/bin/env python3
"""
OOM approach vs Merge approach comparison: s8 (8Socket) and Epycbox, per drive
type. Replaces the earlier fpganode2/opi5/rpi5 comparison (corrections.md #14):
same two machines used throughout Section IV's memory-size/merge discussion.
Temp (source) drive is each machine's local NVMe (nvme-raid0 for s8,
sfatunmbi for epycbox); final drive varies (HDD, SSD, NVMe, Ceph_HDD).
For each N (rounds/files), solid bar = OOM time, hatched bar = merge time.
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

# {drive: {machine: (oom_memory_csv, merge_csv)}}
# s8:      temp = nvme-raid0 (local NVMe)
# epycbox: temp = sfatunmbi  (local NVMe)
DRIVE_CSVS = {
    "HDD": {
        "s8":      ("s8/varying_memory_k32_CP384_IO1_data-i.csv",
                    "s8/k27-k32merge_nvme-raid0-data-i.csv"),
        "epycbox": ("epycbox/varying_memory_k32_CP128_IO1_data-m.csv",
                    "epycbox/k27-k32merge_sfatunmbi-data-m.csv"),
    },
    "SSD": {
        "s8":      ("s8/varying_memory_k32_CP384_IO1_ssd-raid0.csv",
                    "s8/k27-k32merge_nvme-raid0-ssd-raid0.csv"),
        "epycbox": ("epycbox/varying_memory_k32_CP128_IO1_ssd-raid0.csv",
                    "epycbox/k27-k32merge_sfatunmbi-ssd-raid0.csv"),
    },
    "NVME": {
        "s8":      ("s8/varying_memory_k32_CP384_IO1_nvme-raid0_new.csv",
                    "s8/k27-k32merge_nvme-raid0-nvme-raid0.csv"),
        "epycbox": ("epycbox/varying_memory_k32_CP128_IO1_sfatunmbi.csv",
                    "epycbox/k27-k32merge_sfatunmbi-sfatunmbi.csv"),
    },
    "CEPH": {
        "s8":      ("s8/varying_memory_k32_CP384_IO1_ceph.csv",
                    "s8/k27-k32merge_nvme-raid0-ceph.csv"),
        "epycbox": ("epycbox/varying_memory_k32_CP128_IO1_ceph.csv",
                    "epycbox/k27-k32merge_sfatunmbi-ceph.csv"),
    },
}

DRIVE_ORDER  = ["HDD", "SSD", "NVME", "CEPH"]
DRIVE_TITLES = {"HDD": "HDD", "SSD": "SSD", "NVME": "NVME", "CEPH": "Ceph_HDD"}

# N (rounds/files) <-> (approx_mem_gb for OOM, merge source K)
N_PAIRING = {
    2:  (26, 31),
    4:  (14, 30),
    8:  (8,  29),
    16: (5,  28),
    32: (3,  27),
}
N_VALUES = sorted(N_PAIRING.keys())

MACHINE_ORDER  = ["s8", "epycbox"]
MACHINE_COLORS = {
    "s8":      "#1F77B4",
    "epycbox": "#BCBD22",
}
MACHINE_LABELS = {
    "s8":      "8Socket",
    "epycbox": "Epycbox",
}

OOM_ALPHA   = 1.0
MERGE_ALPHA = 0.55


def load_memory_time(csv_path: str, mem_gb: float) -> float | None:
    if not os.path.exists(csv_path):
        return None
    df = pd.read_csv(csv_path)
    df["total_time_min"] = pd.to_numeric(df["total_time_min"], errors="coerce")
    df["memory_gb"]      = pd.to_numeric(df["memory_gb"],      errors="coerce")
    df = df.dropna(subset=["total_time_min", "memory_gb"])
    if df.empty:
        return None
    idx = (df["memory_gb"] - mem_gb).abs().idxmin()
    if abs(float(df.loc[idx, "memory_gb"]) - mem_gb) > 1.5:
        return None
    return float(df.loc[idx, "total_time_min"])


def load_merge_time(csv_path: str, k: int) -> float | None:
    if not os.path.exists(csv_path):
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

    machines  = [m for m in MACHINE_ORDER if m in data]
    n_mach    = len(machines)
    n_groups  = 2 * n_mach
    bw        = 0.7 / n_groups
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
    ax.set_xticklabels(xlabels, fontsize=7.5)
    ax.set_xlabel("N (rounds / files)", fontsize=9)
    ax.set_ylabel("Time (min)", fontsize=9)
    ax.set_title(DRIVE_TITLES.get(drive, drive), fontsize=11, fontweight="bold")
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=6.5, ha="right", va="top",
            color="gray", style="italic")
    if show_legend:
        ax.legend(fontsize=7.5, loc="upper left", ncol=2)


def save_combined():
    COMBINED_ORDER = ["HDD", "SSD", "NVME", "CEPH"]
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))
    axes_flat = axes.flatten()
    for ax, drive in zip(axes_flat, COMBINED_ORDER):
        plot_drive(drive, ax, show_legend=False)

    max_y = max(ax.get_ylim()[1] for ax in axes_flat)
    for ax in axes_flat:
        ax.set_ylim(0, max_y)

    legend_items = []
    for machine in MACHINE_ORDER:
        color = MACHINE_COLORS[machine]
        legend_items.append(mpatches.Patch(color=color, alpha=OOM_ALPHA,
                                            label=f"{MACHINE_LABELS[machine]} OOM"))
        legend_items.append(mpatches.Patch(color=color, alpha=MERGE_ALPHA,
                                            label=f"{MACHINE_LABELS[machine]} Merge", hatch="//"))
    fig.legend(handles=legend_items, loc="lower center", ncol=4,
               fontsize=9.5, frameon=True, bbox_to_anchor=(0.5, -0.03))
    fig.tight_layout(rect=[0, 0.05, 1, 1])
    out = os.path.join(IMAGES_DIR, "oom_vs_merge_combined_s8_epycbox.svg")
    fig.savefig(out, bbox_inches="tight")
    out_png = os.path.join(IMAGES_DIR, "oom_vs_merge_combined_s8_epycbox.png")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    out_paper = os.path.join(PAPER_IMAGES_DIR, "oom_vs_merge_combined_s8_epycbox.png")
    fig.savefig(out_paper, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_paper}")


if __name__ == "__main__":
    print("Generating OOM vs merge plots (s8, epycbox) …")
    save_combined()
    print("Done.")
