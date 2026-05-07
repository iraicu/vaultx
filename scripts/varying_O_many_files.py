#!/usr/bin/env python3
"""
Measure the impact of the -O (keep-open) flag when searching many files.

Runs the same search twice at each thread count – once with -O false
(close/reopen the file for every lookup) and once with -O true (keep the
file descriptor open) – so you can quantify the overhead of repeated open/
close operations as the file count grows.

How to run:
  # Both -O values, sweep -t automatically, all files in ./plots/
  python3 scripts/varying_O_many_files.py \\
      --dir ./plots/ \\
      --lookups 200 --difficulty 3 --r 1 \\
      --output ./data/varying_O_many_files.csv

  # Fixed thread count, explicit -t
  python3 scripts/varying_O_many_files.py \\
      --dir /mnt/hdd/plots/ \\
      --lookups 200 --t-values 1,4,8 \\
      --output ./data/varying_O_hdd.csv

  # Only compare the two -O values at a single fixed -t (no sweep)
  python3 scripts/varying_O_many_files.py \\
      --dir ./plots/ --t-values 4 --lookups 200 \\
      --output ./data/varying_O_fixed_t4.csv
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
    pow2_sweep,
    run_vaultx,
)

CSV_FIELDS = [
    "timestamp",
    "binary",
    "target_dir",
    "k_values",            # comma-separated unique K values present (e.g. "32" or "27,32")
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
    p.add_argument("--dir", required=True,
                   help="Directory containing plot files to search")
    p.add_argument("--lookups", type=int, default=200,
                   help="Random lookups per run (default: 200)")
    p.add_argument("--difficulty", type=int, default=3,
                   help="Hash prefix bytes (default: 3)")
    p.add_argument(
        "--t-values",
        help="Comma-separated -t values to sweep "
             "(default: powers-of-2 sweep to nproc)",
    )
    p.add_argument("--r", type=int, default=1,
                   help="Hash/record threads – -r flag (default: 1)")
    p.add_argument("--binary", default="./vaultx",
                   help="Path to vaultx binary (default: ./vaultx)")
    p.add_argument("--output", default="./data/varying_O_many_files.csv",
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

    files = find_plot_files(args.dir)
    if not files:
        sys.exit(f"ERROR: no plot files found in {args.dir}")

    n_files = len(files)
    total_size = sum(os.path.getsize(f) for f in files)
    k_vals = sorted({k for f in files if (k := extract_k(f)) is not None})
    k_str = ",".join(str(k) for k in k_vals) if k_vals else "unknown"

    nproc = os.cpu_count() or 1
    if args.t_values:
        t_list = sorted({int(x.strip()) for x in args.t_values.split(",") if x.strip()})
    else:
        t_list = pow2_sweep(nproc)

    out_path = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    print(f"Directory   : {args.dir}")
    print(f"Files       : {n_files}  ({total_size / 1e9:.3f} GB total)  K={k_str}")
    print(f"-t values   : {t_list}")
    print(f"-O values   : false, true")
    print(f"Output      : {out_path}\n")

    with open(out_path, "w", newline="") as csvf:
        writer = csv.DictWriter(csvf, fieldnames=CSV_FIELDS)
        writer.writeheader()

        for t in t_list:
            for keep_open in (False, True):
                o_str = "true" if keep_open else "false"
                print(f"[t={t}  -O {o_str}]", flush=True)

                if not args.no_drop_caches:
                    drop_caches()

                res = run_vaultx(
                    binary, args.dir,
                    args.lookups, args.difficulty,
                    t, args.r, keep_open,
                )

                if res is None:
                    print(f"  SKIP – run failed\n")
                    continue

                writer.writerow(
                    {
                        "timestamp":            now_iso(),
                        "binary":               args.binary,
                        "target_dir":           args.dir,
                        "k_values":             k_str,
                        "n_files":              n_files,
                        "total_file_size_bytes": total_size,
                        "total_file_size_gb":   round(total_size / (1024 ** 3), 6),
                        "t":                    t,
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
                    f"open_close={res['open_close_ms']:.4f} ms  "
                    f"(seek={res['seek_ms']:.4f} read={res['read_ms']:.4f} "
                    f"hash={res['hash_ms']:.4f})\n"
                )

    print(f"Done. Results: {out_path}")


if __name__ == "__main__":
    main()
