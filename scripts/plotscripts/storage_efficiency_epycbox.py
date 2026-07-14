#!/usr/bin/env python3
"""
Storage efficiency (SE) vs memory allocation for a K=32 vault on Epycbox
(corrections.md #15). First bar is full in-memory generation (48 GB, 100% SE
by construction); remaining bars are OOM runs at 26, 14, 8, 5, 3, 2.5, 2 GB,
sourced from newexperiments/epycbox/memvary/varying_memory_k32_CP128_IO1_data-l.csv
(storage_efficiency_pct column only -- other columns in that CSV are not used
here). SE declines as memory drops because more OOM rounds are needed, and
nonce pairs split across round boundaries are never matched. No title.
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

CSV_PATH = os.path.join(RESULTS_DIR, "epycbox", "memvary",
                        "varying_memory_k32_CP128_IO1_data-l.csv")

IN_MEMORY_LABEL = "48GB\n(In-Memory)"
IN_MEMORY_SE    = 100.0

BAR_COLOR = "#1F77B4"
IM_COLOR  = "#2CA02C"


def main():
    df = pd.read_csv(CSV_PATH)
    df["memory_gb"]             = pd.to_numeric(df["memory_gb"], errors="coerce")
    df["storage_efficiency_pct"] = pd.to_numeric(df["storage_efficiency_pct"], errors="coerce")
    df = df.dropna(subset=["memory_gb", "storage_efficiency_pct"])
    df = df.sort_values("memory_gb", ascending=False)

    labels = [IN_MEMORY_LABEL] + [f"{m:g}GB" for m in df["memory_gb"]]
    values = [IN_MEMORY_SE] + list(df["storage_efficiency_pct"])
    colors = [IM_COLOR] + [BAR_COLOR] * len(df)

    fig, ax = plt.subplots(figsize=(9, 5.5))
    x = np.arange(len(labels))
    ax.bar(x, values, color=colors, width=0.65, zorder=3,
           edgecolor="white", linewidth=0.5)

    for xi, v in zip(x, values):
        ax.text(xi, v + 1.5, f"{v:.1f}%", ha="center", va="bottom",
                fontsize=9, color="black", zorder=6)

    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=9)
    ax.set_xlabel("Memory allocated (-m)", fontsize=10)
    ax.set_ylabel("Storage efficiency (%)", fontsize=11)
    ax.set_ylim(0, 110)
    ax.set_yticks(np.arange(0, 101, 10))
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.text(0.01, 0.03, "↑ higher is better",
            transform=ax.transAxes, fontsize=8, ha="left", va="bottom",
            color="gray", style="italic")

    fig.tight_layout()
    out     = os.path.join(IMAGES_DIR, "storage_efficiency_epycbox.svg")
    out_png = os.path.join(IMAGES_DIR, "storage_efficiency_epycbox.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "storage_efficiency_epycbox.png")
    fig.savefig(out,     bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating storage efficiency vs memory (epycbox) …")
    main()
    print("Done.")
