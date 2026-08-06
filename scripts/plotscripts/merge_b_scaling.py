#!/usr/bin/env python3
"""
Effect of the merge batch size -B on total merge time and average throughput,
merging 64 K=32 sub-vaults to a spinning HDD on EpycBox.

Source data: newexperiments/epycbox/merge_benchmark/
merge_benchmark_varyB_N64_K32_data-m.csv -- eleven runs sweeping -B from 32MB
to 32GB with N=64, K=32, t=1, R=1024, target /data-m (HDD). Columns used:
B_mb, total_merge_time_s, total_data_gb, expected_peak_ram_mb.

Time and throughput are derived from total_merge_time_s rather than read from
the CSV's total_merge_time_min / avg_throughput_mbs columns: those two columns
are wrong for the B=64 and B=32 rows (off by ~2x and ~4x). The derivation
reproduces the other nine rows exactly, and total_merge_time_s is itself
consistent with read+write+compute on every row.

Note the sibling file merge_benchmark_varyB_varyN_K32.csv in the same directory
is a different experiment (single N=450 run) and is not the source for this
figure.

The figure's point is the knee at -B = 256MB. From 256MB up the curve is flat
-- 154-163 min at 215-235 MB/s across a 128x span of batch sizes -- so 256MB
buys the whole plateau at 512MB of peak RAM instead of 64GB. Below the knee the
batches stop amortising the HDD's seek cost and merge time jumps by ~1.6x to
~4.3 h at ~136 MB/s, with no further memory saving worth having. -B is plotted
categorically -- the sweep doubles each step, and equal spacing keeps both the
flat plateau and the sharpness of the drop readable.
No title.
"""

import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

SCRIPT_DIR       = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR      = os.path.join(SCRIPT_DIR, "..", "..", "newexperiments")
IMAGES_DIR       = os.path.join(SCRIPT_DIR, "..", "..", "images")
PAPER_IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)
os.makedirs(PAPER_IMAGES_DIR, exist_ok=True)

MERGE_CSV = os.path.join(RESULTS_DIR, "epycbox", "merge_benchmark",
                         "merge_benchmark_varyB_N64_K32_data-m.csv")

TIME_COLOR = "#1F77B4"
TPUT_COLOR = "#2CA02C"

KNEE_B_MB = 256          # smallest -B still on the flat plateau


def load() -> pd.DataFrame:
    cols = ["B_mb", "total_merge_time_s", "total_data_gb",
            "expected_peak_ram_mb"]
    df = pd.read_csv(MERGE_CSV)
    for c in cols:
        df[c] = pd.to_numeric(df[c], errors="coerce")
    df = df.dropna(subset=cols).sort_values("B_mb").reset_index(drop=True)
    df["merge_time_min"] = df["total_merge_time_s"] / 60.0
    df["throughput_mbs"] = df["total_data_gb"] * 1024.0 / df["total_merge_time_s"]
    return df


def fmt_ram(mb: float) -> str:
    return f"{mb / 1024:.0f} GB" if mb >= 1024 else f"{mb:.0f} MB"


def main():
    df = load()
    x = np.arange(len(df))
    knee = int(df.index[df["B_mb"] == KNEE_B_MB][0])

    fig, ax = plt.subplots(figsize=(6.4, 3.7))
    ax2 = ax.twinx()

    # Everything left of the knee is the small-batch regime.
    ax.axvspan(-0.4, knee - 0.5, color="#B0B0B0", alpha=0.16, linewidth=0,
               zorder=0)
    ax.text((-0.4 + knee - 0.5) / 2, 197, "batches too small\nto amortise seeks",
            fontsize=8, ha="center", va="center", color="#555555")

    l1, = ax.plot(x, df["merge_time_min"], marker="o", markersize=5,
                  color=TIME_COLOR, linewidth=1.8, label="Merge time (min)",
                  zorder=3)
    l2, = ax2.plot(x, df["throughput_mbs"], marker="s", markersize=5,
                   color=TPUT_COLOR, linewidth=1.8, linestyle="--",
                   label="Avg throughput (MB/s)", zorder=3)

    ax.set_ylim(130, 270)
    ax.set_yticks([140, 160, 180, 200, 220, 240, 260])
    ax.set_ylabel("Total merge time (min)", fontsize=10, color=TIME_COLOR)
    ax.tick_params(axis="y", labelcolor=TIME_COLOR)

    ax2.set_ylim(110, 250)
    ax2.set_yticks([120, 150, 180, 210, 240])
    ax2.set_ylabel("Avg throughput (MB/s)", fontsize=10, color=TPUT_COLOR)
    ax2.tick_params(axis="y", labelcolor=TPUT_COLOR)

    ax.set_xticks(x)
    ax.set_xticklabels([f"{int(b)}" for b in df["B_mb"]], rotation=30, ha="right")
    ax.set_xlabel("Batch size −B (MB), with peak RAM = 2×B", fontsize=10)
    ax.set_xlim(-0.4, len(df) - 0.6)

    # The knee and the two endpoints: 1024x of memory spread, one flat plateau,
    # and nothing gained by dropping below the knee.
    for i, (dx, dy) in ((0, (0.35, -13)),
                        (knee, (0.3, -12)),
                        (len(df) - 1, (-2.1, -10))):
        ax.annotate(f"Peak RAM: {fmt_ram(df['expected_peak_ram_mb'][i])}",
                    xy=(x[i], df["merge_time_min"][i]),
                    xytext=(x[i] + dx, df["merge_time_min"][i] + dy),
                    fontsize=8, ha="left", va="center", zorder=4,
                    arrowprops=dict(arrowstyle="->", color="#333333", lw=0.8))

    ax.legend(handles=[l1, l2], fontsize=9, loc="center right")

    fig.tight_layout()
    out     = os.path.join(IMAGES_DIR, "merge_b_scaling.svg")
    out_png = os.path.join(IMAGES_DIR, "merge_b_scaling.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "merge_b_scaling.png")
    fig.savefig(out,     bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")

    flat  = df[df["B_mb"] >= KNEE_B_MB]
    small = df[df["B_mb"] <  KNEE_B_MB]
    print("\n   -B(MB)   time(min)   tput(MB/s)   peak RAM")
    for _, r in df.iterrows():
        print(f"  {int(r['B_mb']):>7}   {r['merge_time_min']:9.2f}   "
              f"{r['throughput_mbs']:10.2f}   {fmt_ram(r['expected_peak_ram_mb']):>8}"
              f"{'' if r['B_mb'] >= KNEE_B_MB else '   (below knee)'}")
    t, p = flat["merge_time_min"], flat["throughput_mbs"]
    print(f"\n  plateau (-B >= {KNEE_B_MB}MB, {len(flat)} runs)")
    print(f"    time  {t.min():.2f}-{t.max():.2f} min, spread {(t.max() - t.min()) / t.max() * 100:.1f}% of max")
    print(f"    tput  {p.min():.2f}-{p.max():.2f} MB/s")
    print(f"    RAM   {fmt_ram(flat['expected_peak_ram_mb'].min())} .. {fmt_ram(flat['expected_peak_ram_mb'].max())} "
          f"({flat['expected_peak_ram_mb'].max() / flat['expected_peak_ram_mb'].min():.0f}x)")
    t, p = small["merge_time_min"], small["throughput_mbs"]
    print(f"  below knee (-B < {KNEE_B_MB}MB, {len(small)} runs)")
    print(f"    time  {t.min():.2f}-{t.max():.2f} min")
    print(f"    tput  {p.min():.2f}-{p.max():.2f} MB/s")
    print(f"  knee cost  {small['merge_time_min'].mean() / flat['merge_time_min'].mean():.2f}x slower on average")


if __name__ == "__main__":
    print("Generating merge -B batch-size scaling (epycbox, HDD, N=64 K=32) …")
    main()
    print("Done.")
