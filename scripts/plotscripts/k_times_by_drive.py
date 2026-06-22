#!/usr/bin/env python3
"""
Plot k27-k32 plotting times (in minutes) for each drive type across all machines.
Each machine gets one bar-chart subplot; bars represent drive types (HDD, SSD, NVME, CEPH).
Individual PNGs per machine + one combined SVG.
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments")
IMAGES_DIR      = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
BASE_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(BASE_IMAGES_DIR, exist_ok=True)

# ── Drive-type config ─────────────────────────────────────────────────────────
# For each machine: {drive_label: relative_csv_path_from_RESULTS_DIR}
# Rules applied per context.md:
#   - Prefer local drive if available and faster than NFS equivalent
#   - SSD omitted if machine has no SSD (no NFS_SSD exists)
#   - torus: ssd-raid0 is NVMe; sfatunmbi is SSD
#   - athena: sfatunmbi is SSD; it is the NFS server so its drives ARE the NFS drives

MACHINE_DRIVES = {
    "s8": {
        "HDD":      "s8/k27-k32_nfs_hdd_CP384_IM.csv",    # nfs_hdd faster than data-i
        "SSD":      "s8/k27-k32_ssd-raid0_CP384_IM.csv",
        "NVME":     "s8/k27-k32_nvme-raid0_CP384_IM.csv",
        "CEPH_HDD": "s8/k27-k32_ceph_CP384_IM.csv",
    },
    "epycbox": {
        "HDD":      "epycbox/k27-k32_data-m_CP128_IM.csv",  # data-m faster than nfs_hdd
        "SSD":      "epycbox/k27-k32_ssd-raid0_CP128_IM.csv",
        "NVME":     "epycbox/k27-k32_sfatunmbi_CP128_IM.csv",
        "CEPH_HDD": "epycbox/k27-k32_ceph_CP128_IM.csv",
    },
    "athena": {
        "HDD":      "athena/k27-k32_HDD_CP48_IM.csv",
        "SSD":      "athena/k27-k32_sfatunmbi_CP48_IM.csv",
        "NVME":     "athena/k27-k32_NVME_RAID-0_CP48_IM.csv",
        "CEPH_HDD": "athena/k27-k32_ceph_CP48_IM.csv",
    },
    "opi5": {
        "HDD":      "opi5/k27-k32_data-a_CP8_IM.csv",
        # No SSD on opi5
        "NVME":     "opi5/k27-k32_data-fast_CP8_IM.csv",
        "CEPH_HDD": "opi5/k27-k32_ceph_CP8_IM.csv",
    },
    "rpi5": {
        "HDD":      "rpi5/k27-k32_data-a_CP4_IM.csv",      # data-a faster than nfs_hdd
        # No SSD on rpi5
        "NVME":     "rpi5/k27-k32_data-fast_CP4_IM.csv",   # data-fast faster than nfs_nvme
        "CEPH_HDD": "rpi5/k27-k32_ceph_CP4_IM.csv",
    },
    "fpganode2": {
        "HDD":      "fpganode2/k27-k32_nfs_hdd_CP16_IM.csv",
        "SSD":      "fpganode2/k27-k32_ssd-raid0_CP16_IM.csv",
        "NVME":     "fpganode2/k27-k32_nfs_nvme_CP16_IM.csv",
        "CEPH_HDD": "fpganode2/k27-k32_ceph_CP16_IM.csv",
    },
    "gpubox": {
        "HDD":      "gpubox/k27-k32_nfs_hdd_CP96_IM.csv",
        "SSD":      "gpubox/k27-k32_data-fast_CP96_IM.csv",
        "NVME":     "gpubox/k27-k32_nfs_nvme_CP96_IM.csv",
        "CEPH_HDD": "gpubox/k27-k32_ceph_CP96_IM.csv",
    },
    "nvmebox": {
        "HDD":      "nvmebox/k27-k32_data-b_CP64_IM.csv",    # data-b is local HDD
        "SSD":      "nvmebox/k27-k32_data-fast_CP64_IM.csv",
        "NVME":     "nvmebox/k27-k32_nfs_nvme_CP64_IM.csv",
        "CEPH_HDD": "nvmebox/k27-k32_ceph_CP64_IM.csv",
    },
    "thunderx1": {
        "HDD":      "thunderx1/k27-k32_nfs_hdd_CP96_IM.csv",
        "SSD":      "thunderx1/k27-k32_sfatunmbi_CP96_IM.csv",
        "NVME":     "thunderx1/k27-k32_nfs_nvme_CP96_IM.csv",
        "CEPH_HDD": "thunderx1/k27-k32_ceph_CP96_IM.csv",
    },
    "thunderx2": {
        "HDD":      "thunderx2/k27-k32_nfs_hdd_CP224_IM.csv",
        "SSD":      "thunderx2/k27-k32_sfatunmbi_CP224_IM.csv",
        "NVME":     "thunderx2/k27-k32_nfs_nvme_CP224_IM.csv",
        "CEPH_HDD": "thunderx2/k27-k32_ceph_CP224_IM.csv",
    },
    "torus": {
        "HDD":      "torus/k27-k32_data-c_CP32_IM.csv",    # local HDD faster than nfs_hdd
        "SSD":      "torus/k27-k32_sfatunmbi_CP32_IM.csv",
        "NVME":     "torus/k27-k32_ssd-raid0_CP32_IM.csv", # ssd-raid0 is NVMe on torus
        "CEPH_HDD": "torus/k27-k32_ceph_CP32_IM.csv",
    },
}

# Display order and labels
MACHINE_ORDER = [
    "s8", "epycbox", "athena", "gpubox", "nvmebox",
    "thunderx1", "thunderx2", "torus", "fpganode2", "opi5", "rpi5",
]

DRIVE_COLORS = {
    "HDD":      "#8B7355",  # brown
    "CEPH_HDD": "#FF7F0E",  # orange
    "SSD":      "#2CA02C",  # green
    "NVME":     "#1F77B4",  # blue
}
DRIVE_LABELS = {
    "HDD":      "HDD",
    "CEPH_HDD": "CEPH",
    "SSD":      "SSD",
    "NVME":     "NVME",
}
DRIVE_ORDER = ["HDD", "CEPH_HDD", "SSD", "NVME"]
K_VALUES    = [27, 28, 29, 30, 31, 32]


def load_machine_data(machine: str) -> dict[str, pd.DataFrame]:
    """Load all available drive CSVs for a machine; skip missing files."""
    result = {}
    for drive, rel_path in MACHINE_DRIVES[machine].items():
        full_path = os.path.join(RESULTS_DIR, rel_path)
        if os.path.exists(full_path):
            df = pd.read_csv(full_path)
            result[drive] = df
        # silently skip missing files
    return result


def smart_yticks(max_val: float):
    """Return y-axis ticks that give ~5-8 labels and end above max_val."""
    steps = [0.5, 1, 2, 5, 10, 15, 20, 25, 30, 50, 100, 200]
    for step in steps:
        ticks = np.arange(0, max_val * 1.25 + step, step)
        if 5 <= len(ticks) <= 10:
            return ticks
    return np.linspace(0, max_val * 1.2, 7)


def plot_machine(machine: str, ax: plt.Axes, title_fontsize: int = 11):
    """Draw the bar chart for one machine onto ax."""
    drive_data = load_machine_data(machine)
    available_drives = [d for d in DRIVE_ORDER if d in drive_data]

    n_drives  = len(available_drives)
    n_k       = len(K_VALUES)
    bar_width = 0.8 / n_drives
    x         = np.arange(n_k)

    max_time = 0.0
    for i, drive in enumerate(available_drives):
        df     = drive_data[drive]
        times  = [df.loc[df["k"] == k, "total_time_min"].values[0]
                  if k in df["k"].values else 0.0
                  for k in K_VALUES]
        offset = (i - (n_drives - 1) / 2) * bar_width
        ax.bar(x + offset, times, width=bar_width * 0.9,
               color=DRIVE_COLORS[drive], label=DRIVE_LABELS[drive], zorder=3)
        max_time = max(max_time, max(times))

    yticks = smart_yticks(max_time)
    ax.set_yticks(yticks)
    ax.set_ylim(0, yticks[-1])
    ax.set_xticks(x)
    ax.set_xticklabels([f"k{k}" for k in K_VALUES], fontsize=8)
    ax.tick_params(axis="y", labelsize=7)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.set_title(machine, fontsize=title_fontsize, fontweight="bold")
    ax.set_xlabel("K value", fontsize=8)
    ax.set_ylabel("Time (min)", fontsize=8)

    # "lower is better" annotation
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=6, ha="right", va="top",
            color="gray", style="italic")


def save_individual(machine: str):
    fig, ax = plt.subplots(figsize=(6, 4))
    plot_machine(machine, ax)
    # Add legend for individual plots
    handles = [mpatches.Patch(color=DRIVE_COLORS[d], label=DRIVE_LABELS[d])
               for d in DRIVE_ORDER if d in load_machine_data(machine)]
    ax.legend(handles=handles, fontsize=8, loc="upper left")
    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, f"k_times_{machine}.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


def save_combined():
    ncols  = 3
    nrows  = 4  # 3×4 = 12 cells, 11 machines, 1 empty
    fig, axes = plt.subplots(nrows, ncols, figsize=(18, 22))
    axes_flat = axes.flatten()

    for idx, machine in enumerate(MACHINE_ORDER):
        plot_machine(machine, axes_flat[idx])

    # Hide the last (empty) cell
    for idx in range(len(MACHINE_ORDER), len(axes_flat)):
        axes_flat[idx].set_visible(False)

    # Shared legend at the bottom
    legend_handles = [mpatches.Patch(color=DRIVE_COLORS[d], label=DRIVE_LABELS[d])
                      for d in DRIVE_ORDER]
    fig.legend(handles=legend_handles, loc="lower center",
               ncol=4, fontsize=11, frameon=True,
               bbox_to_anchor=(0.5, 0.01))

    fig.suptitle("K27–K32 Plot Times by Drive Type (all machines)",
                 fontsize=14, fontweight="bold", y=1.001)
    fig.tight_layout(rect=[0, 0.04, 1, 1])

    out = os.path.join(IMAGES_DIR, "k_times_combined.svg")
    fig.savefig(out, bbox_inches="tight")
    out_png = os.path.join(IMAGES_DIR, "k_times_combined.png")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    out_base = os.path.join(BASE_IMAGES_DIR, "k_times_combined.png")
    fig.savefig(out_base, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_base}")


if __name__ == "__main__":
    print("Generating k27-k32 time plots per machine …")
    for m in MACHINE_ORDER:
        save_individual(m)
    save_combined()
    print("Done.")
