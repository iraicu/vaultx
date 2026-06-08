#!/usr/bin/env python3
"""
Task 5: Compare time to plot k32 with varying memory (rounds) on s8 vs opi5.
4 individual drive plots + 1 combined.  Both machines use NVME as temp/compute drive;
the destination drive varies per plot.  Memory above 26 GB excluded.

Rounds are computed per row:
  - write_batch_mb < 1024 → rounds = (32*1024) / write_batch_mb / 2  (exact match to user's examples)
  - write_batch_mb == 1024 → rounds = round(storage_efficiency_pct / 100)
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Results")
IMAGES_DIR  = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)

# Drive mapping: {drive_type: {machine: csv_relative_path}}
# All experiments write to the listed drive; NVME is always the compute/temp side.
DRIVE_CSVS = {
    "NVME": {
        "s8":   "s8/varying_memory_k32_CP384_IO1_nvme-raid0.csv",
        "opi5": "opi5/varying_memory_k32_CP8_IO1_data-fast.csv",
    },
    "HDD": {
        "s8":   "s8/varying_memory_k32_CP384_IO1_nfs_hdd.csv",
        "opi5": "opi5/varying_memory_k32_CP8_IO1_data-a.csv",
    },
    "SSD": {
        "s8":   "s8/varying_memory_k32_CP384_IO1_ssd-raid0.csv",
        # opi5 has no SSD
    },
    "CEPH_HDD": {
        "s8":   "s8/varying_memory_k32_CP384_IO1_ceph.csv",
        "opi5": "opi5/varying_memory_k32_CP8_IO1_ceph.csv",
    },
}

DRIVE_ORDER  = ["HDD", "CEPH_HDD", "SSD", "NVME"]
DRIVE_TITLES = {
    "HDD":      "HDD",
    "CEPH_HDD": "CEPH",
    "SSD":      "SSD",
    "NVME":     "NVME",
}

MACHINE_COLORS = {"s8": "#1F77B4", "opi5": "#2CA02C"}  # blue, green
MACHINE_LABELS = {"s8": "s8 (384T / 770GB)", "opi5": "opi5 (8T / 32GB)"}
MAX_MEMORY_GB  = 26   # do not plot above this


def compute_rounds(row) -> int:
    wb  = row["write_batch_mb"]
    eff = row["storage_efficiency_pct"]
    if wb < 1024:
        return int((32 * 1024) / wb / 2)
    return max(1, round(eff / 100))


def load_and_filter(rel_path: str) -> pd.DataFrame:
    df = pd.read_csv(os.path.join(RESULTS_DIR, rel_path))
    df = df[df["memory_gb"] <= MAX_MEMORY_GB].copy()
    df["rounds"] = df.apply(compute_rounds, axis=1)
    df = df.sort_values("memory_gb").reset_index(drop=True)
    return df


def x_labels(df: pd.DataFrame) -> list[str]:
    return [f"{row.memory_gb:g}GB\n({int(row.rounds)}R)"
            for _, row in df.iterrows()]


def plot_drive(drive: str, ax: plt.Axes, show_legend: bool = True):
    machine_dfs = {}
    for machine, rel_path in DRIVE_CSVS.get(drive, {}).items():
        full = os.path.join(RESULTS_DIR, rel_path)
        if os.path.exists(full):
            machine_dfs[machine] = load_and_filter(rel_path)

    if not machine_dfs:
        ax.text(0.5, 0.5, "No data", transform=ax.transAxes,
                ha="center", va="center", color="gray")
        ax.set_title(DRIVE_TITLES[drive], fontsize=11, fontweight="bold")
        return

    # Use the first machine's memory values to define x positions;
    # align other machines by memory_gb.
    ref_df = next(iter(machine_dfs.values()))
    mem_vals = ref_df["memory_gb"].values
    n_mem  = len(mem_vals)
    x      = np.arange(n_mem)
    n_mach = len(machine_dfs)
    bw     = 0.7 / max(n_mach, 1)

    for i, (machine, df) in enumerate(machine_dfs.items()):
        # Align df to mem_vals
        merged = pd.DataFrame({"memory_gb": mem_vals.astype(float)}).merge(
            df[["memory_gb", "total_time_min", "rounds"]].assign(
                memory_gb=lambda d: d["memory_gb"].astype(float)),
            on="memory_gb", how="left")
        times = merged["total_time_min"].values
        offset = (i - (n_mach - 1) / 2) * bw
        ax.bar(x + offset, times, width=bw * 0.9,
               color=MACHINE_COLORS[machine], label=MACHINE_LABELS[machine],
               zorder=3)

    # X-tick labels: show memory + rounds for ref machine
    rounds_vals = ref_df["rounds"].values
    tick_labels = [f"{m:g}GB\n({r}R)" for m, r in zip(mem_vals, rounds_vals)]
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
    fig, ax = plt.subplots(figsize=(8, 5))
    plot_drive(drive, ax, show_legend=True)
    fig.tight_layout()
    out = os.path.join(IMAGES_DIR,
                       f"memory_rounds_{drive.lower()}_s8_vs_opi5.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


def save_combined():
    fig, axes = plt.subplots(1, 4, figsize=(24, 6))
    for ax, drive in zip(axes, DRIVE_ORDER):
        plot_drive(drive, ax, show_legend=False)

    # Shared legend
    import matplotlib.patches as mpatches
    handles = [mpatches.Patch(color=MACHINE_COLORS[m], label=MACHINE_LABELS[m])
               for m in ["s8", "opi5"]]
    fig.legend(handles=handles, loc="lower center", ncol=2,
               fontsize=10, frameon=True, bbox_to_anchor=(0.5, -0.06))
    fig.suptitle("K32 Time vs Memory Allocation (s8 vs opi5, per drive type)",
                 fontsize=13, fontweight="bold")
    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, "memory_rounds_combined_s8_vs_opi5.svg")
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


if __name__ == "__main__":
    print("Generating memory/rounds comparison plots …")
    for drive in DRIVE_ORDER:
        save_individual(drive)
    save_combined()
    print("Done.")
