#!/usr/bin/env python3
"""
Best k32 plotting time on NVME (or fastest local drive) for each machine.
Wide bar chart: Chia plotters (red, left) | VaultX x86 (blue) | VaultX ARM (steel-blue, right).

Bar annotations show: machine name, peak memory used for that plot, threads used.
Bars exceeding Y_CAP are capped, hatched, and the real time is written vertically.
"""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)

# ── Data ─────────────────────────────────────────────────────────────────────
# Chia plotters – best k32 time on NVME / fastest drive
# Format: (label, time_min, machine_display, threads_used, peak_mem_mb)
CHIA_DATA = [
    # ChiaPOS ──────────────────────────────────────────────────────────────
    ("ChiaPOS\n(s8)",       379.57, "s8",      192,   4288.0),  # nvme-raid0, -r 192
    ("ChiaPOS\n(opi5)",     630.00, "opi5",      8,   3500.0),  # data-fast,  -r 8
    # Madmax ───────────────────────────────────────────────────────────────
    ("Madmax\n(s8)",         27.85, "s8",       64,  42329.0),  # nvme-raid0, 64T
    ("Madmax\n(torus)",      97.33, "torus",    32,  25231.0),  # ssd-raid0,  32T
    ("Madmax\n(epycbox)",    54.45, "epycbox",  64,  42329.0),  # nfs_nvme,   64T
    # Bladebit ─────────────────────────────────────────────────────────────
    ("Bladebit\n(s8)",        9.42, "s8",      128, 426031.0),  # nvme-raid0 ramplot, 128T
]

# VaultX – best k32 time on NVME / fastest drive
# Format: (machine_display, time_min, threads_used, peak_mem_mb, is_arm)
VAULTX_DATA = [
    ("s8",        1.77,  384, 50445.2, False),   # nvme-raid0
    ("thunderx2", 5.12,  224, 50240.8, True),    # nfs_nvme
    ("epycbox",   3.54,  128, 50438.1, False),   # nfs_nvme
    ("gpubox",    2.88,   96, 50404.5, False),   # nfs_nvme
    ("thunderx1", 6.35,   96, 50424.0, True),    # nfs_nvme
    ("nvmebox",   2.86,   64, 50436.4, False),   # data-fast
    ("athena",    5.76,   48, 50427.8, False),   # nvme-raid0
    ("torus",     6.36,   32, 50435.3, False),   # nfs_nvme
    ("fpganode2", 11.28,  16, 25536.7, False),   # ssd-raid0
    ("opi5",     34.25,   8,  25537.7, True),    # data-fast
    ("rpi5",     41.32,   4,   6865.9, True),    # data-fast (k27-k32 CP4)
]

# ── Colours ──────────────────────────────────────────────────────────────────
CHIA_COLOR   = "#D62728"
VAULTX_COLOR = "#1F77B4"
ARM_COLOR    = "#5A9EC9"
SEP_COLOR    = "#888888"

ARM_MACHINES = {"thunderx1", "thunderx2", "opi5", "rpi5"}

Y_CAP = 100  # bars exceeding this are capped/hatched


def main():
    entries = []
    for lbl, t, mach, threads, mem_mb in CHIA_DATA:
        entries.append(("chia", lbl, t, CHIA_COLOR, mach, threads, mem_mb))

    sep_after = len(entries) - 1   # separator after last chia bar

    for mach, t, threads, mem_mb, is_arm in VAULTX_DATA:
        color = ARM_COLOR if is_arm else VAULTX_COLOR
        entries.append(("vaultx", mach, t, color, mach, threads, mem_mb))

    n = len(entries)
    x = np.arange(n)

    fig, ax = plt.subplots(figsize=(20, 7))

    for i, (kind, lbl, t, color, mach, threads, mem_mb) in enumerate(entries):
        capped = t > Y_CAP
        bar_h  = Y_CAP if capped else t
        ax.bar(x[i], bar_h, color=color, width=0.7, zorder=3,
               edgecolor="white", linewidth=0.5)

        if capped:
            ax.bar(x[i], bar_h, color="none", width=0.7, zorder=4,
                   edgecolor="white", linewidth=0.5, hatch="////")
            ax.text(x[i], bar_h * 0.5, f"{t:.0f} mins",
                    ha="center", va="center", fontsize=9, color="white",
                    fontweight="bold", zorder=5)
            ax.text(x[i] - 0.40, bar_h * 0.5, f"{t:.0f} mins (capped)",
                    ha="right", va="center", fontsize=8, color="black",
                    fontweight="bold", rotation=90, zorder=6)
        else:
            offset = Y_CAP * 0.012
            mem_gb = mem_mb / 1024.0
            if t < 10:
                ax.text(x[i], t + offset, f"{t:.2f}m\n{threads}T / {mem_gb:.0f}GB",
                        ha="center", va="bottom", fontsize=7, color="black",
                        zorder=6, linespacing=1.4)
            else:
                ax.text(x[i], t + offset, f"{t:.2f}m",
                        ha="center", va="bottom", fontsize=7, color="black", zorder=6)

                inside_thresh = Y_CAP * 0.12
                if t >= inside_thresh:
                    ann_text = f"{mach}\n{mem_gb:.0f}GB/{threads}T"
                    ax.text(x[i], t * 0.5, ann_text,
                            ha="center", va="center", fontsize=7.5,
                            color="white", fontweight="bold", zorder=5)

    # Separator between Chia and VaultX sections
    sep_x = sep_after + 0.5
    ax.axvline(sep_x, color=SEP_COLOR, linestyle=":", linewidth=1.5, zorder=4)
    ax.text(sep_x, Y_CAP * 0.97, "  Chia  ←|→  VaultX  ",
            ha="center", va="top", fontsize=8, color=SEP_COLOR, style="italic")

    xtick_labels = [lbl if kind == "chia" else mach
                    for kind, lbl, t, color, mach, threads, mem_mb in entries]
    ax.set_xticks(x)
    ax.set_xticklabels(xtick_labels, fontsize=8, rotation=15, ha="right")

    yticks = np.arange(0, Y_CAP + 10, 10)
    ax.set_yticks(yticks)
    ax.set_ylim(0, Y_CAP)
    ax.set_ylabel("Time (minutes)", fontsize=11)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.set_title("Best k32 Plotting Time on NVME",
                 fontsize=12, fontweight="bold")
    ax.text(0.99, 0.97, "↓ lower is better",
            transform=ax.transAxes, fontsize=8, ha="right", va="top",
            color="gray", style="italic")

    legend_patches = [
        mpatches.Patch(color=CHIA_COLOR,   label="Chia plotters"),
        mpatches.Patch(color=VAULTX_COLOR, label="VaultX (x86)"),
        mpatches.Patch(color=ARM_COLOR,    label="VaultX (ARM)"),
    ]
    ax.legend(handles=legend_patches, fontsize=9, loc="upper right")

    fig.tight_layout()
    out = os.path.join(IMAGES_DIR, "best_k32_nvme_comparison.png")
    fig.savefig(out, dpi=200, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {out}")


if __name__ == "__main__":
    print("Generating best k32 NVME comparison …")
    main()
    print("Done.")
