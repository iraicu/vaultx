#!/usr/bin/env python3
"""
Search latency breakdown (open/close, seek, read, hash) for K=27--40.
Single-file lookups on HDD (Torus), -t 1 -r 1, 1000 lookups per K. No title.
"""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import pandas as pd

SCRIPT_DIR       = os.path.dirname(os.path.abspath(__file__))
IMAGES_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "images")
PAPER_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
CSV_PATH         = os.path.join(
    SCRIPT_DIR, "..", "..", "newexperiments", "torus", "search",
    "search_benchmark_varying_k.csv",
)
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

COMPONENT_COLS = ["open_close_ms", "seek_ms", "read_ms", "hash_ms"]
COMPONENT_COLORS = {
    "open_close_ms": "#2ecc71",  # green - file open/close
    "seek_ms":       "#3498db",  # blue  - disk seek
    "read_ms":       "#e74c3c",  # red   - disk read
    "hash_ms":       "#9b59b6",  # purple- record hashing
}
COMPONENT_LABELS = {
    "open_close_ms": "File Open/Close",
    "seek_ms":       "Disk Seek",
    "read_ms":       "Disk Read",
    "hash_ms":       "Record Hashing",
}


def main():
    df = pd.read_csv(CSV_PATH).sort_values("k_value")

    ks = df["k_value"].astype(int).tolist()
    x = range(len(ks))

    fig, ax = plt.subplots(figsize=(11, 5.5))

    bottoms = [0.0] * len(ks)
    for col in COMPONENT_COLS:
        values = df[col].tolist()
        ax.bar(x, values, bottom=bottoms, width=0.65,
               color=COMPONENT_COLORS[col], label=COMPONENT_LABELS[col],
               edgecolor="white", linewidth=0.6, zorder=3)
        bottoms = [b + v for b, v in zip(bottoms, values)]

    ax.set_xticks(list(x))
    ax.set_xticklabels(ks)
    ax.set_xlabel("K", fontsize=11)
    ax.set_ylabel("Time (ms)", fontsize=11)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.legend(loc="upper left", fontsize=9, frameon=True)

    fig.tight_layout()
    out_svg = os.path.join(IMAGES_DIR, "search_latency_breakdown_varying_k.svg")
    out_png = os.path.join(IMAGES_DIR, "search_latency_breakdown_varying_k.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "search_latency_breakdown_varying_k.png")
    fig.savefig(out_svg, bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out_svg}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating search latency breakdown (varying K) …")
    main()
    print("Done.")
