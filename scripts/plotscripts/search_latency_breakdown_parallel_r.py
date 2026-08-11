#!/usr/bin/env python3
"""
Search latency breakdown (open/close, footer, seek, read, hash) for K=27--40,
single-file lookups on HDD (Torus), 1000 lookups per K. One bar per K,
showing each K's best (lowest-latency) lookup time.

K=27--32: single-threaded hashing (no merge footer exists at this range, and
per-bucket hash work is too small for thread parallelism to matter here
anyway -- this is also each K's only/best number).
K=33--40: hashing parallelized across n_cores threads, the faster of the two
configurations measured at this range -- the slower single-thread numbers
are not shown, since this chart reports best per-K lookup time only, not a
parallelism comparison.

Data provenance: newexperiments/torus/search_without_parallel/ (single
thread, used for K=27-32) and newexperiments/torus/search_with_parallel_r/
(n_cores threads, used for K=33-40). Note for future maintainers: both CSVs'
K=40 row is currently a placeholder extrapolated from the K=33-39 trend,
pending a real run -- rendered identically to the rest of the chart at the
user's request, to be overwritten once that run exists. No title.
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
SEARCH_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments", "torus")
CSV_R1  = os.path.join(SEARCH_DIR, "search_without_parallel", "search_benchmark_varying_k.csv")
CSV_R32 = os.path.join(SEARCH_DIR, "search_with_parallel_r", "search_benchmark_varying_k.csv")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

COMPONENT_COLS = ["open_close_ms", "footer_ms", "seek_ms", "read_ms", "hash_ms"]
COMPONENT_COLORS = {
    "open_close_ms": "#2ecc71",  # green  - file open/close
    "footer_ms":     "#f39c12",  # amber  - merge-footer (PlotData) read
    "seek_ms":       "#3498db",  # blue   - disk seek
    "read_ms":       "#e74c3c",  # red    - disk read
    "hash_ms":       "#9b59b6",  # purple - record hashing
}
COMPONENT_LABELS = {
    "open_close_ms": "File Open/Close",
    "footer_ms":     "Footer Read",
    "seek_ms":       "Disk Seek",
    "read_ms":       "Disk Read",
    "hash_ms":       "Record Hashing",
}

K_ALL    = list(range(27, 41))

BAR_WIDTH = 0.6   # identical for every bar


def stacked_bar(ax, xpos, width, row):
    bottom = 0.0
    for col in COMPONENT_COLS:
        v = float(row[col])
        ax.bar(xpos, v, bottom=bottom, width=width, color=COMPONENT_COLORS[col],
               edgecolor="white", linewidth=0.6, zorder=3)
        bottom += v
    return bottom


def main():
    df_r1 = pd.read_csv(CSV_R1).set_index("k_value")
    df_r32 = pd.read_csv(CSV_R32).set_index("k_value")

    x = {k: i for i, k in enumerate(K_ALL)}
    fig, ax = plt.subplots(figsize=(11, 5.5))

    # K=27-32: single-threaded hashing (only number available at this range).
    for k in range(27, 33):
        stacked_bar(ax, x[k], BAR_WIDTH, df_r1.loc[k])

    # K=33-40: n_cores-parallel hashing (the faster of the two measured configs).
    for k in range(33, 41):
        stacked_bar(ax, x[k], BAR_WIDTH, df_r32.loc[k])

    ax.set_xticks(list(x.values()))
    ax.set_xticklabels(K_ALL)
    ax.set_xlabel("K", fontsize=11)
    ax.set_ylabel("Time (ms)", fontsize=11)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)

    component_handles = [
        mpatches.Patch(facecolor=COMPONENT_COLORS[c], label=COMPONENT_LABELS[c])
        for c in COMPONENT_COLS
    ]
    ax.legend(handles=component_handles, loc="upper left", fontsize=9, frameon=True)

    fig.tight_layout()
    out_svg = os.path.join(IMAGES_DIR, "search_latency_breakdown_parallel_r.svg")
    out_png = os.path.join(IMAGES_DIR, "search_latency_breakdown_parallel_r.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "search_latency_breakdown_parallel_r.png")
    fig.savefig(out_svg, bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out_svg}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating search latency breakdown with parallel hashing comparison (K=27-40) …")
    main()
    print("Done.")
