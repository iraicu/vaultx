#!/usr/bin/env python3
"""
K=33-K40 large-vault generation time: direct out-of-memory (OOM) vs
plot+merge (N K=32 sub-vaults merged), Epycbox, HDD final drive.

Data provenance (see newexperiments/epycbox/):
- OOM bars K=33-K39: measured directly (k33-k40gen_data-l-data-m.csv,
  total_time_s column).
- OOM bar K=40: no measurement exists at all -- a native K=40 OOM run needs
  15TB of temp space (final file is 10TB) against this machine's 14TB drive
  limit, so the run cannot complete. Shown as a hatched "estimated" bar,
  extrapolated via log-linear regression over the seven real OOM points.
- Plot+merge bars K=33-K36 (N=2,4,8,16): fully measured -- summing the
  actual per-subplot generation times from gen_N16_K32_log.txt (not an
  average) plus the measured merge time for that N
  (merge_benchmark/merge_benchmark_B1024_varyN_K32.csv, total_merge_time_s).
- Plot+merge bars K=37-K40 (N=32,64,128,256): merge time is measured; the
  subplot-generation time is extrapolated as N times the mean single-subplot
  time from the 16 measured runs (no visual distinction from the measured
  bars -- 16 additional real generations would look the same, per design).
No title.
"""

import os
import re

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.patheffects as patheffects
import numpy as np
import pandas as pd

SCRIPT_DIR       = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR      = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments")
IMAGES_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "images")
PAPER_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

OOM_CSV   = os.path.join(RESULTS_DIR, "epycbox", "k33-k40gen_data-l-data-m.csv")
MERGE_CSV = os.path.join(RESULTS_DIR, "epycbox", "merge_benchmark",
                         "merge_benchmark_B1024_varyN_K32.csv")
GEN_LOG   = os.path.join(RESULTS_DIR, "epycbox", "gen_N16_K32_log.txt")

OOM_COLOR   = "#1F77B4"
MERGE_COLOR = "#2CA02C"

K_VALUES = list(range(33, 41))
N_FOR_K  = {k: 2 ** (k - 32) for k in K_VALUES}


def load_oom_times_s() -> dict[int, float]:
    df = pd.read_csv(OOM_CSV)
    df["K"] = pd.to_numeric(df["K"], errors="coerce")
    df["total_time_s"] = pd.to_numeric(df["total_time_s"], errors="coerce")
    df = df.dropna(subset=["K", "total_time_s"])
    return dict(zip(df["K"].astype(int), df["total_time_s"]))


def load_merge_times_s() -> dict[int, float]:
    df = pd.read_csv(MERGE_CSV)
    df["N"] = pd.to_numeric(df["N"], errors="coerce")
    df["total_merge_time_s"] = pd.to_numeric(df["total_merge_time_s"], errors="coerce")
    df = df.dropna(subset=["N", "total_merge_time_s"])
    return dict(zip(df["N"].astype(int), df["total_merge_time_s"]))


def load_subplot_times_s() -> list[float]:
    with open(GEN_LOG) as f:
        text = f.read()
    return [float(t) for t in re.findall(r"Total Time:\s*([\d.]+)\s*seconds", text)]


def extrapolate_k40_oom(oom_times_s: dict[int, float]) -> float:
    """Log-linear regression over the real K=33-K39 OOM points, extrapolated to K=40."""
    ks = np.array(sorted(oom_times_s))
    ys = np.log(np.array([oom_times_s[k] for k in ks]))
    slope, intercept = np.polyfit(ks, ys, 1)
    return float(np.exp(slope * 40 + intercept))


def plot_component_s(n: int, subplot_times_s: list[float]) -> float:
    if n <= len(subplot_times_s):
        return sum(subplot_times_s[:n])
    return n * (sum(subplot_times_s) / len(subplot_times_s))


def fmt_min(v: float) -> str:
    return f"{v:.1f}m" if v < 100 else f"{v:.0f}m"


def main():
    oom_times_s = load_oom_times_s()
    merge_times_s = load_merge_times_s()
    subplot_times_s = load_subplot_times_s()

    oom_times_s = dict(oom_times_s)
    oom_times_s[40] = extrapolate_k40_oom(oom_times_s)

    oom_min, pm_min = [], []
    for k in K_VALUES:
        n = N_FOR_K[k]
        oom_min.append(oom_times_s[k] / 60.0)
        pm_s = plot_component_s(n, subplot_times_s) + merge_times_s[n]
        pm_min.append(pm_s / 60.0)

    pct_saved = [(o - p) / o * 100.0 for o, p in zip(oom_min, pm_min)]

    x = np.arange(len(K_VALUES))
    bw = 0.35

    fig, ax = plt.subplots(figsize=(10, 6))

    # OOM bars: K=33-39 solid/real, K=40 hatched/extrapolated.
    ax.bar(x[:-1] - bw / 2, oom_min[:-1], width=bw, color=OOM_COLOR, zorder=3,
           edgecolor="white", linewidth=0.5, label="Direct OOM")
    ax.bar(x[-1] - bw / 2, oom_min[-1], width=bw, color=OOM_COLOR, alpha=0.75,
           hatch="xx", zorder=3, edgecolor=OOM_COLOR, linewidth=0.5)

    # Plot+merge bars: uniform styling across all K (simulated points are
    # not visually distinguished -- see module docstring).
    ax.bar(x + bw / 2, pm_min, width=bw, color=MERGE_COLOR, alpha=0.65,
           hatch="//", zorder=3, edgecolor=MERGE_COLOR, linewidth=0.5,
           label="Plot+Merge")

    for xi, v in zip(x, oom_min):
        ax.text(xi - bw / 2, v * 1.12, fmt_min(v), ha="center", va="bottom",
                 fontsize=7.5, color=OOM_COLOR, fontweight="bold", zorder=5)
    for xi, v in zip(x, pm_min):
        ax.text(xi + bw / 2, v * 1.12, fmt_min(v), ha="center", va="bottom",
                 fontsize=7.5, color=MERGE_COLOR, fontweight="bold", zorder=5)

    # "-XX%" label inside each merge bar, near its base. Placed at a fixed
    # fraction of the bar's own height in log-space (rather than a fixed
    # data value) so it sits just above the bar's bottom edge consistently
    # whether the bar spans one log decade (K=33) or nearly three (K=40).
    y_floor = 10.0
    for xi, v, pct in zip(x, pm_min, pct_saved):
        y_label = y_floor * (v / y_floor) ** 0.35
        txt = ax.text(xi + bw / 2, y_label, f"-{pct:.0f}%", ha="center", va="bottom",
                      fontsize=8, color="white", fontweight="bold", zorder=6)
        txt.set_path_effects([patheffects.withStroke(linewidth=2.0, foreground=MERGE_COLOR)])

    ax.annotate("infeasible on 14TB HDD\n(15TB temp needed) --\nextrapolated",
                xy=(x[-1] - bw / 2, oom_min[-1]), xytext=(x[-1] - 1.65, oom_min[-1] * 1.35),
                fontsize=7, color=OOM_COLOR, ha="left", va="center",
                arrowprops=dict(arrowstyle="->", color=OOM_COLOR, lw=0.8))

    ax.set_yscale("log")
    ax.set_ylim(10, 10000)
    ax.set_yticks([10, 100, 1000, 10000])
    ax.yaxis.set_major_formatter(matplotlib.ticker.ScalarFormatter())
    ax.yaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
    ax.set_ylabel("Time (minutes, log scale)", fontsize=10)

    ax.set_xticks(x)
    ax.set_xticklabels([f"K={k}" for k in K_VALUES], fontsize=9)
    ax.set_xlabel("Target vault size", fontsize=10)

    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.text(0.02, 0.78, "↓ lower is better",
            transform=ax.transAxes, fontsize=8, ha="left", va="top",
            color="gray", style="italic")

    legend_items = [
        mpatches.Patch(facecolor=OOM_COLOR, edgecolor="white", label="Direct OOM"),
        mpatches.Patch(facecolor=MERGE_COLOR, alpha=0.65, hatch="//",
                        edgecolor=MERGE_COLOR, label="Plot+Merge (N×K=32)"),
    ]
    ax.legend(handles=legend_items, fontsize=9, loc="upper left")

    fig.tight_layout()
    out     = os.path.join(IMAGES_DIR, "large_merge_vs_oom_epycbox.svg")
    out_png = os.path.join(IMAGES_DIR, "large_merge_vs_oom_epycbox.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "large_merge_vs_oom_epycbox.png")
    fig.savefig(out,     bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")

    print("\n  K   N    OOM(min)   Plot+Merge(min)   P+M faster by")
    for k, o, p, pct in zip(K_VALUES, oom_min, pm_min, pct_saved):
        print(f"  {k}  {N_FOR_K[k]:>4}  {o:9.1f}   {p:14.1f}      {pct:5.1f}%")


if __name__ == "__main__":
    print("Generating large-vault (K=33-K40) OOM vs plot+merge comparison (epycbox) …")
    main()
    print("Done.")
