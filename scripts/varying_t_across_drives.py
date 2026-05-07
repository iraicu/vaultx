#!/usr/bin/env python3
"""
Compare the impact of I/O thread count (-t) across different drive types.

Sweeps -t in powers of 2 up to nproc for each drive directory specified.
Only files matching --k are searched so that comparisons are like-for-like.
Typical use: compare HDD vs SSD vs NVMe with K32 plots.

How to run:
  # Compare three drives, K=32, sweep -t automatically
  python3 scripts/varying_t_across_drives.py \\
      --drives "HDD:/mnt/hdd/plots SSD:/mnt/ssd/plots NVMe:/mnt/nvme/plots" \\
      --k 32 --lookups 200 --difficulty 3 --r 1 \\
      --output ./data/varying_t_across_drives_k32.csv

  # Specify explicit thread counts
  python3 scripts/varying_t_across_drives.py \\
      --drives "HDD:/mnt/hdd/plots SSD:/mnt/ssd/plots" \\
      --k 32 --lookups 200 \\
      --t-values 1,2,4,8,16 \\
      --output ./data/varying_t_across_drives_k32.csv

Drive format: LABEL:PATH  (space-separated list, use quotes around the whole value)
"""

import argparse
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vaultx_bench_utils import (
    drop_caches,
    find_plot_files,
    now_iso,
    pow2_sweep,
    run_vaultx,
)

CSV_FIELDS = [
    "timestamp",
    "binary",
    "drive_label",
    "drive_path",
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


def parse_drives(spec: str) -> list[tuple[str, str]]:
    """
    Parse 'LABEL:PATH LABEL:PATH …' into a list of (label, path) tuples.
    Paths may themselves contain colons on Linux (rare but handled via rsplit).
    """
    drives = []
    for token in spec.strip().split():
        label, _, path = token.partition(":")
        if not label or not path:
            sys.exit(
                f"ERROR: bad drive spec '{token}'.  "
                "Expected LABEL:PATH format, e.g. HDD:/mnt/hdd/plots"
            )
        drives.append((label, path))
    return drives


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "--drives", required=True,
        help='Space-separated list of LABEL:PATH pairs, e.g. '
             '"HDD:/mnt/hdd/plots SSD:/mnt/ssd/plots"',
    )
    p.add_argument("--k", type=int, default=32,
                   help="K value to use for comparisons (default: 32)")
    p.add_argument("--lookups", type=int, default=200,
                   help="Random lookups per run (default: 200)")
    p.add_argument("--difficulty", type=int, default=3,
                   help="Hash prefix bytes (default: 3)")
    p.add_argument(
        "--t-values",
        help="Comma-separated -t values to test (default: powers-of-2 sweep to nproc)",
    )
    p.add_argument("--r", type=int, default=1,
                   help="Hash/record threads – -r flag (default: 1)")
    p.add_argument("--keep-open", choices=["true", "false"], default="false",
                   help="Keep files open – -O flag (default: false)")
    p.add_argument("--binary", default="./vaultx",
                   help="Path to vaultx binary (default: ./vaultx)")
    p.add_argument("--output", default="./data/varying_t_across_drives.csv",
                   help="Output CSV path")
    p.add_argument("--no-drop-caches", action="store_true",
                   help="Skip dropping OS page cache between runs")
    return p.parse_args()


def main() -> None:
    args = parse_args()

    binary = os.path.abspath(args.binary)
    if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
        sys.exit(f"ERROR: vaultx binary not found or not executable: {binary}")

    drives = parse_drives(args.drives)

    nproc = os.cpu_count() or 1
    if args.t_values:
        t_list = sorted({int(x.strip()) for x in args.t_values.split(",") if x.strip()})
    else:
        t_list = pow2_sweep(nproc)

    keep_open = args.keep_open == "true"
    out_path = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    print(f"Drives      : {[f'{l}:{p}' for l, p in drives]}")
    print(f"K           : {args.k}")
    print(f"-t values   : {t_list}")
    print(f"Output      : {out_path}\n")

    with open(out_path, "w", newline="") as csvf:
        writer = csv.DictWriter(csvf, fieldnames=CSV_FIELDS)
        writer.writeheader()

        for label, drive_path in drives:
            if not os.path.isdir(drive_path):
                print(f"WARN: {label}:{drive_path} is not a directory – skipping")
                continue

            files = find_plot_files(drive_path, k=args.k)
            if not files:
                print(f"WARN: no K={args.k} files in {drive_path} ({label}) – skipping")
                continue

            n_files = len(files)
            total_size = sum(os.path.getsize(f) for f in files)
            print(
                f"[{label}]  {n_files} file(s)  "
                f"{total_size / 1e9:.3f} GB  path={drive_path}"
            )

            for t in t_list:
                print(f"  t={t}", end="  ", flush=True)

                if not args.no_drop_caches:
                    drop_caches()

                res = run_vaultx(
                    binary, drive_path,
                    args.lookups, args.difficulty,
                    t, args.r, keep_open,
                )

                if res is None:
                    print("SKIP – run failed")
                    continue

                writer.writerow(
                    {
                        "timestamp":            now_iso(),
                        "binary":               args.binary,
                        "drive_label":          label,
                        "drive_path":           drive_path,
                        "k":                    args.k,
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
                    f"avg={res['avg_ms_per_lookup']:.4f} ms  "
                    f"total={res['total_wall_ms']:.1f} ms"
                )

            print()

    print(f"Done. Results: {out_path}")


if __name__ == "__main__":
    main()
