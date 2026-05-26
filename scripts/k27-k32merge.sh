#!/usr/bin/env bash
# k27-k32merge.sh
#
# For each (K_VALUES[i], N_VALUES[i]) pair, and for each (TEMP_DRIVES[j], FINAL_DRIVES[j]):
#   1. Generate N k-subplots in TEMP_DRIVE using -P (plot-and-merge) mode
#   2. Merge all N subplots into FINAL_DRIVE
#   3. Move the merged file to Ceph (or delete if already there)
#   4. Clear system caches, wipe temp and final drives
#
# K-to-N mapping (one-to-one, index-aligned):
#   K_VALUES=(27 28 29 30 31 32)
#   N_VALUES=(512 256 128 64 32 16)
# Each k-n pair roughly equals the size of one k34 plot.
#
# One CSV per drive pair is written to CSV_DIR.
# Columns: K, N, temp_drive, final_drive, total_plot_time_s, read_time_s,
#          write_time_s, merge_time_s, total_time_s, total_time_min, peak_memory_mb
#
# Usage: ./k27-k32merge.sh [-t <threads>]
set -euo pipefail

# Raise the open-file-descriptor limit. The merge phase opens all N subplot
# files simultaneously (likely via mmap), so large N values exhaust the default
# Ubuntu soft limit of 1024. Try to set to 1M; fall back to 65536.
ulimit -n 1048576 2>/dev/null || ulimit -n 65536 2>/dev/null || true


# USER CONFIGURATION

K_VALUES=(27 28 29 30 31 32)
N_VALUES=(512 256 128 64 32 16)

TEMP_DRIVES=(/nfs_nvme/sfatunmbi/temp/ /ssd-raid0/sfatunmbi/temp/ /data-k/sfatunmbi/temp/)
FINAL_DRIVES=(/data-l/sfatunmbi/plots/ /data-l/sfatunmbi/plots/ /data-l/sfatunmbi/plots/)

# Compute threads (override at runtime: ./k27-k32merge.sh -t 64)
COMPUTE_THREADS=$(nproc)

CEPH_DIR=/ceph/sfatunmbi/mergedplots
CSV_DIR=/home/sfatunmbi/vaultx/experiments/plots
SUDO_PASS=sfatunmbi


# CLI

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

if [[ ${#K_VALUES[@]} -ne ${#N_VALUES[@]} ]]; then
  echo "Error: K_VALUES and N_VALUES must have the same length." >&2
  exit 1
fi

if [[ ${#TEMP_DRIVES[@]} -ne ${#FINAL_DRIVES[@]} ]]; then
  echo "Error: TEMP_DRIVES and FINAL_DRIVES must have the same length." >&2
  exit 1
fi

mkdir -p "$CEPH_DIR" "$CSV_DIR"

echo "=== k27-k32merge configuration ==="
echo "  K → N mapping  :"
for (( ki=0; ki<${#K_VALUES[@]}; ki++ )); do
  echo "    K=${K_VALUES[$ki]} → N=${N_VALUES[$ki]}"
done
echo "  Threads        : $COMPUTE_THREADS"
echo "  Ceph dir       : $CEPH_DIR"
echo "  CSV dir        : $CSV_DIR"
echo "  Drive pairs    :"
for (( i=0; i<${#TEMP_DRIVES[@]}; i++ )); do
  echo "    [$(( i+1 ))] temp=${TEMP_DRIVES[$i]}  final=${FINAL_DRIVES[$i]}"
done
echo ""


# HELPERS

drive_base() {
  echo "$1" | sed 's|^/*||; s|/.*||'
}

drop_caches() {
  echo "$SUDO_PASS" | sudo -S sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null \
    || { echo "  Warning: sudo cache drop failed, falling back to sync." >&2; sync; }
}

clean_dir() {
  local dir="$1"
  if [[ -d "$dir" ]]; then
    find "$dir" -maxdepth 1 -type f \( -name "*.plot" -o -name "*.tmp" -o -name "*.tmp2" \) -delete 2>/dev/null || true
  fi
}


# INIT CSV FILES (one per drive pair)

for (( di=0; di<${#TEMP_DRIVES[@]}; di++ )); do
  tb=$(drive_base "${TEMP_DRIVES[$di]}")
  fb=$(drive_base "${FINAL_DRIVES[$di]}")
  csv="$CSV_DIR/k27-k32merge_${tb}-${fb}.csv"
  printf "K,N,temp_drive,final_drive,total_plot_time_s,read_time_s,write_time_s,merge_time_s,total_time_s,total_time_min,peak_memory_mb\n" > "$csv"
  echo "  Initialized CSV: $csv"
done
echo ""


# MAIN LOOP  (outer = K-N pair, inner = drive pairs)

for (( ki=0; ki<${#K_VALUES[@]}; ki++ )); do
  k="${K_VALUES[$ki]}"
  n="${N_VALUES[$ki]}"

  echo ""
  echo "============================================================"
  echo " K=$k  N=$n subplots"
  echo "============================================================"

  for (( di=0; di<${#TEMP_DRIVES[@]}; di++ )); do
    temp_drive="${TEMP_DRIVES[$di]}"
    final_drive="${FINAL_DRIVES[$di]}"
    tb=$(drive_base "$temp_drive")
    fb=$(drive_base "$final_drive")
    csv="$CSV_DIR/k27-k32merge_${tb}-${fb}.csv"

    echo ""
    echo "  [Pair $(( di+1 ))/${#TEMP_DRIVES[@]}]  temp=$temp_drive  final=$final_drive"

    mkdir -p "$temp_drive" "$final_drive"
    clean_dir "$temp_drive"
    clean_dir "$final_drive"

    # ----------------------------------------------------------
    # Run vaultx in plot-and-merge mode (-P)
    # -F  : subplots are generated and stored here  (= TEMP_DRIVE)
    # -T  : merged output goes here                 (= FINAL_DRIVE)
    # -g/-j : generation temp dirs (same as -F for in-memory mode)
    # No -m : binary determines memory usage automatically
    # ----------------------------------------------------------
    log=$(mktemp --suffix=".k27-k32merge.log")
    echo "  CMD: $BIN -P -k $k -n $n -g $temp_drive -j $temp_drive -F $temp_drive -T $final_drive -t $COMPUTE_THREADS"

    set +e
    "$BIN" -P -k "$k" -n "$n" \
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

    total_plot_time=$(grep "^Total Time:" "$log" 2>/dev/null \
      | awk '{sum += $3} END {printf "%.3f", sum+0}')

    read_time=$(grep "^Read Time:" "$log" 2>/dev/null | tail -1 \
      | awk '{v=$3; gsub(/s$/,"",v); printf "%.3f", v+0}')
    write_time=$(grep "^Write Time:" "$log" 2>/dev/null | tail -1 \
      | awk '{v=$3; gsub(/s$/,"",v); printf "%.3f", v+0}')
    merge_time=$(grep "^Merge Time:" "$log" 2>/dev/null | tail -1 \
      | awk '{v=$3; gsub(/s$/,"",v); printf "%.3f", v+0}')

    peak_memory=$(grep "^Peak Memory Usage:" "$log" 2>/dev/null | tail -1 \
      | awk '{printf "%.2f", $4+0}')

    total_plot_time="${total_plot_time:-0.000}"
    read_time="${read_time:-0.000}"
    write_time="${write_time:-0.000}"
    merge_time="${merge_time:-0.000}"
    peak_memory="${peak_memory:-NA}"

    total_time_s=$(awk "BEGIN {printf \"%.3f\", ${total_plot_time}+${merge_time}}")
    total_time_min=$(awk "BEGIN {printf \"%.4f\", ${total_time_s}/60}")

    rm -f "$log"

    # Handle merged file → Ceph
    # Merged file name: merge_<K>_<N>.plot

    merged_file=""
    if [[ -f "$final_drive/merge_${k}_${n}.plot" ]]; then
      merged_file="$final_drive/merge_${k}_${n}.plot"
    else
      merged_file=$(find "$final_drive" -maxdepth 1 -name "merge_${k}_${n}_*.plot" \
        -printf "%T@ %p\n" 2>/dev/null | sort -n | tail -1 | awk '{print $2}') || true
    fi

    if [[ -n "$merged_file" && -f "$merged_file" ]]; then
      merged_name=$(basename "$merged_file")
      if [[ -f "$CEPH_DIR/$merged_name" ]]; then
        echo "  '$merged_name' already in Ceph — deleting local copy."
        rm -f "$merged_file"
      else
        echo "  Moving '$merged_name' → $CEPH_DIR/"
        mv "$merged_file" "$CEPH_DIR/"
      fi
    else
      echo "  Warning: merged file not found in $final_drive" >&2
    fi


    # Clear system page cache

    drop_caches


    # Wipe temp and final drives

    clean_dir "$temp_drive"
    clean_dir "$final_drive"

    # Append row to CSV
    printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
      "$k" "$n" "$temp_drive" "$final_drive" \
      "$total_plot_time" "$read_time" "$write_time" "$merge_time" \
      "$total_time_s" "$total_time_min" "$peak_memory" \
      >> "$csv"

    echo "  Row written → $csv"
  done
done

echo ""
echo "All experiments complete."
echo "Results: $CSV_DIR"
