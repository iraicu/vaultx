#!/usr/bin/env bash
# run_merge_benchmark.sh
#
# Benchmarks vaultx -P merge by sweeping K, N, and B values.
# Assumes source plot files (k{K}-*.plot) already exist in each TEMP_DRIVE.
# One CSV per K value is written to EXPERIMENTS_DIR.
#
# CSV columns:
#   K, N, B, approach, temp_drive, final_drive,
#   total_time_s, total_time_min,
#   read_time_s, write_time_s, compute_time_s,
#   avg_throughput_mb_s, min_throughput_mb_s, max_throughput_mb_s,
#   peak_memory_mb
#
# After each merge the output file is moved to CEPH_DIR.
# If an identical filename already exists in CEPH_DIR, the new file is deleted.
# Source plots in TEMP_DRIVE are never removed.
# Page cache is dropped after every run.
#
# Usage: ./run_merge_benchmark.sh
set -euo pipefail


K_VALUES=(32)
N_VALUES=(2 4 8 16 32 64)
B_VALUES=(256 512 1024 2048 4096)

# pipelined | serial | tasks
APPROACH=pipelined

COMPUTE_THREADS=$(nproc)
MERGE_IO_THREADS=(1)

# Paired 1-to-1 with FINAL_DRIVES.
TEMP_DRIVES=(
  "/data-l/iraicu/tmp/"
)
FINAL_DRIVES=(
  "/data-r/iraicu/vaults/"
)

CEPH_DIR="/ceph/sfatunmbi/mergedplots"
EXPERIMENTS_DIR="${HOME}/vaultx/newexperiments/$(hostname)"
SUDO_PASS="sfatunmbi"


ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/vaultx"

[[ -x "$BIN" ]] || { echo "Error: vaultx not found or not executable at $BIN" >&2; exit 1; }

if [[ ${#TEMP_DRIVES[@]} -ne ${#FINAL_DRIVES[@]} ]]; then
  echo "Error: TEMP_DRIVES and FINAL_DRIVES must have the same length." >&2
  exit 1
fi

case "$APPROACH" in
  pipelined|serial|tasks) ;;
  *) echo "Error: APPROACH must be pipelined, serial, or tasks. Got: $APPROACH" >&2; exit 1 ;;
esac

if [[ ${#K_VALUES[@]} -eq 0 || ${#N_VALUES[@]} -eq 0 || ${#B_VALUES[@]} -eq 0 ]]; then
  echo "Error: K_VALUES, N_VALUES, and B_VALUES must each contain at least one entry." >&2
  exit 1
fi

safe_mkdir "$CEPH_DIR" "$EXPERIMENTS_DIR"

# Check for /usr/bin/time -v support
HAS_TIME_V=false
if /usr/bin/time -v true 2>/dev/null; then
  HAS_TIME_V=true
fi

# ---- Helpers ----

drop_caches() {
  echo "$SUDO_PASS" | sudo -S sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null \
    || { echo "  Warning: cache drop failed, falling back to sync." >&2; sync; }
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

drive_id() {
  echo "$1" | sed 's|^/*||; s|/.*||'
}

# Parse a vaultx merge log and emit one CSV data row.
# Args: log time_log k n b approach temp_drive final_drive
parse_log() {
  local log="$1" time_log="$2"
  local k="$3" n="$4" b="$5" approach="$6" temp_drive="$7" final_drive="$8"

  # --- Total merge time from: [X.XXs] Completed merging ...
  local total_time
  total_time=$(grep -m1 "Completed merging" "$log" 2>/dev/null \
    | grep -oP '(?<=\[)[\d.]+(?=s\])' | head -1 || true)
  [[ -z "$total_time" ]] && total_time="NA"

  # --- Phase times from completion summary
  local read_time write_time compute_time
  read_time=$(grep -m1 "^Read Time:" "$log" 2>/dev/null \
    | grep -oP '[\d.]+(?=s)' | head -1 || true)
  write_time=$(grep -m1 "^Write Time:" "$log" 2>/dev/null \
    | grep -oP '[\d.]+(?=s)' | head -1 || true)
  compute_time=$(grep -m1 "^Merge Time:" "$log" 2>/dev/null \
    | grep -oP '[\d.]+(?=s)' | head -1 || true)
  [[ -z "$read_time"    ]] && read_time="NA"
  [[ -z "$write_time"   ]] && write_time="NA"
  [[ -z "$compute_time" ]] && compute_time="NA"

  # --- Per-batch throughput from progress lines (pipelined/serial only).
  # Tasks approach uses a different progress format without Throughput:.
  local min_tput max_tput avg_tput
  local tput_values
  tput_values=$(grep -oP '(?<=Throughput: )[\d.]+' "$log" 2>/dev/null || true)

  if [[ -n "$tput_values" ]]; then
    min_tput=$(echo "$tput_values" | sort -n | head -1)
    max_tput=$(echo "$tput_values" | sort -n | tail -1)
    avg_tput=$(echo "$tput_values" \
      | awk '{s += $1; n++} END { if (n > 0) printf "%.2f", s/n; else print "NA" }')
  else
    # Fall back to average throughput from the completion summary
    local comp_tput
    comp_tput=$(grep -m1 "^Avg Throughput:" "$log" 2>/dev/null \
      | grep -oP '[\d.]+(?= MB/s)' | head -1 || true)
    min_tput="${comp_tput:-NA}"
    max_tput="${comp_tput:-NA}"
    avg_tput="${comp_tput:-NA}"
  fi

  # --- Peak memory from /usr/bin/time -v (kbytes → MB); fall back to 2×B estimate
  local peak_mem_mb
  if [[ -s "$time_log" ]]; then
    local peak_kb
    peak_kb=$(grep "Maximum resident set size" "$time_log" 2>/dev/null \
      | awk '{print $NF}' || true)
    if [[ -n "$peak_kb" && "$peak_kb" -gt 0 ]]; then
      peak_mem_mb=$(awk "BEGIN { printf \"%.1f\", ${peak_kb} / 1024 }")
    else
      peak_mem_mb=$(awk "BEGIN { printf \"%.0f\", 2 * ${b} }")
    fi
  else
    peak_mem_mb=$(awk "BEGIN { printf \"%.0f\", 2 * ${b} }")
  fi

  # --- Also try parsing "Peak Memory Usage:" line added by merge.c
  local logged_peak
  logged_peak=$(grep -m1 "^Peak Memory Usage:" "$log" 2>/dev/null \
    | grep -oP '[\d.]+(?= MB)' | head -1 || true)
  [[ -n "$logged_peak" ]] && peak_mem_mb="$logged_peak"

  # --- Convert total time to minutes
  local total_time_min
  if [[ "$total_time" != "NA" ]]; then
    total_time_min=$(awk "BEGIN { printf \"%.4f\", ${total_time} / 60 }")
  else
    total_time_min="NA"
  fi

  printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
    "$k" "$n" "$b" "$approach" "$temp_drive" "$final_drive" \
    "$total_time" "$total_time_min" \
    "$read_time" "$write_time" "$compute_time" \
    "$avg_tput" "$min_tput" "$max_tput" \
    "$peak_mem_mb"
}

CSV_HEADER="K,N,B,approach,temp_drive,final_drive,total_time_s,total_time_min,read_time_s,write_time_s,compute_time_s,avg_throughput_mb_s,min_throughput_mb_s,max_throughput_mb_s,peak_memory_mb"

total_experiments=$(( ${#K_VALUES[@]} * ${#N_VALUES[@]} * ${#B_VALUES[@]} * ${#TEMP_DRIVES[@]} ))
current_exp=0

echo "=== run_merge_benchmark ==="
echo "  K values    : ${K_VALUES[*]}"
echo "  N values    : ${N_VALUES[*]}"
echo "  B values    : ${B_VALUES[*]} MB"
echo "  Approach    : $APPROACH"
echo "  Threads     : compute=$COMPUTE_THREADS  io=$MERGE_IO_THREADS"
echo "  Ceph dir    : $CEPH_DIR"
echo "  Results dir : $EXPERIMENTS_DIR"
echo "  Total runs  : $total_experiments"
echo ""

# MAIN LOOP: outer = K (one CSV per K), inner = N × B × drive pair

for k in "${K_VALUES[@]}"; do

  csv="${EXPERIMENTS_DIR}/merge_k${k}.csv"
  printf "%s\n" "$CSV_HEADER" > "$csv"
  echo "Initialized CSV: $csv"
  echo ""
  echo "============================================================"
  echo " K=$k | Approach=$APPROACH"
  echo "============================================================"

  for n in "${N_VALUES[@]}"; do
    for b in "${B_VALUES[@]}"; do
      for (( di=0; di<${#TEMP_DRIVES[@]}; di++ )); do

        temp_drive="${TEMP_DRIVES[$di]}"
        final_drive="${FINAL_DRIVES[$di]}"
        (( current_exp++ )) || true

        echo ""
        echo "  [${current_exp}/${total_experiments}]  K=$k  N=$n  B=${b}MB  Approach=$APPROACH"
        echo "   temp=$temp_drive  →  final=$final_drive"

        plot_count=0
        if [[ -d "$temp_drive" ]]; then
          plot_count=$(find "$temp_drive" -maxdepth 1 -name "k${k}-*.plot" 2>/dev/null | wc -l)
        fi

        if (( plot_count < n )); then
          echo "  SKIP: found $plot_count k${k} plots in $temp_drive, need $n" >&2
          printf "%s\n" \
            "${k},${n},${b},${APPROACH},${temp_drive},${final_drive},SKIP,SKIP,SKIP,SKIP,SKIP,SKIP,SKIP,SKIP,SKIP" \
            >> "$csv"
          continue
        fi

        safe_mkdir "$final_drive"
        log=$(mktemp --suffix=".merge.log")
        time_log=$(mktemp --suffix=".time.log")

        drop_caches

        set +e
        if [[ "$HAS_TIME_V" == true ]]; then
          {
            /usr/bin/time -v "$BIN" \
              -P merge \
              -k  "$k" \
              -n  "$n" \
              -F  "$temp_drive" \
              -T  "$final_drive" \
              -t  "$COMPUTE_THREADS" \
              -mt "$MERGE_IO_THREADS" \
              -A  "$APPROACH" \
              -B  "$b" \
              2>&1
          } 2>"$time_log" | tee "$log"
        else
          "$BIN" \
            -P merge \
            -k  "$k" \
            -n  "$n" \
            -F  "$temp_drive" \
            -T  "$final_drive" \
            -t  "$COMPUTE_THREADS" \
            -mt "$MERGE_IO_THREADS" \
            -A  "$APPROACH" \
            -B  "$b" \
            2>&1 | tee "$log"
        fi
        vaultx_exit="${PIPESTATUS[0]}"
        set -e

        if [[ "$vaultx_exit" -ne 0 ]]; then
          echo "  Error: vaultx exited with code $vaultx_exit" >&2
          printf "%s\n" \
            "${k},${n},${b},${APPROACH},${temp_drive},${final_drive},ERROR,ERROR,ERROR,ERROR,ERROR,ERROR,ERROR,ERROR,ERROR" \
            >> "$csv"
          rm -f "$log" "$time_log"
          drop_caches
          continue
        fi

        parse_log "$log" "$time_log" \
          "$k" "$n" "$b" "$APPROACH" "$temp_drive" "$final_drive" \
          >> "$csv"
        echo "  Row written → $csv"

        rm -f "$log" "$time_log"

        merged_file=""
        if [[ -f "${final_drive}/merge_${k}_${n}.plot" ]]; then
          merged_file="${final_drive}/merge_${k}_${n}.plot"
        else
          # vaultx may append _1, _2, … if the plain name existed; take the newest
          merged_file=$(find "$final_drive" -maxdepth 1 \
            -name "merge_${k}_${n}*.plot" \
            -printf "%T@ %p\n" 2>/dev/null \
            | sort -n | tail -1 | awk '{print $2}') || true
        fi

        if [[ -n "$merged_file" && -f "$merged_file" ]]; then
          merged_name=$(basename "$merged_file")
          if [[ -f "${CEPH_DIR}/${merged_name}" ]]; then
            echo "  '$merged_name' already in Ceph — deleting local copy."
            safe_rm "$merged_file"
          else
            echo "  Moving '$merged_name' → $CEPH_DIR/"
            safe_mv "$merged_file" "$CEPH_DIR/"
          fi
        else
          echo "  Warning: no merged file found in $final_drive" >&2
        fi

        drop_caches

      done  # drive pairs
    done    # B values
  done      # N values

  echo ""
  echo "K=$k complete. Results: $csv"

done  # K values

echo ""
echo "All experiments complete."
echo "Results: $EXPERIMENTS_DIR"
