#!/usr/bin/env bash
# k32merge.sh
#
# For each N in N_VALUES, and for each (TEMP_DRIVES[i], FINAL_DRIVES[i]) pair:
#   1. Generate N k32 subplots in TEMP_DRIVE using -P (plot-and-merge) mode
#   2. Merge all subplots into FINAL_DRIVE
#   3. Move the merged file to Ceph (or delete if already there)
#   4. Clear system caches, wipe temp and final drives
#
# One CSV per drive pair is written to CSV_DIR.
# Columns: N, K, temp_drive, final_drive, total_plot_time_s, read_time_s,
#          write_time_s, merge_time_s, total_time_s, total_time_min, peak_memory_mb
#
# Usage: ./k32merge.sh [-t <threads>]
set -euo pipefail


# USER CONFIGURATION

N_VALUES=(2 4 8 16 32 64 128 256)
TEMP_DRIVES=(/nfs_nvme/sfatunmbi/temp/ /ssd-raid0/sfatunmbi/temp/ /data-k/sfatunmbi/temp/)
FINAL_DRIVES=(/data-l/sfatunmbi/plots/ /data-l/sfatunmbi/plots/ /data-l/sfatunmbi/plots/)

# Compute threads
COMPUTE_THREADS=$(nproc)

K=32
CEPH_DIR=/ceph/sfatunmbi/mergedplots
CSV_DIR=/home/sfatunmbi/vaultx/experiments/plots
SUDO_PASS=sfatunmbi

while [[ $# -gt 0 ]]; do
  case "$1" in
    -t|--threads) COMPUTE_THREADS="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: $0 [-t <threads>]"
      echo "  -t  Number of compute threads (default: nproc = $(nproc))"
      exit 0 ;;
    *) echo "Unknown argument: $1" >&2; exit 1 ;;
  esac
done

# PATHS & VALIDATION

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/vaultx"

[[ -x "$BIN" ]] || { echo "Error: vaultx not found or not executable at $BIN" >&2; exit 1; }

if [[ ${#TEMP_DRIVES[@]} -ne ${#FINAL_DRIVES[@]} ]]; then
  echo "Error: TEMP_DRIVES and FINAL_DRIVES must have the same length." >&2
  exit 1
fi

safe_mkdir "$CEPH_DIR" "$CSV_DIR"

echo "=== k32merge configuration ==="
echo "  K              : $K"
echo "  N values       : ${N_VALUES[*]}"
echo "  Threads        : $COMPUTE_THREADS"
echo "  Ceph dir       : $CEPH_DIR"
echo "  CSV dir        : $CSV_DIR"
echo "  Drive pairs    :"
for (( i=0; i<${#TEMP_DRIVES[@]}; i++ )); do
  echo "    [$(( i+1 ))] temp=${TEMP_DRIVES[$i]}  final=${FINAL_DRIVES[$i]}"
done
echo ""

# Extract the first non-empty path component: /data-k/temp/ → data-k
drive_base() {
  echo "$1" | sed 's|^/*||; s|/.*||'
}

drop_caches() {
  echo "$SUDO_PASS" | sudo -S sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null \
    || { echo "  Warning: sudo cache drop failed, falling back to sync." >&2; sync; }
}

safe_mkdir() {
  mkdir -p "$@" 2>/dev/null || echo "$SUDO_PASS" | sudo -S mkdir -p "$@"
}

safe_rm() {
  rm -f "$@" 2>/dev/null || echo "$SUDO_PASS" | sudo -S rm -f "$@" 2>/dev/null || true
}

safe_mv() {
  mv "$@" 2>/dev/null || echo "$SUDO_PASS" | sudo -S mv "$@"
}

# Remove all plot/tmp files
clean_dir() {
  local dir="$1"
  if [[ -d "$dir" ]]; then
    find "$dir" -maxdepth 1 -type f \( -name "*.plot" -o -name "*.tmp" -o -name "*.tmp2" \) -delete 2>/dev/null || \
      echo "$SUDO_PASS" | sudo -S find "$dir" -maxdepth 1 -type f \( -name "*.plot" -o -name "*.tmp" -o -name "*.tmp2" \) -delete 2>/dev/null || true
  fi
}

# Parse a value from log, return default if missing
grep_val() {
  local pattern="$1" log="$2" default="${3:-NA}"
  local val
  val=$(grep -m1 "$pattern" "$log" 2>/dev/null | tail -1) || true
  [[ -n "$val" ]] && echo "$val" || echo "$default"
}

# INIT CSV FILES (one per drive pair, written before experiments start)

for (( di=0; di<${#TEMP_DRIVES[@]}; di++ )); do
  tb=$(drive_base "${TEMP_DRIVES[$di]}")
  fb=$(drive_base "${FINAL_DRIVES[$di]}")
  csv="$CSV_DIR/k32merge_${tb}-${fb}.csv"
  printf "N,K,temp_drive,final_drive,total_plot_time_s,read_time_s,write_time_s,merge_time_s,total_time_s,total_time_min,peak_memory_mb\n" > "$csv"
  echo "  Initialized CSV: $csv"
done

# MAIN LOOP  (outer = N, inner = drive pairs)

for n in "${N_VALUES[@]}"; do
  echo ""
  echo "============================================================"
  echo " N=$n  K=$K subplots"
  echo "============================================================"

  for (( di=0; di<${#TEMP_DRIVES[@]}; di++ )); do
    temp_drive="${TEMP_DRIVES[$di]}"
    final_drive="${FINAL_DRIVES[$di]}"
    tb=$(drive_base "$temp_drive")
    fb=$(drive_base "$final_drive")
    csv="$CSV_DIR/k32merge_${tb}-${fb}.csv"

    echo ""
    echo "  [Pair $(( di+1 ))/${#TEMP_DRIVES[@]}]  temp=$temp_drive  final=$final_drive"

    safe_mkdir "$temp_drive" "$final_drive"
    clean_dir "$temp_drive"
    clean_dir "$final_drive"

    # ----------------------------------------------------------
    # Run vaultx in plot-and-merge mode (-P)
    # -F  : subplots are generated and stored here  (= TEMP_DRIVE)
    # -T  : merged output goes here                 (= FINAL_DRIVE)
    # -g/-j : generation temp dirs (same as -F for in-memory mode)
    # ----------------------------------------------------------
    log=$(mktemp --suffix=".k32merge.log")
    echo "  CMD: $BIN -P -k $K -n $n -g $temp_drive -j $temp_drive -F $temp_drive -T $final_drive -t $COMPUTE_THREADS"

    set +e
    "$BIN" -P -k "$K" -n "$n" \
      -g "$temp_drive" -j "$temp_drive" \
      -F "$temp_drive" -T "$final_drive" \
      -t "$COMPUTE_THREADS" 2>&1 | tee "$log"
    vaultx_exit=${PIPESTATUS[0]}
    set -e

    if [[ $vaultx_exit -ne 0 ]]; then
      echo "  Error: vaultx exited with code $vaultx_exit" >&2
      rm -f "$log"
      continue
    fi

    # Parse timing from log

    # Sum all per-subplot "Total Time: X.XXX seconds" lines
    total_plot_time=$(grep "^Total Time:" "$log" 2>/dev/null \
      | awk '{sum += $3} END {printf "%.3f", sum+0}')

    # Merge section: "Read Time: X.XXs"  (strip trailing 's')
    read_time=$(grep "^Read Time:" "$log" 2>/dev/null | tail -1 \
      | awk '{v=$3; gsub(/s$/,"",v); printf "%.3f", v+0}')
    write_time=$(grep "^Write Time:" "$log" 2>/dev/null | tail -1 \
      | awk '{v=$3; gsub(/s$/,"",v); printf "%.3f", v+0}')
    merge_time=$(grep "^Merge Time:" "$log" 2>/dev/null | tail -1 \
      | awk '{v=$3; gsub(/s$/,"",v); printf "%.3f", v+0}')

    # "Peak Memory Usage: XXXX.XX MB"
    peak_memory=$(grep "^Peak Memory Usage:" "$log" 2>/dev/null | tail -1 \
      | awk '{printf "%.2f", $4+0}')

    # Defaults for any missing fields
    total_plot_time="${total_plot_time:-0.000}"
    read_time="${read_time:-0.000}"
    write_time="${write_time:-0.000}"
    merge_time="${merge_time:-0.000}"
    peak_memory="${peak_memory:-NA}"

    total_time_s=$(awk "BEGIN {printf \"%.3f\", ${total_plot_time}+${merge_time}}")
    total_time_min=$(awk "BEGIN {printf \"%.4f\", ${total_time_s}/60}")

    rm -f "$log"


    # Handle merged file → Ceph
    # Merged file name: merge_<K>_<N>.plot  (vaultx naming convention)

    merged_file=""
    # Prefer exact name; fall back to any suffixed variant
    if [[ -f "$final_drive/merge_${K}_${n}.plot" ]]; then
      merged_file="$final_drive/merge_${K}_${n}.plot"
    else
      merged_file=$(find "$final_drive" -maxdepth 1 -name "merge_${K}_${n}_*.plot" \
        -printf "%T@ %p\n" 2>/dev/null | sort -n | tail -1 | awk '{print $2}') || true
    fi

    if [[ -n "$merged_file" && -f "$merged_file" ]]; then
      merged_name=$(basename "$merged_file")
      if [[ -f "$CEPH_DIR/$merged_name" ]]; then
        echo "  '$merged_name' already in Ceph — deleting local copy."
        safe_rm "$merged_file"
      else
        echo "  Moving '$merged_name' → $CEPH_DIR/"
        safe_mv "$merged_file" "$CEPH_DIR/"
      fi
    else
      echo "  Warning: merged file not found in $final_drive" >&2
    fi


    # Clear system page cache

    drop_caches


    # Wipe temp and final drives (ready for next experiment)

    clean_dir "$temp_drive"
    clean_dir "$final_drive"


    # Append row to CSV

    printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
      "$n" "$K" "$temp_drive" "$final_drive" \
      "$total_plot_time" "$read_time" "$write_time" "$merge_time" \
      "$total_time_s" "$total_time_min" "$peak_memory" \
      >> "$csv"

    echo "  Row written → $csv"
  done
done

echo ""
echo "All experiments complete."
echo "Results: $CSV_DIR"
