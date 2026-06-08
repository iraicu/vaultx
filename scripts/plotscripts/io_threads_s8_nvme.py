#!/usr/bin/env python3
"""
Task 8: IO thread count vs k32 plot time on s8 NVME, with compute threads fixed at max (384).
Bar chart showing that increasing IO threads gives no benefit (or worsens performance).
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

IO_CSV = "s8/varying_IO_k32_nvme-raid0_IM.csv"


def main():
    df = pd.read_csv(os.path.join(RESULTS_DIR, IO_CSV))
    df = df.sort_values("threads_io").reset_index(drop=True)

    io_threads = df["threads_io"].values
    times      = df["total_time_min"].values

    x  = np.arange(len(io_threads))
    bw = 0.6

    fig, ax = plt.subplots(figsize=(10, 5))

    bars = ax.bar(x, times, width=bw, color="#1F77B4", zorder=3,
                  edgecolor="white", linewidth=0.5)

    # Annotate bars with time values
    max_t = max(times)
    for xi, t in enumerate(times):
        ax.text(xi, t + max_t * 0.01, f"{t:.2f}m",
                ha="center", va="bottom", fontsize=8)

    # Reference line at IO=1 (baseline)
    baseline = times[0]
    ax.axhline(baseline, color="#D62728", linestyle="--", linewidth=1.2,
               alpha=0.7, label=f"Baseline (IO=1): {baseline:.2f} min")

    # y-axis ticks
    step_candidates = [0.2, 0.5, 1]
    for step in step_candidates:
        ticks = np.arange(0, max_t * 1.3 + step, step)
        if 5 <= len(ticks) <= 12:
            break
    ax.set_yticks(ticks)
    ax.set_ylim(0, ticks[-1])

    ax.set_xticks(x)
    ax.set_xticklabels([str(t) for t in io_threads], fontsize=9)
    ax.set_xlabel("IO Thread Count (compute threads fixed at 384)", fontsize=10)
    ax.set_ylabel("Time (minutes)", fontsize=10)
    ax.set_title("IO Thread Scaling — k32 on NVME",
                 fontsize=12, fontweight="bold")
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.legend(fontsize=8, loc="upper right")
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=8, ha="right", va="top",
            color="gray", style="italic")

    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, "io_threads_k32_s8_nvme.png")
    fig.savefig(out, dpi=180, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


if __name__ == "__main__":
    print("Generating IO threads plot …")
    main()
    print("Done.")
