#!/usr/bin/env python3
"""
Task 4: Time-vs-threads comparison – VaultX vs Madmax on s8, for HDD and NVME.
For each drive type:
  - Top panel (merged):  both VaultX and Madmax time on one axes.
    X-axis: log-2 (equal spacing per doubling).
    Y-axis: log scale (ticks at 1, 10, 100) so ideal appears as straight diagonal.
    Each tool has its own ideal t_baseline/N line.
  - Bottom panel (full width): Peak memory for both tools.
Saves individual drive PNGs + one combined PNG.
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments")
IMAGES_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "images")
PAPER_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

VAULTX_NVME_CSV = "s8/varying_CP_k32_nvme-raid0_IM.csv"
VAULTX_HDD_CSV  = "s8/varying_CP_k32_data-i_IM.csv"
MADMAX_CSV      = "s8/others/madmax/varying_threads_k32_madmax_s8.csv"

MIN_THREADS  = 8
VAULTX_COLOR = "#1F77B4"
MADMAX_COLOR = "#D62728"
IDEAL_COLOR  = "#7F7F7F"


def load_vaultx(rel_path: str) -> pd.DataFrame:
    df = pd.read_csv(os.path.join(RESULTS_DIR, rel_path))
    df = df[df["varying_threads"] >= MIN_THREADS]
    return df.sort_values("varying_threads").reset_index(drop=True)


def load_madmax(drive_keyword: str) -> pd.DataFrame:
    df = pd.read_csv(os.path.join(RESULTS_DIR, MADMAX_CSV))
    df = df[df["temp_dir"].str.contains(drive_keyword, na=False)]
    return df.sort_values("thread_count_r").reset_index(drop=True)


def plot_merged_time_panel(ax: plt.Axes,
                           vx_threads: np.ndarray, vx_times: np.ndarray,
                           mm_threads: np.ndarray, mm_times: np.ndarray,
                           drive_label: str):
    """
    Both VaultX and Madmax on one axes.
    Log-2 x-axis, log y-axis (1→10→100 ticks).
    Each tool has its own ideal t_baseline * n_baseline / N line.
    """
    # VaultX
    vx_t0 = vx_times[0]; vx_n0 = vx_threads[0]
    vx_ideal = vx_t0 * vx_n0 / vx_threads
    ax.plot(vx_threads, vx_times, "o-", color=VAULTX_COLOR, linewidth=2,
            markersize=5, label="VaultX", zorder=3)
    ax.plot(vx_threads, vx_ideal, "--", color=VAULTX_COLOR, linewidth=1.0,
            alpha=0.45, zorder=2, label="Ideal")

    # Madmax
    mm_t0 = mm_times[0]; mm_n0 = mm_threads[0]
    mm_ideal = mm_t0 * mm_n0 / mm_threads
    ax.plot(mm_threads, mm_times, "s-", color=MADMAX_COLOR, linewidth=2,
            markersize=5, label="Madmax", zorder=3)
    ax.plot(mm_threads, mm_ideal, "--", color=MADMAX_COLOR, linewidth=1.0,
            alpha=0.45, zorder=2)   # no extra label (paired with its solid line)

    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.xaxis.set_major_formatter(plt.ScalarFormatter())
    ax.xaxis.set_minor_formatter(plt.NullFormatter())
    ax.yaxis.set_major_formatter(plt.ScalarFormatter())
    ax.yaxis.set_minor_formatter(plt.NullFormatter())

    all_threads = sorted(set(vx_threads.tolist() + mm_threads.tolist()))
    ax.set_xticks(all_threads)
    ax.set_xticklabels([str(t) for t in all_threads], fontsize=8,
                       rotation=45, ha="right")

    # Y-ticks: 1, 10, 100 (and minor grid for clarity)
    ax.set_yticks([1, 10, 100])
    ax.set_yticklabels(["1", "10", "100"], fontsize=8)
    ax.set_ylim(
        min(vx_times.min(), mm_times.min()) * 0.7,
        max(vx_times.max(), mm_times.max()) * 2.5
    )

    ax.set_xlabel("Thread count", fontsize=9)
    ax.set_ylabel("Time", fontsize=9)
    ax.set_title(f"k32 VaultX vs Madmax - {drive_label} (thread scaling)",
                 fontsize=11, fontweight="bold")
    ax.grid(True, which="both", linestyle="--", alpha=0.3)
    ax.legend(fontsize=8, loc="upper right")
    ax.text(0.99, 0.03, "↓ lower is better",
            transform=ax.transAxes, fontsize=6.5,
            ha="right", va="bottom", color="gray", style="italic")


def plot_memory_panel(ax: plt.Axes,
                      vx_threads: np.ndarray, vx_mem_gb: np.ndarray,
                      mm_threads: np.ndarray, mm_mem_gb: np.ndarray,
                      drive_label: str):
    ax.plot(vx_threads, vx_mem_gb, "o-", color=VAULTX_COLOR, linewidth=2,
            markersize=5, label="VaultX")
    ax.plot(mm_threads, mm_mem_gb, "s-", color=MADMAX_COLOR, linewidth=2,
            markersize=5, label="Madmax")

    ax.set_xscale("log", base=2)
    ax.xaxis.set_major_formatter(plt.ScalarFormatter())
    ax.xaxis.set_minor_formatter(plt.NullFormatter())
    all_threads = sorted(set(vx_threads.tolist() + mm_threads.tolist()))
    ax.set_xticks(all_threads)
    ax.set_xticklabels([str(t) for t in all_threads], fontsize=7,
                       rotation=45, ha="right")

    ax.set_title("Peak Memory", fontsize=10, fontweight="bold")
    ax.set_xlabel("Thread count", fontsize=9)
    ax.set_ylabel("Peak memory (GB)", fontsize=9)
    ax.grid(True, which="both", linestyle="--", alpha=0.3)
    ax.legend(fontsize=8)


def build_drive_figure(vx_df: pd.DataFrame, mm_df: pd.DataFrame,
                       drive_label: str) -> plt.Figure:
    vx_threads = vx_df["varying_threads"].values
    vx_times   = vx_df["total_time_min"].values
    vx_mem_gb  = vx_df["peak_memory_mb"].values / 1024.0

    mm_threads = mm_df["thread_count_r"].values
    mm_times   = mm_df["total_plot_time(min)"].values
    mm_mem_gb  = mm_df["peak_memory"].values / 1024.0

    fig = plt.figure(figsize=(10, 9))
    gs  = gridspec.GridSpec(2, 1, figure=fig,
                            height_ratios=[3.0, 1.4], hspace=0.45)

    ax_time = fig.add_subplot(gs[0])
    ax_mem  = fig.add_subplot(gs[1])

    plot_merged_time_panel(ax_time, vx_threads, vx_times,
                           mm_threads, mm_times, drive_label)
    plot_memory_panel(ax_mem, vx_threads, vx_mem_gb,
                      mm_threads, mm_mem_gb, drive_label)

    fig.suptitle("")
    return fig


def main():
    vx_nvme = load_vaultx(VAULTX_NVME_CSV)
    vx_hdd  = load_vaultx(VAULTX_HDD_CSV)
    mm_nvme = load_madmax("nvme-raid0")
    mm_ceph = load_madmax("ceph")

    for drive_label, vx_df, mm_df, suffix in [
        ("NVME", vx_nvme, mm_nvme, "nvme"),
        ("HDD", vx_hdd, mm_ceph, "hdd"),
    ]:
        fig = build_drive_figure(vx_df, mm_df, drive_label)
        out = os.path.join(IMAGES_DIR, f"speedup_vaultx_vs_madmax_{suffix}.png")
        fig.savefig(out, dpi=180, bbox_inches="tight")
        plt.close(fig)
        print(f"  saved {out}")

    # Combined: NVME left, HDD right; same y-scale on time panels and on memory panels
    fig_c = plt.figure(figsize=(18, 9))
    gs_c  = gridspec.GridSpec(2, 2, figure=fig_c,
                               height_ratios=[3.0, 1.4],
                               hspace=0.32, wspace=0.22)

    configs = [
        ("NVME", vx_nvme, mm_nvme, 0),  # left column
        ("HDD",  vx_hdd,  mm_ceph, 1),  # right column
    ]

    ax_times = []
    ax_mems  = []
    for dlabel, vx_df, mm_df, col in configs:
        vx_threads = vx_df["varying_threads"].values
        vx_times   = vx_df["total_time_min"].values
        vx_mem_gb  = vx_df["peak_memory_mb"].values / 1024.0
        mm_threads = mm_df["thread_count_r"].values
        mm_times   = mm_df["total_plot_time(min)"].values
        mm_mem_gb  = mm_df["peak_memory"].values / 1024.0

        ax_time = fig_c.add_subplot(gs_c[0, col])
        ax_mem  = fig_c.add_subplot(gs_c[1, col])

        plot_merged_time_panel(ax_time, vx_threads, vx_times,
                               mm_threads, mm_times, dlabel)
        plot_memory_panel(ax_mem, vx_threads, vx_mem_gb,
                          mm_threads, mm_mem_gb, dlabel)

        ax_times.append(ax_time)
        ax_mems.append(ax_mem)

    # Synchronise y-axis limits so both columns share the same scale
    t_ylims = [ax.get_ylim() for ax in ax_times]
    shared_t = (min(y[0] for y in t_ylims), max(y[1] for y in t_ylims))
    for ax in ax_times:
        ax.set_ylim(*shared_t)

    m_ylims = [ax.get_ylim() for ax in ax_mems]
    shared_m = (min(y[0] for y in m_ylims), max(y[1] for y in m_ylims))
    for ax in ax_mems:
        ax.set_ylim(*shared_m)

    out_c = os.path.join(IMAGES_DIR, "speedup_vaultx_vs_madmax.svg")
    fig_c.savefig(out_c, bbox_inches="tight")
    out_c_png = os.path.join(IMAGES_DIR, "speedup_vaultx_vs_madmax.png")
    fig_c.savefig(out_c_png, dpi=300, bbox_inches="tight")
    out_c_paper = os.path.join(PAPER_IMAGES_DIR, "speedup_vaultx_vs_madmax.png")
    fig_c.savefig(out_c_paper, dpi=300, bbox_inches="tight")
    plt.close(fig_c)
    print(f"  saved {out_c}")
    print(f"  saved {out_c_png}")
    print(f"  saved {out_c_paper}")


if __name__ == "__main__":
    print("Generating VaultX vs Madmax time-vs-threads plots …")
    main()
    print("Done.")
