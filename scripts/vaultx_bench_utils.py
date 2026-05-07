"""
vaultx_bench_utils.py
Shared utilities for all VaultX search benchmark scripts.

Not meant to be run directly.
"""

import os
import re
import shutil
import subprocess
import tempfile
from datetime import datetime, timezone

# Matches filenames like k27-<hex>.plot or k32-<hex>.plot
_PLOT_NAME_RE = re.compile(r'^k(\d+)-[0-9a-fA-F]+\.plot$')


def extract_k(path: str) -> int | None:
    """Return K integer from a plot filename, or None if not parseable."""
    m = _PLOT_NAME_RE.match(os.path.basename(path))
    return int(m.group(1)) if m else None


def find_plot_files(directory: str, k: int | None = None) -> list[str]:
    """
    Return a sorted list of plot file paths in `directory`.
    If `k` is given, only return files whose filename K matches.
    """
    results = []
    try:
        for name in sorted(os.listdir(directory)):
            full = os.path.join(directory, name)
            if not (os.path.isfile(full) or os.path.islink(full)):
                continue
            is_kplot = _PLOT_NAME_RE.match(name) is not None
            is_merge = name.startswith("merge_") and name.endswith(".plot")
            if not is_kplot and not is_merge:
                continue
            if k is not None and extract_k(name) != k:
                continue
            results.append(full)
    except OSError as exc:
        print(f"ERROR listing {directory}: {exc}")
    return results


def make_symlink_dir(files: list[str]) -> tuple[str, callable]:
    """
    Create a temporary directory containing symlinks to each file in `files`.
    Returns (tmpdir_path, cleanup_fn).  Call cleanup_fn() when done.
    """
    tmpdir = tempfile.mkdtemp(prefix="vaultx_bench_")
    for src in files:
        link = os.path.join(tmpdir, os.path.basename(src))
        os.symlink(os.path.abspath(src), link)
    return tmpdir, lambda: shutil.rmtree(tmpdir, ignore_errors=True)


def drop_caches() -> None:
    """Best-effort drop of OS page cache."""
    drop_path = "/proc/sys/vm/drop_caches"
    try:
        if os.access(drop_path, os.W_OK):
            with open(drop_path, "w") as f:
                f.write("3\n")
            return
    except OSError:
        pass
    try:
        subprocess.run(
            ["sudo", "-n", "sh", "-c", f"sync; echo 3 > {drop_path}"],
            check=False,
            capture_output=True,
            timeout=10,
        )
    except Exception:
        pass


def run_vaultx(
    binary: str,
    target: str,
    lookups: int,
    difficulty: int,
    t: int,
    r: int,
    keep_open: bool,
) -> dict | None:
    """
    Run a vaultx batch search and return a dict of parsed metrics.
    Returns None if the run fails or output cannot be parsed.

    Returned dict keys:
      open_close_ms, seek_ms, read_ms, hash_ms   – avg per lookup (wall-clock)
      total_wall_ms, avg_ms_per_lookup
      n_lookups, found, not_found, matches
      per_file  – list of per-file dicts (filename, file_size_bytes, k,
                  lookups, found, not_found, matches,
                  avg_ms_per_lookup, total_ms)
    """
    cmd = [
        binary,
        "-S", str(lookups),
        "-D", str(difficulty),
        "-f", target,
        "-t", str(t),
        "-r", str(r),
        "-O", "true" if keep_open else "false",
    ]
    print(f"  --> {' '.join(cmd)}", flush=True)

    try:
        proc = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
    except Exception as exc:
        print(f"  ERROR spawning vaultx: {exc}")
        return None

    output = proc.stdout or ""

    if proc.returncode != 0:
        print(f"  WARN: vaultx exited {proc.returncode}")
        print(output)
        return None

    # TIMING line (printed without -b flag):
    #   TIMING open_close_ms  seek_ms  read_ms  hash_ms  total_wall_ms  avg_per_lookup_ms
    timing_m = re.search(
        r"^TIMING\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)",
        output,
        re.MULTILINE,
    )
    if not timing_m:
        print("  WARN: TIMING line not found – is -b true set? (should not be)")
        print(output)
        return None

    # TOTAL (all lookups) line:
    #   TOTAL (all lookups) <blank-size-col> lookups found not_found matches avg total
    total_m = re.search(
        r"TOTAL \(all lookups\)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)",
        output,
    )
    n_lookups = lookups
    found = not_found = matches = 0
    if total_m:
        n_lookups = int(total_m.group(1))
        found     = int(total_m.group(2))
        not_found = int(total_m.group(3))
        matches   = int(total_m.group(4))

    # Per-file result lines:
    #   <path>.plot  <size_bytes>  <lookups>  <found>  <not_found>  <matches>  <avg_ms>  <total_ms>
    per_file: list[dict] = []
    for line in output.splitlines():
        m = re.match(
            r"^(\S+\.plot)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)",
            line.strip(),
        )
        if m:
            per_file.append(
                {
                    "filename":        m.group(1),
                    "file_size_bytes": int(m.group(2)),
                    "lookups":         int(m.group(3)),
                    "found":           int(m.group(4)),
                    "not_found":       int(m.group(5)),
                    "matches":         int(m.group(6)),
                    "avg_ms_per_lookup": float(m.group(7)),
                    "total_ms":        float(m.group(8)),
                    "k":               extract_k(m.group(1)),
                }
            )

    return {
        "open_close_ms":    float(timing_m.group(1)),
        "seek_ms":          float(timing_m.group(2)),
        "read_ms":          float(timing_m.group(3)),
        "hash_ms":          float(timing_m.group(4)),
        "total_wall_ms":    float(timing_m.group(5)),
        "avg_ms_per_lookup": float(timing_m.group(6)),
        "n_lookups":        n_lookups,
        "found":            found,
        "not_found":        not_found,
        "matches":          matches,
        "per_file":         per_file,
    }


def now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def pow2_sweep(max_val: int) -> list[int]:
    """Return [1, 2, 4, …, max_val] with max_val always included."""
    vals, v = [], 1
    while v < max_val:
        vals.append(v)
        v *= 2
    vals.append(max_val)
    return list(dict.fromkeys(vals))  # deduplicate while preserving order
