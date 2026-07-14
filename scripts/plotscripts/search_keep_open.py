#!/usr/bin/env python3
"""
Per-lookup time breakdown for 450 K=32 files on HDD (-t 1): -O false vs.
-O true (EpycBox). Kept small -- two stacked bars, no reason to occupy a
full column's height.

Source: newexperiments/epycbox/search_t_sweep_20260701_051309.csv
(target=/data-l/.../plots/, t=1, r=1, keep_open in {false, true}).
"""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

SCRIPT_DIR       = os.path.dirname(os.path.abspath(__file__))
IMAGES_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "images")
PAPER_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

LABELS = ["−O false", "−O true"]
# (open_close, seek, read, hash) ms, from the sweep CSV
COMPONENTS = {
    "open/close": [30.04, 2.82],
    "seek":       [2428.78, 1462.73],
    "read":       [685.52, 1484.68],
    "hash":       [45.18, 44.19],
}
COLORS = {
    "open/close": "#D62728",
    "seek":       "#FF7F0E",
    "read":       "#2CA02C",
    "hash":       "#1F77B4",
}
TOTALS = [3189.53, 2994.42]


def main():
    fig, ax = plt.subplots(figsize=(3.3, 3.1))
    x = np.arange(len(LABELS))
    bottom = np.zeros(len(LABELS))
    for name, vals in COMPONENTS.items():
        ax.bar(x, vals, bottom=bottom, width=0.5, color=COLORS[name],
               label=name, zorder=3, edgecolor="white", linewidth=0.4)
        bottom += np.array(vals)

    for xi, total in zip(x, TOTALS):
        ax.text(xi, total + 60, f"{total:,.0f}ms", ha="center", va="bottom",
                fontsize=7.5)

    ax.set_xticks(x)
    ax.set_xticklabels(LABELS, fontsize=8)
    ax.set_ylabel("Avg time/lookup (ms)", fontsize=8.5)
    ax.set_ylim(0, max(TOTALS) * 1.18)
    ax.tick_params(axis="y", labelsize=7.5)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.legend(fontsize=6.5, loc="upper center", bbox_to_anchor=(0.5, -0.16),
              ncol=2, framealpha=0.9, columnspacing=1.0, handletextpad=0.5)
    ax.text(0.02, 0.97, "↓ lower is better", transform=ax.transAxes,
            fontsize=6.5, ha="left", va="top", color="gray", style="italic")

    fig.tight_layout()
    out     = os.path.join(IMAGES_DIR, "search_keep_open.svg")
    out_png = os.path.join(IMAGES_DIR, "search_keep_open.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "search_keep_open.png")
    fig.savefig(out,     bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating search_keep_open (compact) …")
    main()
    print("Done.")
