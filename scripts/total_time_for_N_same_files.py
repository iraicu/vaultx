#!/usr/bin/env python3
"""
Measure total search time for N files of the same K value.

Varies the number of files searched simultaneously (first N files taken from a
directory), keeping lookups fixed, to show how throughput/latency scales with
file count.  A temporary symlink directory is used to present exactly N files
to vaultx without modifying the source directory.

How to run:
  # Sweep 1, 2, 4, 8 files of K=32 in ./plots/k32/
  python3 scripts/total_time_for_N_same_files.py \\
      --dir ./plots/k32/ --k 32 \\
      --n-values 1,2,4,8 \\
      --lookups 100 --difficulty 3 \\
      --t 4 --r 1 --keep-open false \\
      --output ./data/total_time_N_same_files_k32.csv

  # Use all N values up to however many files exist (powers of 2)
  python3 scripts/total_time_for_N_same_files.py \\
      --dir ./plots/ --k 32 \\
      --lookups 200 \\
      --output ./data/total_time_N_same_files_k32.csv
"""

import argparse
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vaultx_bench_utils import (
    drop_caches,
    find_plot_files,
    make_symlink_dir,
    now_iso,
    pow2_sweep,
    run_vaultx,
)

CSV_FIELDS = [
    "timestamp",
    "binary",
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
    p.add_argument(
        "--dir", required=True,
        help="Directory containing plot files (filtered by --k if given)",
    )
    p.add_argument(
        "--k", type=int, default=None,
        help="K value to filter files by.  If omitted, all files in --dir are used "
             "regardless of K.",
    )
    p.add_argument(
        "--n-values",
        help="Comma-separated N file counts to test, e.g. 1,2,4,8 "
             "(default: powers-of-2 sweep up to total available files)",
    )
    p.add_argument("--lookups", type=int, default=100,
                   help="Random lookups per run (default: 100)")
    p.add_argument("--difficulty", type=int, default=3,
                   help="Hash prefix bytes (default: 3)")
    p.add_argument("--t", type=int, default=4,
                   help="I/O (read) threads – -t flag (default: 4)")
    p.add_argument("--r", type=int, default=1,
                   help="Hash/record threads – -r flag (default: 1)")
    p.add_argument("--keep-open", choices=["true", "false"], default="false",
                   help="Keep files open – -O flag (default: false)")
    p.add_argument("--binary", default="./vaultx",
                   help="Path to vaultx binary (default: ./vaultx)")
    p.add_argument("--output", default="./data/total_time_N_same_files.csv",
                   help="Output CSV path")
    p.add_argument("--no-drop-caches", action="store_true",
                   help="Skip dropping OS page cache between runs")
    return p.parse_args()


def main() -> None:
    args = parse_args()

    binary = os.path.abspath(args.binary)
    if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
        sys.exit(f"ERROR: vaultx binary not found or not executable: {binary}")

    if not os.path.isdir(args.dir):
        sys.exit(f"ERROR: --dir is not a directory: {args.dir}")

    all_files = find_plot_files(args.dir, k=args.k)
    if not all_files:
        k_hint = f" with K={args.k}" if args.k is not None else ""
        sys.exit(f"ERROR: no plot files{k_hint} found in {args.dir}")

    max_n = len(all_files)
    print(f"Found {max_n} file(s) in {args.dir}")

    if args.n_values:
        n_list = [int(x.strip()) for x in args.n_values.split(",")]
        n_list = sorted(set(n for n in n_list if 1 <= n <= max_n))
    else:
        n_list = [n for n in pow2_sweep(max_n) if n <= max_n]

    if not n_list:
        sys.exit("ERROR: no valid N values after filtering against available file count")

    keep_open = args.keep_open == "true"
    out_path = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    print(f"N values to sweep: {n_list}")
    print(f"Output: {out_path}\n")

    with open(out_path, "w", newline="") as csvf:
        writer = csv.DictWriter(csvf, fieldnames=CSV_FIELDS)
        writer.writeheader()

        for n in n_list:
            selected = all_files[:n]
            total_size = sum(os.path.getsize(f) for f in selected)
            k_display = args.k if args.k is not None else "mixed"

            print(f"[N={n}  K={k_display}]  total_size={total_size / 1e9:.3f} GB")

            # Create a temp dir with exactly N symlinked files so vaultx scans only them
            tmpdir, cleanup = make_symlink_dir(selected)
            try:
                if not args.no_drop_caches:
                    drop_caches()

                res = run_vaultx(
                    binary, tmpdir,
                    args.lookups, args.difficulty,
                    args.t, args.r, keep_open,
                )
            finally:
                cleanup()

            if res is None:
                print(f"  SKIP N={n} – run failed\n")
                continue

            writer.writerow(
                {
                    "timestamp":            now_iso(),
                    "binary":               args.binary,
                    "k":                    k_display,
                    "n_files":              n,
                    "total_file_size_bytes": total_size,
                    "total_file_size_gb":   round(total_size / (1024 ** 3), 6),
                    "t":                    args.t,
                    "r":                    args.r,
                    "keep_open":            keep_open,
                    "difficulty":           args.difficulty,
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
                f"  avg={res['avg_ms_per_lookup']:.4f} ms/lookup  "
                f"total={res['total_wall_ms']:.1f} ms  "
                f"(open={res['open_close_ms']:.4f} seek={res['seek_ms']:.4f} "
                f"read={res['read_ms']:.4f} hash={res['hash_ms']:.4f})\n"
            )

    print(f"Done. Results: {out_path}")


if __name__ == "__main__":
    main()
