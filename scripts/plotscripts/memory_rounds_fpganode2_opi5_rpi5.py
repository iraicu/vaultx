#!/usr/bin/env python3
"""
OOM approach: k32 time vs memory allocation on fpganode2, opi5, rpi5.
3 bars per memory x-tick (one per machine). 4 drive-type subplots.
rpi5 limited to ≤5 GB (7 GB run has same peak mem as 5 GB — OOM plateau).
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
IMAGES_DIR  = os.path.join(SCRIPT_DIR, "..", "..", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)

# {drive: {machine: csv_path_relative_to_RESULTS_DIR}}
# fpganode2: HDD = nfs_hdd, SSD = ssd-raid0, NVME = nfs_nvme, CEPH = ceph
# opi5:      HDD = data-a (local, no SSD)
# rpi5:      HDD = data-a (local, faster than nfs_hdd; no SSD)
DRIVE_CSVS = {
    "HDD": {
        "fpganode2": "fpganode2/varying_memory_k32_CP16_IO1_nfs_hdd.csv",
        "opi5":      "opi5/varying_memory_k32_CP8_IO1_data-a.csv",
        "rpi5":      "rpi5/varying_memory_k32_CP4_IO1_data-a.csv",
    },
    "SSD": {
        "fpganode2": "fpganode2/varying_memory_k32_CP16_IO1_ssd-raid0.csv",
        # opi5 and rpi5 have no SSD
    },
    "NVME": {
        "fpganode2": "fpganode2/varying_memory_k32_CP16_IO1_nfs_nvme.csv",
        "opi5":      "opi5/varying_memory_k32_CP8_IO1_data-fast.csv",
        "rpi5":      "rpi5/varying_memory_k32_CP4_IO1_data-fast.csv",
    },
    "CEPH": {
        "fpganode2": "fpganode2/varying_memory_k32_CP16_IO1_ceph.csv",
        "opi5":      "opi5/varying_memory_k32_CP8_IO1_ceph.csv",
        "rpi5":      "rpi5/varying_memory_k32_CP4_IO1_ceph.csv",
    },
}

DRIVE_ORDER  = ["HDD", "SSD", "NVME", "CEPH"]
DRIVE_TITLES = {"HDD": "HDD", "SSD": "SSD", "NVME": "NVME", "CEPH": "CEPH"}

MACHINE_ORDER  = ["fpganode2", "opi5", "rpi5"]
MACHINE_COLORS = {
    "fpganode2": "#BCBD22",   # yellow-green
    "opi5":      "#17BECF",   # teal
    "rpi5":      "#AEC7E8",   # light blue
}
MACHINE_LABELS = {
    "fpganode2": "fpganode2 (16T/32GB)",
    "opi5":      "opi5 (8T/32GB)",
    "rpi5":      "rpi5 (4T/8GB)",
}

MAX_MEM_GB      = 26   # cap for all machines
RPI5_MAX_MEM_GB = 8    # rpi5 has data up to 8 GB (k29 / N=8 data point)


def _parse_numeric(series: pd.Series) -> pd.Series:
    """Coerce to float, tolerating malformed entries like '753.46.01'."""
    def _safe(v):
        try:
            return float(v)
        except (ValueError, TypeError):
            s = str(v).strip()
            parts = s.split(".")
            if len(parts) >= 2:
                try:
                    return float(f"{parts[0]}.{parts[1]}")
                except ValueError:
                    pass
            return float("nan")
    return series.map(_safe)


def compute_rounds(row) -> int:
    wb  = row["write_batch_mb"]
    eff = row["storage_efficiency_pct"]
    if wb < 1024:
        return int((32 * 1024) / wb / 2)
    return max(1, round(eff / 100))


def load_and_filter(machine: str, rel_path: str) -> pd.DataFrame | None:
    full = os.path.join(RESULTS_DIR, rel_path)
    if not os.path.exists(full):
        return None
    df = pd.read_csv(full)
    cap = RPI5_MAX_MEM_GB if machine == "rpi5" else MAX_MEM_GB
    df  = df[df["memory_gb"] <= cap].copy()
    # Robust numeric coercion — handles malformed values like "753.46.01"
    for col in ("storage_efficiency_pct", "write_batch_mb", "total_time_min"):
        df[col] = _parse_numeric(df[col])
    df = df.dropna(subset=["total_time_min"])
    df["rounds"] = df.apply(compute_rounds, axis=1)
    df = df.sort_values("memory_gb").reset_index(drop=True)
    return df if not df.empty else None


def plot_drive(drive: str, ax: plt.Axes, show_legend: bool = True):
    machine_dfs = {}
    for machine in MACHINE_ORDER:
        rel = DRIVE_CSVS.get(drive, {}).get(machine)
        if rel:
            df = load_and_filter(machine, rel)
            if df is not None:
                machine_dfs[machine] = df

    if not machine_dfs:
        ax.text(0.5, 0.5, "No data", transform=ax.transAxes,
                ha="center", va="center", color="gray")
        ax.set_title(DRIVE_TITLES[drive], fontsize=11, fontweight="bold")
        return

    # Build union of all memory values (sorted) as x positions
    all_mem = sorted(set(
        float(v) for df in machine_dfs.values() for v in df["memory_gb"]
    ))
    x      = np.arange(len(all_mem))
    n_mach = len(machine_dfs)
    bw     = 0.7 / max(n_mach, 1)

    for i, machine in enumerate(m for m in MACHINE_ORDER if m in machine_dfs):
        df     = machine_dfs[machine]
        offset = (i - (n_mach - 1) / 2) * bw
        times  = []
        for mem in all_mem:
            row = df[df["memory_gb"].astype(float).round(4) == round(mem, 4)]
            times.append(float(row["total_time_min"].values[0]) if not row.empty else 0.0)
        ax.bar(x + offset, times, width=bw * 0.9,
               color=MACHINE_COLORS[machine], label=MACHINE_LABELS[machine],
               zorder=3)

    # X-tick: show memory GB (and rounds from first available machine)
    ref_df = next(iter(machine_dfs.values()))
    mem_to_rounds = dict(zip(
        ref_df["memory_gb"].astype(float).round(4),
        ref_df["rounds"]
    ))
    tick_labels = [f"{m:g}GB\n({mem_to_rounds.get(round(m, 4), '?')}R)" for m in all_mem]
    ax.set_xticks(x)
    ax.set_xticklabels(tick_labels, fontsize=7)
    ax.set_xlabel("Memory allocated (rounds)", fontsize=9)
    ax.set_ylabel("Time (min)", fontsize=9)
    ax.set_title(DRIVE_TITLES[drive], fontsize=11, fontweight="bold")
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=6.5, ha="right", va="top",
            color="gray", style="italic")
    if show_legend:
        ax.legend(fontsize=8)


def save_individual(drive: str):
    fig, ax = plt.subplots(figsize=(9, 5))
    plot_drive(drive, ax, show_legend=True)
    fig.tight_layout()
    out = os.path.join(IMAGES_DIR,
                       f"memory_rounds_{drive.lower()}_fpganode2_opi5_rpi5.png")
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
                       "memory_rounds_combined_fpganode2_opi5_rpi5.svg")
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


if __name__ == "__main__":
    print("Generating OOM memory/rounds plots (fpganode2, opi5, rpi5) …")
    for drive in DRIVE_ORDER:
        save_individual(drive)
    save_combined()
    print("Done.")
