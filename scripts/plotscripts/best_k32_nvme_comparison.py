#!/usr/bin/env python3
"""
Task 3: Best k32 plotting time on NVME for each machine.
Wide bar chart: Chia plotters (red, left) | VaultX x86 (blue) | VaultX ARM (steel-blue, right).
Labels inside each bar: machine name, core count, available RAM.
"""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
IMAGES_DIR  = os.path.join(SCRIPT_DIR, "..", "..", "Paper", "images")
os.makedirs(IMAGES_DIR, exist_ok=True)

# ── Static data (update values here if measurements change) ──────────────────
# Chia plotters – best k32 time on NVME (minutes)
# ChiaPOS on s8 is missing.
CHIA_DATA = [
    # (label, time_min, machine_display, cores, mem_gb)
    ("ChiaPOS\n(s8)",       506.0, "s8",      384, 770),
    ("Madmax\n(s8)",         27.85, "s8",      384, 770),
    ("Bladebit\n(epycbox)", 17.92, "epycbox", 128, 192),
]

# VaultX – best k32 time on NVME (minutes), sorted by thread count descending
VAULTX_DATA = [
    # (machine_display, time_min, cores, mem_gb, is_arm)
    ("s8",        1.83,  384, 770,  False),
    ("thunderx2", 5.18,  224, 118,  True),
    ("epycbox",   3.54,  128, 192,  False),
    ("gpubox",    2.87,   96, 384,  False),
    ("thunderx1", 6.19,   96,  64,  True),
    ("nvmebox",   3.27,   64, 187,  False),
    ("athena",    5.82,   48,  64,  False),
    ("torus",     6.39,   32,  64,  False),
    ("fpganode2", 14.22,  16,  32,  False),
    ("opi5",     33.3,    8,  32,  True),
    ("rpi5",     41.32,   4,   8,  True),
]

# ── Colors ───────────────────────────────────────────────────────────────────
CHIA_COLOR   = "#D62728"       # red
VAULTX_COLOR = "#1F77B4"       # blue
ARM_COLOR    = "#5A9EC9"       # medium blue for ARM machines (visible labels)
SEP_COLOR    = "#888888"

ARM_MACHINES = {"thunderx1", "thunderx2", "opi5", "rpi5"}


def bar_label(machine: str, cores: int, mem_gb: int, time_min: float,
              max_time: float, ax_height: float) -> tuple[str, float, str]:
    """Return (text, y_position, va) for the annotation inside/outside the bar."""
    label_text = f"{machine}\n{cores}T / {mem_gb}GB"
    inside_threshold = max_time * 0.12  # bars shorter than this get label above
    if time_min >= inside_threshold:
        return label_text, time_min * 0.5, "center"
    else:
        return label_text, time_min + max_time * 0.01, "bottom"


Y_CAP = 100   # y-axis maximum; bars exceeding this are drawn at cap with bold label


def main():
    entries = []
    for lbl, t, mach, cores, mem in CHIA_DATA:
        entries.append(("chia", lbl, t, CHIA_COLOR, mach, cores, mem))

    sep_after = len(entries) - 1   # separator goes after last chia bar

    for mach, t, cores, mem, is_arm in VAULTX_DATA:
        color = ARM_COLOR if is_arm else VAULTX_COLOR
        entries.append(("vaultx", mach, t, color, mach, cores, mem))

    n = len(entries)
    x = np.arange(n)

    fig, ax = plt.subplots(figsize=(18, 7))

    for i, (kind, lbl, t, color, mach, cores, mem) in enumerate(entries):
        capped  = t > Y_CAP
        bar_h   = Y_CAP if capped else t
        ax.bar(x[i], bar_h, color=color, width=0.7, zorder=3,
               edgecolor="white", linewidth=0.5)

        if capped:
            # Hatching to signal "bar continues beyond"
            ax.bar(x[i], bar_h, color="none", width=0.7, zorder=4,
                   edgecolor="white", linewidth=0.5, hatch="////")
            # White time label centred inside the bar
            ax.text(x[i], bar_h * 0.5, f"{t:.0f} mins",
                    ha="center", va="center", fontsize=9, color="white",
                    fontweight="bold", zorder=5)
            # Vertical label on the left side of the bar (written upwards)
            ax.text(x[i] - 0.40, bar_h * 0.5, f"{t:.0f} mins (capped)",
                    ha="right", va="center", fontsize=8, color="black",
                    fontweight="bold", rotation=90, zorder=6)
        else:
            offset = Y_CAP * 0.012
            if t < 10:
                # Bar too short for inside labels — show all info above
                ax.text(x[i], t + offset, f"{t:.2f}m\n{cores}T / {mem}GB",
                        ha="center", va="bottom", fontsize=7, color="black",
                        zorder=6, linespacing=1.4)
            else:
                # Time label just above bar
                ax.text(x[i], t + offset, f"{t:.2f}m",
                        ha="center", va="bottom", fontsize=7, color="black", zorder=6)

                # Machine/spec annotation inside the bar if tall enough
                inside_thresh = Y_CAP * 0.12
                if t >= inside_thresh:
                    ann_text = f"{mach}\n{mem}GB/{cores}T"
                    ax.text(x[i], t * 0.5, ann_text,
                            ha="center", va="center", fontsize=7.5,
                            color="white", fontweight="bold", zorder=5)

    # Dotted separator between Chia and VaultX
    sep_x = sep_after + 0.5
    ax.axvline(sep_x, color=SEP_COLOR, linestyle=":", linewidth=1.5, zorder=4)
    ax.text(sep_x, Y_CAP * 0.97, "  Chia  ←|→  VaultX  ",
            ha="center", va="top", fontsize=8, color=SEP_COLOR, style="italic")

    # x-tick labels
    xtick_labels = [lbl if kind == "chia" else mach
                    for kind, lbl, t, color, mach, cores, mem in entries]
    ax.set_xticks(x)
    ax.set_xticklabels(xtick_labels, fontsize=8, rotation=15, ha="right")

    # Fixed y-axis 0–100
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
