#!/usr/bin/env python3


from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot search benchmark CSV ",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "csv",
        help="CSV path from run_search_benchmark_by_files.sh",
    )
    parser.add_argument(
        "-o",
        "--output",
        help="Output PNG path; defaults to ./graphs/<csv_basename>.png",
    )
    parser.add_argument(
        "--title",
        help="Optional plot title override",
        default=None,
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Display the plot window after saving",
    )
    return parser.parse_args()


def resolve_output_path(csv_path: Path, override: str | None) -> Path:
    if override:
        return Path(override).expanduser().resolve()
    repo_root = Path(__file__).resolve().parents[1]
    return (repo_root / "graphs" / f"{csv_path.stem}.png").resolve()


def validate_columns(df: pd.DataFrame, required: Iterable[str]) -> None:
    missing = [col for col in required if col not in df.columns]
    if missing:
        raise ValueError(f"CSV is missing required column(s): {', '.join(missing)}")


def detect_sweep_dim(df: pd.DataFrame) -> str:
    uniq_t = df["t"].nunique()
    uniq_r = df["r"].nunique()
    if uniq_t > 1 and uniq_r == 1:
        return "t"
    if uniq_r > 1 and uniq_t == 1:
        return "r"
    # Fall back to mode column if present
    if "sweep_mode" in df.columns:
        if df["sweep_mode"].str.contains("t", case=False).any():
            return "t"
        if df["sweep_mode"].str.contains("r", case=False).any():
            return "r"
    raise ValueError("Cannot infer sweep dimension (need varying t or r)")


def plot_panels(df: pd.DataFrame, sweep_dim: str, keep_values: list[str]) -> plt.Figure:
    sns.set_theme(style="white", font_scale=1.0)
    fig, axes = plt.subplots(1, len(keep_values), figsize=(7.0 * len(keep_values), 5.0), squeeze=False)

    x_label = "I/O threads (-t)" if sweep_dim == "t" else "Record threads (-r)"
    x_col = sweep_dim

    grouped_map: dict[str, pd.DataFrame | None] = {}
    global_max = 0.0
    for keep_flag in keep_values:
        subset = df[df["keep_open"] == keep_flag]
        if subset.empty:
            grouped_map[keep_flag] = None
            continue

        grouped = (
            subset
            .groupby(x_col)
            .agg(avg_ms_per_lookup=("avg_ms_per_lookup", "mean"))
            .reset_index()
            .sort_values(x_col)
        )
        grouped_map[keep_flag] = grouped

        max_val = grouped["avg_ms_per_lookup"].max()
        if pd.notna(max_val):
            global_max = max(global_max, float(max_val))

    ylim_max = global_max * 1.1 if global_max > 0 else 1.0

    for idx, keep_flag in enumerate(keep_values):
        ax = axes[0, idx]
        grouped = grouped_map.get(keep_flag)
        if grouped is None or grouped.empty:
            ax.set_title(f"keep_open={keep_flag} (no data)")
            ax.axis("off")
            continue

        x_positions = range(len(grouped))
        bar_width = 0.5
        ax.bar(x_positions, grouped["avg_ms_per_lookup"], width=bar_width, color="#1f77b4", label="avg ms/lookup")
        ax.set_xlabel(x_label)
        ax.set_ylabel("Avg ms/lookup")
        ax.set_xticks(list(x_positions), grouped[x_col])
        ax.set_ylim(bottom=0.0, top=ylim_max)
        ax.legend(loc="best")
        ax.set_title(f"keep_open={keep_flag}")

    fig.tight_layout()
    return fig


def main() -> None:
    args = parse_args()
    csv_path = Path(args.csv).expanduser().resolve()
    if not csv_path.exists():
        raise FileNotFoundError(f"CSV not found: {csv_path}")

    df = pd.read_csv(csv_path)
    # Drop rows that are clearly malformed (e.g., stray single-value lines).
    df = df.dropna(how="all")
    required = {
        "t",
        "r",
        "keep_open",
        "avg_ms_per_lookup",
        "total_ms",
    }
    validate_columns(df, required)

    df["t"] = pd.to_numeric(df["t"], errors="coerce")
    df["r"] = pd.to_numeric(df["r"], errors="coerce")
    df["avg_ms_per_lookup"] = pd.to_numeric(df["avg_ms_per_lookup"], errors="coerce")
    df["total_ms"] = pd.to_numeric(df["total_ms"], errors="coerce")
    # Always recompute total_s from total_ms to avoid stale or missing values in CSV.
    df["total_s"] = df["total_ms"] / 1000.0
    df = df.dropna(subset=["t", "r", "avg_ms_per_lookup", "total_ms", "total_s", "keep_open"])
    df["t"] = df["t"].astype(int)
    df["r"] = df["r"].astype(int)
    df["keep_open"] = df["keep_open"].astype(str)

    sweep_dim = detect_sweep_dim(df)
    keep_values = sorted(df["keep_open"].unique())

    fig = plot_panels(df, sweep_dim, keep_values)
    title_text = args.title or f"VAULTX search benchmark ({csv_path.name})"
    fig.suptitle(title_text, fontsize=14, fontweight="bold")
    plt.tight_layout(rect=(0, 0, 1, 0.94))

    output_path = resolve_output_path(csv_path, args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=220)

    if args.show:
        plt.show()

    print(f"Saved plot to {output_path}")


if __name__ == "__main__":
    main()
