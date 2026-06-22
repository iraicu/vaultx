#!/usr/bin/env python3
"""
Best VaultX k32 plotting time per machine per drive type.
One wide grouped bar chart: 4 bars per machine (HDD, SSD, NVME, CEPH),
3 bars for machines without SSD (opi5, rpi5). No title.
Drive order: HDD, SSD, NVME, CEPH.
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

# Drive CSV paths relative to RESULTS_DIR.
# Rules from context.md / k_times_by_drive.py:
#   - s8 HDD: nfs_hdd (faster than data-i)
#   - epycbox HDD: data-m (faster than nfs_hdd), NVME: sfatunmbi (local)
#   - torus NVME: ssd-raid0 (that drive is NVMe on torus), SSD: sfatunmbi
#   - fpganode2: no local HDD/NVME → use nfs_hdd / nfs_nvme; SSD: ssd-raid0
#   - rpi5 HDD: data-a (local, faster than nfs_hdd)
#   - rpi5 NVME: data-fast (local, faster than nfs_nvme)
MACHINE_DRIVES = {
    "s8": {
        "HDD":  "s8/k27-k32_nfs_hdd_CP384_IM.csv",
        "SSD":  "s8/k27-k32_ssd-raid0_CP384_IM.csv",
        "NVME": "s8/k27-k32_nvme-raid0_CP384_IM.csv",
        "CEPH": "s8/k27-k32_ceph_CP384_IM.csv",
    },
    "epycbox": {
        "HDD":  "epycbox/k27-k32_data-m_CP128_IM.csv",
        "SSD":  "epycbox/k27-k32_ssd-raid0_CP128_IM.csv",
        "NVME": "epycbox/k27-k32_sfatunmbi_CP128_IM.csv",
        "CEPH": "epycbox/k27-k32_ceph_CP128_IM.csv",
    },
    "athena": {
        "HDD":  "athena/k27-k32_HDD_CP48_IM.csv",
        "SSD":  "athena/k27-k32_sfatunmbi_CP48_IM.csv",
        "NVME": "athena/k27-k32_NVME_RAID-0_CP48_IM.csv",
        "CEPH": "athena/k27-k32_ceph_CP48_IM.csv",
    },
    "gpubox": {
        "HDD":  "gpubox/k27-k32_nfs_hdd_CP96_IM.csv",
        "SSD":  "gpubox/k27-k32_data-fast_CP96_IM.csv",
        "NVME": "gpubox/k27-k32_nfs_nvme_CP96_IM.csv",
        "CEPH": "gpubox/k27-k32_ceph_CP96_IM.csv",
    },
    "nvmebox": {
        "HDD":  "nvmebox/k27-k32_data-b_CP64_IM.csv",
        "SSD":  "nvmebox/k27-k32_data-fast_CP64_IM.csv",
        "NVME": "nvmebox/k27-k32_nfs_nvme_CP64_IM.csv",
        "CEPH": "nvmebox/k27-k32_ceph_CP64_IM.csv",
    },
    "thunderx1": {
        "HDD":  "thunderx1/k27-k32_nfs_hdd_CP96_IM.csv",
        "SSD":  "thunderx1/k27-k32_sfatunmbi_CP96_IM.csv",
        "NVME": "thunderx1/k27-k32_nfs_nvme_CP96_IM.csv",
        "CEPH": "thunderx1/k27-k32_ceph_CP96_IM.csv",
    },
    "thunderx2": {
        "HDD":  "thunderx2/k27-k32_nfs_hdd_CP224_IM.csv",
        "SSD":  "thunderx2/k27-k32_sfatunmbi_CP224_IM.csv",
        "NVME": "thunderx2/k27-k32_nfs_nvme_CP224_IM.csv",
        "CEPH": "thunderx2/k27-k32_ceph_CP224_IM.csv",
    },
    "torus": {
        "HDD":  "torus/k27-k32_data-c_CP32_IM.csv",
        "SSD":  "torus/k27-k32_sfatunmbi_CP32_IM.csv",
        "NVME": "torus/k27-k32_ssd-raid0_CP32_IM.csv",
        "CEPH": "torus/k27-k32_ceph_CP32_IM.csv",
    },
    "fpganode2": {
        "HDD":  "fpganode2/k27-k32_nfs_hdd_CP16_IM.csv",
        "SSD":  "fpganode2/k27-k32_ssd-raid0_CP16_IM.csv",
        "NVME": "fpganode2/k27-k32_nfs_nvme_CP16_IM.csv",
        "CEPH": "fpganode2/k27-k32_ceph_CP16_IM.csv",
    },
    "opi5": {
        "HDD":  "opi5/k27-k32_data-a_CP8_IM.csv",
        # no SSD on opi5
        "NVME": "opi5/k27-k32_data-fast_CP8_IM.csv",
        "CEPH": "opi5/k27-k32_ceph_CP8_IM.csv",
    },
    "rpi5": {
        "HDD":  "rpi5/k27-k32_data-a_CP4_IM.csv",
        # no SSD on rpi5
        "NVME": "rpi5/k27-k32_data-fast_CP4_IM.csv",
        "CEPH": "rpi5/k27-k32_ceph_CP4_IM.csv",
    },
}

MACHINE_ORDER = [
    "s8", "epycbox", "athena", "gpubox", "nvmebox",
    "thunderx1", "thunderx2", "torus", "fpganode2", "opi5", "rpi5",
]

DRIVE_ORDER = ["HDD", "SSD", "NVME", "CEPH"]

DRIVE_COLORS = {
    "HDD":  "#8B7355",   # brown
    "SSD":  "#2CA02C",   # green
    "NVME": "#1F77B4",   # blue
    "CEPH": "#FF7F0E",   # orange
}

DRIVE_LABELS = {"HDD": "HDD", "SSD": "SSD", "NVME": "NVME", "CEPH": "CEPH"}


def get_k32_time(rel_path: str) -> float | None:
    full = os.path.join(RESULTS_DIR, rel_path)
    if not os.path.exists(full):
        return None
    df = pd.read_csv(full)
    row = df[df["k"] == 32]
    if row.empty:
        return None
    return float(row["total_time_min"].values[0])


def main():
    # Collect k32 time per machine per drive
    data = {}
    for machine in MACHINE_ORDER:
        data[machine] = {}
        for drive, path in MACHINE_DRIVES[machine].items():
            t = get_k32_time(path)
            if t is not None:
                data[machine][drive] = t

    # Build grouped bar chart
    n_machines = len(MACHINE_ORDER)
    # Each machine slot: 4 possible drives, but some have fewer
    group_width = 4  # max bars per group
    bar_width   = 0.18

    fig, ax = plt.subplots(figsize=(28, 6))

    for m_idx, machine in enumerate(MACHINE_ORDER):
        drives_present = [d for d in DRIVE_ORDER if d in data[machine]]
        n_bars = len(drives_present)
        # Centre the group of bars within the machine slot
        offsets = np.linspace(-(n_bars - 1) / 2, (n_bars - 1) / 2, n_bars) * bar_width

        for bar_i, drive in enumerate(drives_present):
            t = data[machine][drive]
            x_pos = m_idx + offsets[bar_i]
            ax.bar(x_pos, t, width=bar_width * 0.9,
                   color=DRIVE_COLORS[drive], zorder=3,
                   edgecolor="white", linewidth=0.4)

    ax.set_xticks(range(n_machines))
    ax.set_xticklabels(MACHINE_ORDER, fontsize=9, rotation=15, ha="right")
    ax.set_ylabel("Time (min)", fontsize=11)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=8, ha="right", va="top",
            color="gray", style="italic")

    legend_handles = [
        mpatches.Patch(color=DRIVE_COLORS[d], label=DRIVE_LABELS[d])
        for d in DRIVE_ORDER
    ]
    ax.legend(handles=legend_handles, fontsize=9, loc="upper left", ncol=4)

    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, "k32_best_by_drive.svg")
    fig.savefig(out, bbox_inches="tight")
    out_png = os.path.join(IMAGES_DIR, "k32_best_by_drive.png")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    out_paper = os.path.join(PAPER_IMAGES_DIR, "k32_best_by_drive.png")
    fig.savefig(out_paper, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_paper}")


if __name__ == "__main__":
    print("Generating k32 best times by drive …")
    main()
    print("Done.")
