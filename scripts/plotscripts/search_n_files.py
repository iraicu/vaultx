#!/usr/bin/env python3
"""
Avg per-lookup time: 450 separate K=32 files (-t 1) vs. one merged file of
450 K=32 sub-vaults (-r 1), on HDD (EpycBox). Just two data points plus the
speedup, so kept deliberately small -- no reason to occupy a full column's
height for two bars.

Source data: newexperiments/epycbox/search_t_sweep_20260701_052842.csv
(t=1, keep_open=false, target=/data-l/.../plots/, 450 separate files) and
newexperiments/epycbox/search_r_sweep_20260701_055110.csv
(r=1, target=merge_32_450.plot, one merged file of 450 K=32 sub-vaults).
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

LABELS = ["450 separate files\n(−t 1)", "1 merged file\n(−r 1)"]
VALUES = [3151.34, 65.8]  # ms per lookup
COLORS = ["#FF7F0E", "#1F77B4"]


def main():
    fig, ax = plt.subplots(figsize=(3.3, 3.1))
    x = np.arange(len(LABELS))
    ax.bar(x, VALUES, width=0.55, color=COLORS, zorder=3,
           edgecolor="white", linewidth=0.5)

    for xi, v in zip(x, VALUES):
        ax.text(xi, v + max(VALUES) * 0.02, f"{v:,.1f}ms",
                ha="center", va="bottom", fontsize=7.5)

    speedup = VALUES[0] / VALUES[1]
    ax.text(0.72, 0.92, f"{speedup:.0f}$\\times$", transform=ax.transAxes,
            ha="center", va="top", fontsize=10, fontweight="bold", color="#D62728")

    ax.set_xticks(x)
    ax.set_xticklabels(LABELS, fontsize=7.5)
    ax.set_ylabel("Avg time/lookup (ms)", fontsize=8.5)
    ax.set_ylim(0, max(VALUES) * 1.18)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.tick_params(axis="y", labelsize=7.5)
    ax.text(0.02, 0.97, "↓ lower is better", transform=ax.transAxes,
            fontsize=6.5, ha="left", va="top", color="gray", style="italic")

    fig.tight_layout()
    out     = os.path.join(IMAGES_DIR, "search_n_files.svg")
    out_png = os.path.join(IMAGES_DIR, "search_n_files.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "search_n_files.png")
    fig.savefig(out,     bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating search_n_files (compact) …")
    main()
    print("Done.")
