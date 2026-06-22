#!/usr/bin/env python3
"""
K=32 generation time vs allocated memory: VaultX vs Bladebit (epycbox).
Two side-by-side subplots: NVME (left) and HDD (right), shared y-axis (log, 10–1000 min).
X-axis: memory allocation scale [2, 4, 8, 16, 32, 48 GB].
VaultX bars use actual allocations [2, 5, 8, 14, 26, 50 GB] mapped to those positions.
Bladebit bars at [4, 8, 16, 32, 48 GB] (no 2 GB: minimum is 4 GB).
"""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
import pandas as pd

SCRIPT_DIR       = os.path.dirname(os.path.abspath(__file__))
DATA_DIR         = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments", "epycbox")
IMAGES_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "images")
PAPER_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

# X-axis memory scale positions and labels
X_LABELS   = ["2 GB", "4 GB", "8 GB", "16 GB", "32 GB", "48 GB"]
X_POSITIONS = np.arange(len(X_LABELS))  # 0..5

# VaultX: actual memory values per x-position (None = no data)
VX_MEM_GB = [2, 5, 8, 14, 26, 50]

# Bladebit: actual memory values per x-position (None = no data at x=0)
BB_MEM_GB = [None, 4, 8, 16, 32, 48]

VX_COLOR = "#1F77B4"   # blue
BB_COLOR = "#2CA02C"   # green

BAR_WIDTH = 0.35


def load_vx(drive_file: str) -> dict[int, float]:
    """Return {memory_gb: total_time_min} from a VaultX varying-memory CSV."""
    path = os.path.join(DATA_DIR, drive_file)
    df = pd.read_csv(path)
    return dict(zip(df["memory_gb"].astype(int), df["total_time_min"]))


def load_bb(drive_key: str) -> dict[int, float]:
    """Return {cache_gb: total_plot_time_min} from the Bladebit CSV for one drive."""
    path = os.path.join(DATA_DIR, "bladebit", "bladebit_varying_memory_k32.csv")
    df = pd.read_csv(path)
    # cache_size column looks like "48G"; filter rows whose temp_dir contains drive_key
    subset = df[df["temp_dir"].str.contains(drive_key, na=False)].copy()
    subset["cache_gb"] = subset["cache_size"].str.replace("G", "").astype(int)
    return dict(zip(subset["cache_gb"], subset["total_plot_time(min)"]))


def draw_subplot(ax: plt.Axes, vx_data: dict, bb_data: dict, title: str,
                 show_ylabel: bool = True):
    for xi, (vx_mem, bb_mem) in enumerate(zip(VX_MEM_GB, BB_MEM_GB)):
        # VaultX bar (always present)
        vx_time = vx_data.get(vx_mem)
        if vx_time is not None:
            ax.bar(xi - BAR_WIDTH / 2, vx_time, width=BAR_WIDTH,
                   color=VX_COLOR, zorder=3, edgecolor="white", linewidth=0.4)
            ax.text(xi - BAR_WIDTH / 2, vx_time * 1.25,
                    f"{vx_time:.1f}m", ha="center", va="bottom", fontsize=6.5,
                    color=VX_COLOR, fontweight="bold", zorder=5)

        # Bladebit bar (absent at xi=0)
        if bb_mem is not None:
            bb_time = bb_data.get(bb_mem)
            if bb_time is not None:
                ax.bar(xi + BAR_WIDTH / 2, bb_time, width=BAR_WIDTH,
                       color=BB_COLOR, zorder=3, edgecolor="white", linewidth=0.4)
                ax.text(xi + BAR_WIDTH / 2, bb_time * 1.25,
                        f"{bb_time:.0f}m", ha="center", va="bottom", fontsize=6.5,
                        color=BB_COLOR, fontweight="bold", zorder=5)

    ax.set_yscale("log")
    ax.set_ylim(1, 3000)
    ax.set_yticks([1, 10, 100, 1000])
    ax.yaxis.set_major_formatter(matplotlib.ticker.ScalarFormatter())
    ax.yaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
    ax.set_xticks(X_POSITIONS)
    ax.set_xticklabels(X_LABELS, fontsize=8)
    if show_ylabel:
        ax.set_ylabel("Time (minutes, log scale)", fontsize=9)
    ax.set_title(title, fontsize=10, fontweight="bold")
    ax.grid(axis="y", linestyle="--", alpha=0.35, zorder=0)
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=7, ha="right", va="top",
            color="gray", style="italic")


def main():
    vx_nvme = load_vx("varying_memory_k32_CP128_IO1_sfatunmbi.csv")
    vx_hdd  = load_vx("varying_memory_k32_CP128_IO1_data-m.csv")
    bb_nvme = load_bb("/nfs_nvme/")
    bb_hdd  = load_bb("/data-m/")

    fig, (ax_nvme, ax_hdd) = plt.subplots(1, 2, figsize=(12, 5), sharey=True)
    fig.suptitle("K=32 Gen time vs Memory: VX/Bladebit (Epycbox)",
                 fontsize=11, fontweight="bold")

    draw_subplot(ax_nvme, vx_nvme, bb_nvme, "NVMe", show_ylabel=True)
    draw_subplot(ax_hdd,  vx_hdd,  bb_hdd,  "HDD",  show_ylabel=False)

    legend_handles = [
        mpatches.Patch(color=VX_COLOR, label="VaultX"),
        mpatches.Patch(color=BB_COLOR, label="Bladebit"),
    ]
    fig.legend(handles=legend_handles, loc="lower center", ncol=2,
               fontsize=9, frameon=True, bbox_to_anchor=(0.5, -0.04))

    fig.tight_layout(rect=[0, 0.05, 1, 1])

    base = "vaultx_vs_bladebit_memory_k32"
    out     = os.path.join(IMAGES_DIR, f"{base}.png")
    out_p   = os.path.join(PAPER_IMAGES_DIR, f"{base}.png")
    fig.savefig(out,   dpi=300, bbox_inches="tight")
    fig.savefig(out_p, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_p}")


if __name__ == "__main__":
    print("Generating VaultX vs Bladebit memory comparison …")
    main()
    print("Done.")
