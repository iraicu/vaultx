#!/usr/bin/env python3
"""
Compare the impact of hash/record threads (-r) on two scenarios:
  A) A single large K plot (e.g. K35)
  B) Multiple smaller K plots (e.g. K32) whose combined size ≈ the large plot

Both scenarios are benchmarked at each -r value so you can see whether -r
helps more with one large file or many smaller ones of equivalent total data.

How to run:
  # Single K35 file vs K32 files of similar combined size on HDD
  python3 scripts/varying_r_large_vs_small.py \\
      --large-file /mnt/hdd/k35-abc123.plot \\
      --small-dir  /mnt/hdd/k32/ \\
      --lookups 200 --difficulty 3 --t 1 \\
      --output ./data/varying_r_large_vs_small.csv

  # Explicit -r sweep
  python3 scripts/varying_r_large_vs_small.py \\
      --large-file /mnt/hdd/k35-abc123.plot \\
      --small-dir  /mnt/hdd/k32/ \\
      --r-values 1,2,4,8,16,32 \\
      --lookups 200 \\
      --output ./data/varying_r_large_vs_small.csv

Notes:
  * -t is fixed at 1 during the -r sweep (single-file: only -r varies).
  * For the multi-file scenario, the first N small files whose combined size
    does not exceed the large file size (allowing up to --size-tolerance %)
    are selected.
  * Use --size-tolerance to widen or narrow the match window (default: 20 %).
"""

import argparse
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vaultx_bench_utils import (
    drop_caches,
    extract_k,
    find_plot_files,
    make_symlink_dir,
    now_iso,
    pow2_sweep,
    run_vaultx,
)

CSV_FIELDS = [
    "timestamp",
    "binary",
    "scenario",        # "single_large" or "multi_small"
    "k",
    "n_files",
    "total_file_size_bytes",
    "total_file_size_gb",
    "t",
    "r",
    "keep_open",
    "difficulty",
    "n_lookups",
    "found",
    "not_found",
    "all_matches",
    "open_close_ms",
    "seek_ms",
    "read_ms",
    "hash_ms",
    "avg_ms_per_lookup",
    "total_wall_ms",
    "total_s",
]


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("--large-file", required=True,
                   help="Path to the single large K plot file (e.g. K35)")
    p.add_argument("--small-dir", required=True,
                   help="Directory containing the smaller K plot files (e.g. K32)")
    p.add_argument(
        "--r-values",
        help="Comma-separated -r values to sweep "
             "(default: powers-of-2 sweep up to nproc)",
    )
    p.add_argument("--lookups", type=int, default=200,
                   help="Random lookups per run (default: 200)")
    p.add_argument("--difficulty", type=int, default=3,
                   help="Hash prefix bytes (default: 3)")
    p.add_argument("--t", type=int, default=1,
                   help="I/O threads – -t flag, kept fixed during -r sweep (default: 1)")
    p.add_argument("--keep-open", choices=["true", "false"], default="false",
                   help="Keep files open – -O flag (default: false)")
    p.add_argument(
        "--size-tolerance", type=float, default=20.0,
        help="Allow combined small-file size to exceed large file size by up to "
             "this %% before capping (default: 20)",
    )
    p.add_argument("--binary", default="./vaultx",
                   help="Path to vaultx binary (default: ./vaultx)")
    p.add_argument("--output", default="./data/varying_r_large_vs_small.csv",
                   help="Output CSV path")
    p.add_argument("--no-drop-caches", action="store_true",
                   help="Skip dropping OS page cache between runs")
    return p.parse_args()


def select_small_files(
    small_dir: str, large_size: int, tolerance_pct: float
) -> list[str]:
    """
    Return a list of files from small_dir whose combined size is closest to
    large_size without exceeding large_size * (1 + tolerance_pct / 100).
    """
    files = find_plot_files(small_dir)
    if not files:
        return []

    cap = large_size * (1.0 + tolerance_pct / 100.0)
    selected, running = [], 0
    for f in files:
        sz = os.path.getsize(f)
        if running + sz > cap:
            break
        selected.append(f)
        running += sz
    return selected


def run_scenario(
    binary, target, lookups, difficulty, t, r, keep_open,
    scenario, k, n_files, total_size, no_drop,
    writer, csvf, args,
) -> None:
    print(
        f"  [{scenario}]  K={k}  n_files={n_files}  "
        f"size={total_size / 1e9:.3f} GB  r={r}",
        flush=True,
    )

    if not no_drop:
        drop_caches()

    res = run_vaultx(binary, target, lookups, difficulty, t, r, keep_open)
    if res is None:
        print("    SKIP – run failed")
        return

    writer.writerow(
        {
            "timestamp":            now_iso(),
            "binary":               args.binary,
            "scenario":             scenario,
            "k":                    k,
            "n_files":              n_files,
            "total_file_size_bytes": total_size,
            "total_file_size_gb":   round(total_size / (1024 ** 3), 6),
            "t":                    t,
            "r":                    r,
            "keep_open":            keep_open,
            "difficulty":           difficulty,
            "n_lookups":            res["n_lookups"],
            "found":                res["found"],
            "not_found":            res["not_found"],
            "all_matches":          res["matches"],
            "open_close_ms":        round(res["open_close_ms"], 4),
            "seek_ms":              round(res["seek_ms"], 4),
            "read_ms":              round(res["read_ms"], 4),
            "hash_ms":              round(res["hash_ms"], 4),
            "avg_ms_per_lookup":    round(res["avg_ms_per_lookup"], 4),
            "total_wall_ms":        round(res["total_wall_ms"], 4),
            "total_s":              round(res["total_wall_ms"] / 1000.0, 6),
        }
    )
    csvf.flush()

    print(
        f"    avg={res['avg_ms_per_lookup']:.4f} ms  "
        f"total={res['total_wall_ms']:.1f} ms  "
        f"(open={res['open_close_ms']:.4f} seek={res['seek_ms']:.4f} "
        f"read={res['read_ms']:.4f} hash={res['hash_ms']:.4f})"
    )


def main() -> None:
    args = parse_args()

    binary = os.path.abspath(args.binary)
    if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
        sys.exit(f"ERROR: vaultx binary not found or not executable: {binary}")

    large_file = os.path.abspath(args.large_file)
    if not os.path.isfile(large_file):
        sys.exit(f"ERROR: large file not found: {large_file}")

    if not os.path.isdir(args.small_dir):
        sys.exit(f"ERROR: --small-dir is not a directory: {args.small_dir}")

    large_size = os.path.getsize(large_file)
    large_k = extract_k(large_file) or "?"

    small_files = select_small_files(args.small_dir, large_size, args.size_tolerance)
    if not small_files:
        sys.exit(
            f"ERROR: no small files found in {args.small_dir} "
            f"(or none fit within size tolerance)"
        )

    small_size = sum(os.path.getsize(f) for f in small_files)
    small_k = extract_k(small_files[0]) if small_files else "?"
    n_small = len(small_files)

    nproc = os.cpu_count() or 1
    if args.r_values:
        r_list = sorted({int(x.strip()) for x in args.r_values.split(",") if x.strip()})
    else:
        r_list = pow2_sweep(nproc)

    keep_open = args.keep_open == "true"
    out_path = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    print(f"Large file  : K={large_k}  {large_size / 1e9:.3f} GB  {os.path.basename(large_file)}")
    print(f"Small files : K={small_k}  {n_small} file(s)  {small_size / 1e9:.3f} GB")
    print(f"-r values   : {r_list}")
    print(f"Output      : {out_path}\n")

    with open(out_path, "w", newline="") as csvf:
        writer = csv.DictWriter(csvf, fieldnames=CSV_FIELDS)
        writer.writeheader()

        for r in r_list:
            # Scenario A: single large file
            run_scenario(
                binary, large_file,
                args.lookups, args.difficulty, args.t, r, keep_open,
                "single_large", large_k, 1, large_size,
                args.no_drop_caches, writer, csvf, args,
            )

            # Scenario B: multiple small files via temp symlink dir
            tmpdir, cleanup = make_symlink_dir(small_files)
            try:
                run_scenario(
                    binary, tmpdir,
                    args.lookups, args.difficulty, args.t, r, keep_open,
                    "multi_small", small_k, n_small, small_size,
                    args.no_drop_caches, writer, csvf, args,
                )
            finally:
                cleanup()

            print()

    print(f"Done. Results: {out_path}")


if __name__ == "__main__":
    main()
