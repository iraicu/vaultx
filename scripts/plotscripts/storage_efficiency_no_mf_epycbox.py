#!/usr/bin/env python3
"""
Storage efficiency (SE) vs K for fully in-memory vault generation on EpycBox
with the matching factor disabled (f_m = 1, i.e. the raw expected-distance
formula with no per-K tuning), sourced from
newexperiments/epycbox/k27-k32_hdd_for_SE.csv (storage_efficiency_pct column
only -- other columns in that CSV are not used here). All runs are full
in-memory (no OOM rounds), so any SE loss here is purely a function of
per-bucket record density, not memory-limited round splitting: smaller K
means smaller Table~1 buckets, fewer candidate pairs within the untuned
distance window, and lower SE. This motivates why VaultX empirically tunes
a matching factor per K instead of using f_m = 1.
"""

import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments")
IMAGES_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "images")
PAPER_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

CSV_PATH = os.path.join(RESULTS_DIR, "epycbox", "k27-k32_hdd_for_SE.csv")

BAR_COLOR = "#1F77B4"


def main():
    df = pd.read_csv(CSV_PATH)
    df["k"] = pd.to_numeric(df["k"], errors="coerce")
    df["storage_efficiency_pct"] = pd.to_numeric(df["storage_efficiency_pct"], errors="coerce")
    df = df.dropna(subset=["k", "storage_efficiency_pct"]).sort_values("k")

    labels = [f"K={int(k)}" for k in df["k"]]
    values = list(df["storage_efficiency_pct"])

    fig, ax = plt.subplots(figsize=(9, 5.5))
    x = np.arange(len(labels))
    ax.bar(x, values, color=BAR_COLOR, width=0.6, zorder=3,
           edgecolor="white", linewidth=0.5)

    ax.axhline(100, color="#D62728", linestyle="--", linewidth=1.2, zorder=4)
    ax.text(len(labels) - 0.4, 100.8, "100% (target)", fontsize=8,
            color="#D62728", ha="right", va="bottom", style="italic")

    for xi, v in zip(x, values):
        ax.text(xi, v + 1.5, f"{v:.1f}%", ha="center", va="bottom",
                fontsize=9, color="black", zorder=6)

    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=9)
    ax.set_xlabel("K-value (full in-memory, no matching factor)", fontsize=10)
    ax.set_ylabel("Storage efficiency (%)", fontsize=11)
    ax.set_ylim(0, 112)
    ax.set_yticks(np.arange(0, 101, 10))
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.text(0.01, 0.03, "↑ higher is better",
            transform=ax.transAxes, fontsize=8, ha="left", va="bottom",
            color="gray", style="italic")

    fig.tight_layout()
    out     = os.path.join(IMAGES_DIR, "storage_efficiency_no_mf_epycbox.svg")
    out_png = os.path.join(IMAGES_DIR, "storage_efficiency_no_mf_epycbox.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "storage_efficiency_no_mf_epycbox.png")
    fig.savefig(out,     bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating storage efficiency vs K, no matching factor (epycbox) …")
    main()
    print("Done.")
