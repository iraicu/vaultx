#!/usr/bin/env python3
"""
Measure average search time per lookup across different K values.

One benchmark run is performed per K value (on the first file found for that K
in the target directory).  Results are written to a CSV with full timing
breakdowns suitable for graphing.

How to run:
  # Basic – run on all K values found in ./plots/, 100 lookups each
  python3 scripts/search_time_varying_k.py --dir ./plots/ --lookups 100

  # Restrict K range, set threads, change output path
  python3 scripts/search_time_varying_k.py \\
      --dir /mnt/hdd/plots/ \\
      --k-list 27,28,29,30,31,32 \\
      --lookups 200 --difficulty 3 \\
      --t 1 --r 1 --keep-open false \\
      --binary ./vaultx \\
      --output ./data/search_time_varying_k.csv

  # Plot the result (once you have a plotting script)
  # python3 scripts/plot_search_time_varying_k.py ./data/search_time_varying_k.csv
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
    now_iso,
    run_vaultx,
)

CSV_FIELDS = [
    "timestamp",
    "binary",
    "k",
    "filename",
    "file_size_bytes",
    "file_size_gb",
    "n_files",
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
        help="Directory containing plot files of various K values",
    )
    p.add_argument(
        "--k-list",
        help="Comma-separated K values to test, e.g. 27,28,29,30,31,32 "
             "(default: all K values found in --dir)",
    )
    p.add_argument("--lookups", type=int, default=100,
                   help="Random lookups per run (default: 100)")
    p.add_argument("--difficulty", type=int, default=3,
                   help="Hash prefix bytes for each lookup (default: 3)")
    p.add_argument("--t", type=int, default=1,
                   help="I/O (read) threads – -t flag (default: 1)")
    p.add_argument("--r", type=int, default=1,
                   help="Hash/record threads – -r flag (default: 1)")
    p.add_argument("--keep-open", choices=["true", "false"], default="false",
                   help="Keep plot file open during search – -O flag (default: false)")
    p.add_argument("--binary", default="./vaultx",
                   help="Path to vaultx binary (default: ./vaultx)")
    p.add_argument("--output", default="./data/search_time_varying_k.csv",
                   help="Output CSV path (default: ./data/search_time_varying_k.csv)")
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

    # Group plot files by K
    all_files = find_plot_files(args.dir)
    by_k: dict[int, list[str]] = {}
    for f in all_files:
        k = extract_k(f)
        if k is not None:
            by_k.setdefault(k, []).append(f)

    if not by_k:
        sys.exit(f"ERROR: no k*.plot files found in {args.dir}")

    k_filter: set[int] | None = None
    if args.k_list:
        k_filter = {int(x.strip()) for x in args.k_list.split(",")}

    keep_open = args.keep_open == "true"

    out_path = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    k_list = sorted(k for k in by_k if k_filter is None or k in k_filter)
    if not k_list:
        sys.exit(f"ERROR: no files match requested K values {k_filter}")

    print(f"K values to benchmark: {k_list}")
    print(f"Output: {out_path}\n")

    with open(out_path, "w", newline="") as csvf:
        writer = csv.DictWriter(csvf, fieldnames=CSV_FIELDS)
        writer.writeheader()

        for k in k_list:
            filepath = by_k[k][0]  # use first file for this K
            file_size = os.path.getsize(filepath)

            print(f"[K={k}]  {os.path.basename(filepath)}  ({file_size / 1e9:.3f} GB)")

            if not args.no_drop_caches:
                drop_caches()

            res = run_vaultx(
                binary, filepath,
                args.lookups, args.difficulty,
                args.t, args.r, keep_open,
            )

            if res is None:
                print(f"  SKIP K={k} – run failed\n")
                continue

            writer.writerow(
                {
                    "timestamp":        now_iso(),
                    "binary":           args.binary,
                    "k":                k,
                    "filename":         filepath,
                    "file_size_bytes":  file_size,
                    "file_size_gb":     round(file_size / (1024 ** 3), 6),
                    "n_files":          1,
                    "t":                args.t,
                    "r":                args.r,
                    "keep_open":        keep_open,
                    "difficulty":       args.difficulty,
                    "n_lookups":        res["n_lookups"],
                    "found":            res["found"],
                    "not_found":        res["not_found"],
                    "all_matches":      res["matches"],
                    "open_close_ms":    round(res["open_close_ms"], 4),
                    "seek_ms":          round(res["seek_ms"], 4),
                    "read_ms":          round(res["read_ms"], 4),
                    "hash_ms":          round(res["hash_ms"], 4),
                    "avg_ms_per_lookup": round(res["avg_ms_per_lookup"], 4),
                    "total_wall_ms":    round(res["total_wall_ms"], 4),
                    "total_s":          round(res["total_wall_ms"] / 1000.0, 6),
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
