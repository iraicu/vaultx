#!/usr/bin/env python3
"""
Avg per-lookup search latency: VaultX vs. ChiaPoS (K=32, EpycBox), across
four drive types (NVMe, SSD, HDD, Ceph-mounted HDD). For Section IV-G4
(Search/Lookup Latency vs. ChiaPoS).

Source data is /usr/bin/time wall-clock elapsed for `-S 1000` (1000 random
lookups) runs, traced from:
  newexperiments/epycbox/vaultxsearchresult_{nvme,ssd-raid0,data-l,ceph}.txt
  newexperiments/epycbox/search_chiapos_{nvme,ssd-raid0,data-l,ceph}.txt
(data-l is the HDD mount used throughout the Section IV-F search experiments;
ssd-raid0/sfatunmbi/ceph are SSD/NVMe/Ceph respectively -- same drive mapping
as search_n_files.py, varying_t_and_r.py, search_keep_open.py.)

Elapsed times were for 1000 lookups; here they are divided by 1000 and
converted to milliseconds per lookup (numerically the same digits, since
seconds-for-1000-lookups and ms-for-1-lookup are the same quantity).
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

# Elapsed wall-clock time for 1000 random K=32 lookups (seconds), EpycBox.
TOTAL_S_1000_LOOKUPS = {
    "NVME": {"ChiaPoS": 9.94,   "VaultX": 0.11},
    "SSD":  {"ChiaPoS": 13.46,  "VaultX": 0.35},
    "HDD":  {"ChiaPoS": 293.37, "VaultX": 6.60},
    "CEPH": {"ChiaPoS": 1195.38, "VaultX": 69.87},
}
DRIVES = ["NVME", "SSD", "HDD", "CEPH"]

# seconds-for-1000-lookups == ms-per-lookup (both /1000 and *1000 apply)
MS_PER_LOOKUP = {
    drive: {system: t for system, t in vals.items()}
    for drive, vals in TOTAL_S_1000_LOOKUPS.items()
}

COLORS = {"ChiaPoS": "#E6A817", "VaultX": "#2C7BB6"}


def main():
    x = np.arange(len(DRIVES))
    width = 0.38

    chia_vals   = [MS_PER_LOOKUP[d]["ChiaPoS"] for d in DRIVES]
    vaultx_vals = [MS_PER_LOOKUP[d]["VaultX"] for d in DRIVES]
    speedups    = [c / v for c, v in zip(chia_vals, vaultx_vals)]

    fig, ax = plt.subplots(figsize=(7, 5))

    chia_bars = ax.bar(x - width / 2, chia_vals, width, label="Chia (ChiaPoS)",
                        color=COLORS["ChiaPoS"], edgecolor="black", linewidth=0.6, zorder=3)
    vaultx_bars = ax.bar(x + width / 2, vaultx_vals, width, label="VaultX",
                          color=COLORS["VaultX"], edgecolor="black", linewidth=0.6, zorder=3)

    ax.set_yscale("log")
    ax.set_ylim(0.07, 2000)

    # Value label just above each bar, colored to match (chia gold, vaultx blue).
    for xi, v in zip(x - width / 2, chia_vals):
        ax.text(xi, v * 1.08, f"{v:.2f}ms", ha="center", va="bottom",
                 fontsize=9, color=COLORS["ChiaPoS"])
    for xi, v in zip(x + width / 2, vaultx_vals):
        ax.text(xi, v * 1.3, f"{v:.2f}ms", ha="center", va="bottom",
                 fontsize=9, color=COLORS["VaultX"])

    # Speedup label: bold white, inside the chia bar, near its bottom.
    label_y = ax.get_ylim()[0] * 2.1  # comfortably above axis floor, still
                                       # far below the shortest chia bar
    for xi, s in zip(x - width / 2, speedups):
        ax.text(xi, label_y, f"{s:.0f}×", ha="center", va="bottom",
                 fontsize=13, fontweight="bold", color="white", zorder=4)

    ax.set_xticks(x)
    ax.set_xticklabels(DRIVES, fontsize=11)
    ax.set_xlabel("Storage type", fontsize=11)
    ax.set_ylabel("Avg time per lookup (ms, log scale)", fontsize=11)
    ax.grid(axis="y", which="major", linestyle="--", alpha=0.4, zorder=0)
    ax.legend(fontsize=10, loc="upper left")

    fig.tight_layout()
    out     = os.path.join(IMAGES_DIR, "search_vs_chia.svg")
    out_png = os.path.join(IMAGES_DIR, "search_vs_chia.png")
    out_pp  = os.path.join(PAPER_IMAGES_DIR, "search_vs_chia.png")
    fig.savefig(out,     bbox_inches="tight")
    fig.savefig(out_png, dpi=300, bbox_inches="tight")
    fig.savefig(out_pp,  dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")
    print(f"  saved {out_png}")
    print(f"  saved {out_pp}")


if __name__ == "__main__":
    print("Generating search_vs_chia (avg per-lookup, ms) …")
    main()
    print("Done.")
