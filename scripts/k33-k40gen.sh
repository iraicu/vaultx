#!/usr/bin/env bash
# k33-k40gen.sh
#
# Rebuild vaultx with NONCE_SIZE=5 RECORD_SIZE=16, then for each k in 33..40
# and for each (TEMP_DRIVES[i], FINAL_DRIVES[i]) pair:
#   1. Generate a single k-plot (no merging)
#   2. Move the plot to Ceph /ceph/sfatunmbi/singleplots
#   3. Clear system caches, wipe temp and final drives
#
# One CSV per drive pair is written to CSV_DIR.
# Columns: K, temp_drive, final_drive, total_time_s, total_time_min, peak_memory_mb
#
# Usage: ./k33-k40gen.sh [-t <threads>]
set -euo pipefail


# USER CONFIGURATION

K_VALUES=(33 34 35 36 37 38 39 40)
TEMP_DRIVES=(/nfs_nvme/sfatunmbi/temp/ /ssd-raid0/sfatunmbi/temp/ /data-k/sfatunmbi/temp/)
FINAL_DRIVES=(/data-l/sfatunmbi/plots/ /data-l/sfatunmbi/plots/ /data-l/sfatunmbi/plots/)

# Compute threads (override at runtime: ./k33-k40gen.sh -t 64)
COMPUTE_THREADS=$(nproc)

CEPH_DIR=/ceph/sfatunmbi/singleplots
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

if [[ ${#TEMP_DRIVES[@]} -ne ${#FINAL_DRIVES[@]} ]]; then
  echo "Error: TEMP_DRIVES and FINAL_DRIVES must have the same length." >&2
  exit 1
fi

mkdir -p "$CEPH_DIR" "$CSV_DIR"

echo "=== k33-k40gen configuration ==="
echo "  K values       : ${K_VALUES[*]}"
echo "  Threads        : $COMPUTE_THREADS"
echo "  Ceph dir       : $CEPH_DIR"
echo "  CSV dir        : $CSV_DIR"
echo "  Drive pairs    :"
for (( i=0; i<${#TEMP_DRIVES[@]}; i++ )); do
  echo "    [$(( i+1 ))] temp=${TEMP_DRIVES[$i]}  final=${FINAL_DRIVES[$i]}"
done
echo ""


# REBUILD for NONCE_SIZE=5, RECORD_SIZE=16

echo "--- Rebuilding vaultx (NONCE_SIZE=5 RECORD_SIZE=16) ---"
cd "$ROOT_DIR"
make clean
make vaultx_x86_c NONCE_SIZE=5 RECORD_SIZE=16
echo "--- Build complete ---"
echo ""

[[ -x "$BIN" ]] || { echo "Error: vaultx not found or not executable at $BIN after build." >&2; exit 1; }


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
  csv="$CSV_DIR/k33-k40gen_${tb}-${fb}.csv"
  printf "K,temp_drive,final_drive,total_time_s,total_time_min,peak_memory_mb\n" > "$csv"
  echo "  Initialized CSV: $csv"
done
echo ""


# MAIN LOOP  (outer = K, inner = drive pairs)

for k in "${K_VALUES[@]}"; do
  echo ""
  echo "============================================================"
  echo " K=$k single plot generation"
  echo "============================================================"

  for (( di=0; di<${#TEMP_DRIVES[@]}; di++ )); do
    temp_drive="${TEMP_DRIVES[$di]}"
    final_drive="${FINAL_DRIVES[$di]}"
    tb=$(drive_base "$temp_drive")
    fb=$(drive_base "$final_drive")
    csv="$CSV_DIR/k33-k40gen_${tb}-${fb}.csv"

    echo ""
    echo "  [Pair $(( di+1 ))/${#TEMP_DRIVES[@]}]  temp=$temp_drive  final=$final_drive"

    mkdir -p "$temp_drive" "$final_drive"
    clean_dir "$temp_drive"
    clean_dir "$final_drive"


    # Run vaultx single-plot generation (no merge, no -m flag)
    # -g/-j : temp directories for intermediate files
    # -f    : final output directory for the .plot file
    # No -m : binary uses all available memory

    log=$(mktemp --suffix=".k33-k40gen.log")
    echo "  CMD: $BIN -k $k -g $temp_drive -j $temp_drive -f $final_drive -t $COMPUTE_THREADS"

    set +e
    "$BIN" -k "$k" \
      -g "$temp_drive" -j "$temp_drive" \
      -f "$final_drive" \
      -t "$COMPUTE_THREADS" 2>&1 | tee "$log"
    vaultx_exit=${PIPESTATUS[0]}
    set -e

    if [[ $vaultx_exit -ne 0 ]]; then
      echo "  Error: vaultx exited with code $vaultx_exit" >&2
      rm -f "$log"
      continue
    fi

    # Parse timing from log

    total_time_s=$(grep "^Total Time:" "$log" 2>/dev/null | tail -1 \
      | awk '{printf "%.3f", $3+0}')
    peak_memory=$(grep "^Peak Memory Usage:" "$log" 2>/dev/null | tail -1 \
      | awk '{printf "%.2f", $4+0}')

    total_time_s="${total_time_s:-NA}"
    peak_memory="${peak_memory:-NA}"

    if [[ "$total_time_s" != "NA" ]]; then
      total_time_min=$(awk "BEGIN {printf \"%.4f\", ${total_time_s}/60}")
    else
      total_time_min="NA"
    fi

    rm -f "$log"


    # Handle plot file → Ceph
    # The plot file is named k<K>-<PLOT_ID>.plot in final_drive (-f)

    plot_file=$(find "$final_drive" -maxdepth 1 -name "k${k}-*.plot" \
      -printf "%T@ %p\n" 2>/dev/null | sort -n | tail -1 | awk '{print $2}') || true

    if [[ -n "$plot_file" && -f "$plot_file" ]]; then
      plot_name=$(basename "$plot_file")
      if [[ -f "$CEPH_DIR/$plot_name" ]]; then
        echo "  '$plot_name' already in Ceph — deleting local copy."
        rm -f "$plot_file"
      else
        echo "  Moving '$plot_name' → $CEPH_DIR/"
        mv "$plot_file" "$CEPH_DIR/"
      fi
    else
      echo "  Warning: plot file k${k}-*.plot not found in $final_drive" >&2
    fi


    # Clear system page cache

    drop_caches


    # Wipe temp and final drives

    clean_dir "$temp_drive"
    clean_dir "$final_drive"


    # Append row to CSV

    printf "%s,%s,%s,%s,%s,%s\n" \
      "$k" "$temp_drive" "$final_drive" \
      "$total_time_s" "$total_time_min" "$peak_memory" \
      >> "$csv"

    echo "  Row written → $csv"
  done
done

echo ""
echo "All experiments complete."
echo "Results: $CSV_DIR"
