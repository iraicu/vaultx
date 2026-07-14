#!/usr/bin/env python3
"""
Thread scaling for HDD search: -t across 450 separate K=32 files vs. -r
within one merged file of 450 K=32 sub-vaults (EpycBox).

Uses a broken y-axis instead of log scale: the -r cluster (~40-70ms) and
the -t cluster (~1.7k-3.2k ms) each get their own linear range, so the
shape within each cluster (decline then uptick) is visible directly,
rather than being compressed by a shared log scale.

Source:
- -t line: newexperiments/epycbox/search_t_sweep_20260701_052842.csv
  (keep_open=false, 450 separate files, target=/data-l/.../plots/)
- -r line: mean across the three complete r-sweeps on the merged file
  (target=merge_32_450.plot): search_r_sweep_20260701_055026.csv,
  _055110.csv, _055127.csv (the fourth sweep, _053532.csv, is missing
  r=32/128 and is excluded from the average).
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments", "epycbox")
IMAGES_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "images")
PAPER_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

T_SWEEP_CSV = os.path.join(RESULTS_DIR, "search_t_sweep_20260701_052842.csv")
R_SWEEP_CSVS = [
    os.path.join(RESULTS_DIR, "search_r_sweep_20260701_055026.csv"),
    os.path.join(RESULTS_DIR, "search_r_sweep_20260701_055110.csv"),
    os.path.join(RESULTS_DIR, "search_r_sweep_20260701_055127.csv"),
]

T_COLOR = "#FFB000"
R_COLOR = "#1F77B4"

TOP_YLIM    = (1600, 3300)
BOTTOM_YLIM = (0, 100)


def load_t_line():
    df = pd.read_csv(T_SWEEP_CSV)
    df = df[df["keep_open"] == False].sort_values("t")  # noqa: E712
    return df["t"].tolist(), df["avg_ms_per_lookup"].tolist()


def load_r_line():
    dfs = [pd.read_csv(p) for p in R_SWEEP_CSVS]
    combined = pd.concat(dfs, ignore_index=True)
    grouped = combined.groupby("r")["avg_ms_per_lookup"].mean().sort_index()
    return grouped.index.tolist(), grouped.values.tolist()


def main():
    t_x, t_y = load_t_line()
    r_x, r_y = load_r_line()

    fig, (ax_top, ax_bot) = plt.subplots(
        2, 1, sharex=True, figsize=(3.6, 4.0),
        gridspec_kw={"height_ratios": [2, 1], "hspace": 0.08},
    )

    for ax in (ax_top, ax_bot):
        ax.plot(t_x, t_y, "-o", color=T_COLOR, label="−t (450 files)",
                markersize=4, linewidth=1.4, zorder=3)
        ax.plot(r_x, r_y, "-s", color=R_COLOR, label="−r (1 merged file)",
                markersize=4, linewidth=1.4, zorder=3)
        ax.set_xscale("log", base=2)
        ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)

    ax_top.set_ylim(*TOP_YLIM)
    ax_bot.set_ylim(*BOTTOM_YLIM)

    # Hide the spines between the two axes and add break marks
    ax_top.spines.bottom.set_visible(False)
    ax_bot.spines.top.set_visible(False)
    ax_top.tick_params(labeltop=False, bottom=False)
    ax_bot.xaxis.tick_bottom()

    # Show plain thread-count numbers (1,2,4,...) instead of log-scale 2^n ticks
    tick_vals = sorted(set(t_x) | set(r_x))
    ax_bot.set_xticks(tick_vals)
    ax_bot.set_xticklabels([str(v) for v in tick_vals])
    ax_bot.minorticks_off()

    d = 0.5
    kwargs = dict(marker=[(-1, -d), (1, d)], markersize=8,
                  linestyle="none", color="k", mec="k", mew=1, clip_on=False)
    ax_top.plot([0, 1], [0, 0], transform=ax_top.transAxes, **kwargs)
    ax_bot.plot([0, 1], [1, 1], transform=ax_bot.transAxes, **kwargs)

    ax_bot.set_xlabel("Thread count (−t or −r)", fontsize=8.5)
    ax_top.tick_params(axis="y", labelsize=7.5)
    ax_bot.tick_params(axis="y", labelsize=7.5)
    ax_bot.tick_params(axis="x", labelsize=7.5)
    ax_top.legend(fontsize=7, loc="upper right")
    ax_top.text(0.02, 0.06, "↓ lower is better", transform=ax_top.transAxes,
                fontsize=6.5, ha="left", va="bottom", color="gray", style="italic")

    fig.subplots_adjust(left=0.24)
    fig.text(0.04, 0.55, "Avg time/lookup (ms)", fontsize=8.5,
              ha="center", va="center", rotation="vertical")

    out     = os.path.join(IMAGES_DIR, "varying_t_and_r.svg")
    out_png = os.path.join(IMAGES_DIR, "varying_t_and_r.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "varying_t_and_r.png")
    fig.savefig(out,     bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating varying_t_and_r (broken y-axis) …")
    main()
    print("Done.")
