#!/usr/bin/env python3
"""
Drive comparison for Eightsocket/s8 (Madmax): time, power, and peak memory vs thread count.
HDD = ceph (treated as HDD-class), NVME = nvme-raid0.

Three-panel figure:
  1. Time (minutes)
  2. Average power (W)
  3. Peak memory (GB)
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_FILE  = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments",
                          "s8", "others", "madmax", "varying_threads_k32_madmax_s8.csv")
IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)

NVME_COLOR = "#1F77B4"
HDD_COLOR  = "#D62728"


def load_s8():
    df = pd.read_csv(DATA_FILE)
    nvme = df[df["temp_dir"].str.contains("nvme-raid0", na=False)].copy()
    hdd  = df[df["temp_dir"].str.contains("ceph", na=False)].copy()
    nvme = nvme.sort_values("thread_count_r").reset_index(drop=True)
    hdd  = hdd.sort_values("thread_count_r").reset_index(drop=True)
    return nvme, hdd


def main():
    nvme, hdd = load_s8()

    threads_nvme = nvme["thread_count_r"].values
    threads_hdd  = hdd["thread_count_r"].values
    all_threads  = sorted(set(threads_nvme.tolist() + threads_hdd.tolist()))

    time_nvme = nvme["total_plot_time(min)"].values
    time_hdd  = hdd["total_plot_time(min)"].values

    power_nvme = nvme["avg_power"].values.astype(float)
    power_hdd  = hdd["avg_power"].values.astype(float)

    mem_nvme = nvme["peak_memory"].values.astype(float) / 1024.0
    mem_hdd  = hdd["peak_memory"].values.astype(float) / 1024.0

    fig = plt.figure(figsize=(10, 12))
    gs  = gridspec.GridSpec(3, 1, figure=fig, hspace=0.45)

    # ── Time ────────────────────────────────────────────────────────────────
    ax_t = fig.add_subplot(gs[0])
    ax_t.plot(threads_nvme, time_nvme, "o-", color=NVME_COLOR, linewidth=2,
              markersize=5, label="NVME")
    ax_t.plot(threads_hdd, time_hdd, "s-", color=HDD_COLOR, linewidth=2,
              markersize=5, label="HDD")
    ax_t.set_xscale("log", base=2)
    ax_t.xaxis.set_major_formatter(plt.ScalarFormatter())
    ax_t.xaxis.set_minor_formatter(plt.NullFormatter())
    ax_t.set_xticks(all_threads)
    ax_t.set_xticklabels([str(t) for t in all_threads], fontsize=8, rotation=45, ha="right")
    ax_t.set_xlabel("Thread count", fontsize=9)
    ax_t.set_ylabel("Time (minutes)", fontsize=9)
    ax_t.set_title("Time vs Threads", fontsize=11, fontweight="bold")
    ax_t.grid(True, linestyle="--", alpha=0.3)
    ax_t.legend(fontsize=8)
    ax_t.text(0.99, 0.97, "↓ lower is better",
              transform=ax_t.transAxes, fontsize=6.5, ha="right", va="top",
              color="gray", style="italic")

    # ── Power ───────────────────────────────────────────────────────────────
    ax_p = fig.add_subplot(gs[1])
    ax_p.plot(threads_nvme, power_nvme, "o-", color=NVME_COLOR, linewidth=2,
              markersize=5, label="NVME")
    ax_p.plot(threads_hdd, power_hdd, "s-", color=HDD_COLOR, linewidth=2,
              markersize=5, label="HDD")
    ax_p.set_xscale("log", base=2)
    ax_p.xaxis.set_major_formatter(plt.ScalarFormatter())
    ax_p.xaxis.set_minor_formatter(plt.NullFormatter())
    ax_p.set_xticks(all_threads)
    ax_p.set_xticklabels([str(t) for t in all_threads], fontsize=8, rotation=45, ha="right")
    ax_p.set_xlabel("Thread count", fontsize=9)
    ax_p.set_ylabel("Avg power (W)", fontsize=9)
    ax_p.set_title("Avg Power vs Threads", fontsize=11, fontweight="bold")
    ax_p.grid(True, linestyle="--", alpha=0.3)
    ax_p.legend(fontsize=8)

    # ── Peak Memory ─────────────────────────────────────────────────────────
    ax_m = fig.add_subplot(gs[2])
    ax_m.plot(threads_nvme, mem_nvme, "o-", color=NVME_COLOR, linewidth=2,
              markersize=5, label="NVME")
    ax_m.plot(threads_hdd, mem_hdd, "s-", color=HDD_COLOR, linewidth=2,
              markersize=5, label="HDD")
    ax_m.set_xscale("log", base=2)
    ax_m.xaxis.set_major_formatter(plt.ScalarFormatter())
    ax_m.xaxis.set_minor_formatter(plt.NullFormatter())
    ax_m.set_xticks(all_threads)
    ax_m.set_xticklabels([str(t) for t in all_threads], fontsize=8, rotation=45, ha="right")
    ax_m.set_xlabel("Thread count", fontsize=9)
    ax_m.set_ylabel("Peak memory (GB)", fontsize=9)
    ax_m.set_title("Peak Memory vs Threads", fontsize=11, fontweight="bold")
    ax_m.grid(True, linestyle="--", alpha=0.3)
    ax_m.legend(fontsize=8)

    fig.suptitle("Eightsocket: Madmax k32 (HDD vs NVME)",
                 fontsize=13, fontweight="bold", y=0.995)

    out = os.path.join(IMAGES_DIR, "drive_comparison_s8.png")
    fig.savefig(out, dpi=180, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


if __name__ == "__main__":
    print("Generating s8 drive comparison …")
    main()
    print("Done.")
