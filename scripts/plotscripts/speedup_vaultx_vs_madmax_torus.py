#!/usr/bin/env python3
"""
VaultX vs Madmax thread-scaling comparison on Torus.

Drive pairs:
  - NVME (ssd-raid0): VaultX varying_CP_k32_ssd-raid0_IM.csv  vs Madmax varying_threads_k32_madmax_nvme.csv
  - HDD  (nfs_hdd):   VaultX varying_CP_k32_nfs_hdd_IM.csv    vs Madmax varying_threads_k32_madmax.csv

Both tools share the same thread range: 2–32.
Figure suptitle carries "k32 VaultX vs Madmax — Torus"; per-panel titles use only drive + metric.
Saves individual PNGs + a combined PNG.
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np

SCRIPT_DIR   = os.path.dirname(os.path.abspath(__file__))
DATA_DIR     = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments", "torus")
IMAGES_DIR   = os.path.join(SCRIPT_DIR, "..", "..", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)

MIN_THREADS = 2
MAX_THREADS = 32

VAULTX_COLOR = "#1F77B4"
MADMAX_COLOR = "#D62728"


def load_vaultx(filename: str) -> pd.DataFrame:
    path = os.path.join(DATA_DIR, filename)
    df = pd.read_csv(path)
    df = df[(df["varying_threads"] >= MIN_THREADS) & (df["varying_threads"] <= MAX_THREADS)]
    return df.sort_values("varying_threads").reset_index(drop=True)


def load_madmax(filename: str) -> pd.DataFrame:
    path = os.path.join(DATA_DIR, "madmax", filename)
    df = pd.read_csv(path)
    df = df[(df["thread_count_r"] >= MIN_THREADS) & (df["thread_count_r"] <= MAX_THREADS)]
    return df.sort_values("thread_count_r").reset_index(drop=True)


def plot_time_panel(ax, vx_threads, vx_times, mm_threads, mm_times, drive_label):
    vx_ideal = vx_times[0] * vx_threads[0] / vx_threads
    ax.plot(vx_threads, vx_times, "o-", color=VAULTX_COLOR, linewidth=2, markersize=5,
            label="VaultX", zorder=3)
    ax.plot(vx_threads, vx_ideal, "--", color=VAULTX_COLOR, linewidth=1.0, alpha=0.45,
            zorder=2, label="Ideal (VaultX)")

    mm_ideal = mm_times[0] * mm_threads[0] / mm_threads
    ax.plot(mm_threads, mm_times, "s-", color=MADMAX_COLOR, linewidth=2, markersize=5,
            label="Madmax", zorder=3)
    ax.plot(mm_threads, mm_ideal, "--", color=MADMAX_COLOR, linewidth=1.0, alpha=0.45,
            zorder=2, label="Ideal (Madmax)")

    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.xaxis.set_major_formatter(plt.ScalarFormatter())
    ax.xaxis.set_minor_formatter(plt.NullFormatter())
    ax.yaxis.set_major_formatter(plt.ScalarFormatter())
    ax.yaxis.set_minor_formatter(plt.NullFormatter())

    all_threads = sorted(set(vx_threads.tolist() + mm_threads.tolist()))
    ax.set_xticks(all_threads)
    ax.set_xticklabels([str(t) for t in all_threads], fontsize=8, rotation=45, ha="right")

    ax.set_yticks([1, 10, 100, 1000])
    ax.set_yticklabels(["1", "10", "100", "1000"], fontsize=8)
    ax.set_ylim(
        min(vx_times.min(), mm_times.min()) * 0.7,
        max(vx_times.max(), mm_times.max()) * 2.5,
    )
    ax.set_xlabel("Thread count", fontsize=9)
    ax.set_ylabel("Time (min)", fontsize=9)
    ax.set_title(f"{drive_label} (thread scaling)", fontsize=11, fontweight="bold")
    ax.grid(True, which="both", linestyle="--", alpha=0.3)
    ax.legend(fontsize=8, loc="upper right")
    ax.text(0.99, 0.03, "↓ lower is better",
            transform=ax.transAxes, fontsize=6.5, ha="right", va="bottom",
            color="gray", style="italic")


def plot_memory_panel(ax, vx_threads, vx_mem_gb, mm_threads, mm_mem_gb, drive_label):
    ax.plot(vx_threads, vx_mem_gb, "o-", color=VAULTX_COLOR, linewidth=2, markersize=5,
            label="VaultX")
    ax.plot(mm_threads, mm_mem_gb, "s-", color=MADMAX_COLOR, linewidth=2, markersize=5,
            label="Madmax")

    ax.set_xscale("log", base=2)
    ax.xaxis.set_major_formatter(plt.ScalarFormatter())
    ax.xaxis.set_minor_formatter(plt.NullFormatter())
    all_threads = sorted(set(vx_threads.tolist() + mm_threads.tolist()))
    ax.set_xticks(all_threads)
    ax.set_xticklabels([str(t) for t in all_threads], fontsize=7, rotation=45, ha="right")

    ax.set_title("Peak Memory", fontsize=10, fontweight="bold")
    ax.set_xlabel("Thread count", fontsize=9)
    ax.set_ylabel("Peak memory (GB)", fontsize=9)
    ax.grid(True, which="both", linestyle="--", alpha=0.3)
    ax.legend(fontsize=8)


def build_figure(vx_df, mm_df, drive_label):
    vx_threads = vx_df["varying_threads"].values.astype(float)
    vx_times   = vx_df["total_time_min"].values.astype(float)
    vx_mem_gb  = vx_df["peak_memory_mb"].values.astype(float) / 1024.0

    mm_threads = mm_df["thread_count_r"].values.astype(float)
    mm_times   = mm_df["total_plot_time(min)"].values.astype(float)
    mm_mem_gb  = mm_df["peak_memory(MB)"].values.astype(float) / 1024.0

    fig = plt.figure(figsize=(10, 9))
    gs  = gridspec.GridSpec(2, 1, figure=fig, height_ratios=[3.0, 1.4], hspace=0.45)

    ax_time = fig.add_subplot(gs[0])
    ax_mem  = fig.add_subplot(gs[1])

    plot_time_panel(ax_time, vx_threads, vx_times, mm_threads, mm_times, drive_label)
    plot_memory_panel(ax_mem, vx_threads, vx_mem_gb, mm_threads, mm_mem_gb, drive_label)

    fig.suptitle("k32 VaultX vs Madmax — Torus", fontsize=12, fontweight="bold", y=1.01)
    return fig


def main():
    vx_nvme = load_vaultx("varying_CP_k32_ssd-raid0_IM.csv")
    vx_hdd  = load_vaultx("varying_CP_k32_nfs_hdd_IM.csv")
    mm_nvme = load_madmax("varying_threads_k32_madmax_nvme.csv")
    mm_hdd  = load_madmax("varying_threads_k32_madmax.csv")

    configs = [
        ("NVME", vx_nvme, mm_nvme, "nvme"),
        ("HDD",  vx_hdd,  mm_hdd,  "hdd"),
    ]

    for drive_label, vx_df, mm_df, suffix in configs:
        fig = build_figure(vx_df, mm_df, drive_label)
        out = os.path.join(IMAGES_DIR, f"speedup_vaultx_vs_madmax_torus_{suffix}.png")
        fig.savefig(out, dpi=180, bbox_inches="tight")
        plt.close(fig)
        print(f"  saved {out}")

    # Combined figure: NVME on top, HDD on bottom
    fig_c = plt.figure(figsize=(12, 20))
    gs_c  = gridspec.GridSpec(4, 1, figure=fig_c,
                               height_ratios=[3.0, 1.4, 3.0, 1.4], hspace=0.5)

    for (drive_label, vx_df, mm_df, _), r0 in zip(configs, [0, 2]):
        vx_threads = vx_df["varying_threads"].values.astype(float)
        vx_times   = vx_df["total_time_min"].values.astype(float)
        vx_mem_gb  = vx_df["peak_memory_mb"].values.astype(float) / 1024.0
        mm_threads = mm_df["thread_count_r"].values.astype(float)
        mm_times   = mm_df["total_plot_time(min)"].values.astype(float)
        mm_mem_gb  = mm_df["peak_memory(MB)"].values.astype(float) / 1024.0

        ax_time = fig_c.add_subplot(gs_c[r0])
        ax_mem  = fig_c.add_subplot(gs_c[r0 + 1])
        plot_time_panel(ax_time, vx_threads, vx_times, mm_threads, mm_times, drive_label)
        plot_memory_panel(ax_mem, vx_threads, vx_mem_gb, mm_threads, mm_mem_gb, drive_label)

    fig_c.suptitle("k32 VaultX vs Madmax — Torus (thread scaling)",
                   fontsize=13, fontweight="bold", y=0.99)
    out_c = os.path.join(IMAGES_DIR, "speedup_vaultx_vs_madmax_torus.png")
    fig_c.savefig(out_c, dpi=150, bbox_inches="tight")
    plt.close(fig_c)
    print(f"  saved {out_c}")


if __name__ == "__main__":
    print("Generating VaultX vs Madmax time-vs-threads plots for Torus …")
    main()
    print("Done.")
