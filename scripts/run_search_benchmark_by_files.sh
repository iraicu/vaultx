#!/usr/bin/env bash
set -euo pipefail


# run_search_benchmark_by_files.sh

# General-purpose vaultx search benchmark.  Sweeps -t (io threads,
# multi-file directory) or -r (hash threads, single file).

# Usage:
#   ./scripts/run_search_benchmark_by_files.sh -sweep t
#   ./scripts/run_search_benchmark_by_files.sh -sweep r

N_LOOKUPS=10
DIFFICULTY=3
K_VALUE=32
PREVIOUS_SEARCH=false
KEEP_OPEN=false
SWEEP_O=true
TARGET="./plots/"
BINARY="./vaultx"
OUT_DIR="./data"
LOG_FILE="/home/sfatunmbi/epycbox/search_log.txt"
SUDO_PASS="sfatunmbi"


usage() {
  cat <<'EOF'
Usage: run_search_benchmark_by_files.sh -sweep <mode>

  -sweep t    Sweep -t (io_threads) from 1 to N_cores using TARGET as a
              directory.  Each file in the directory is searched in
              parallel across the t threads.

  -sweep r    Sweep -r (hash_threads) from 1 to N_cores using TARGET as
              a single plot file.  The r threads parallelize the hashing
              step within each bucket.

Edit the EDITABLE CONFIGURATION block at the top of this script to
change N_LOOKUPS, DIFFICULTY, K_VALUE, KEEP_OPEN, SWEEP_O, TARGET, etc.
EOF
}


SWEEP_MODE=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    -sweep)
      [[ $# -lt 2 ]] && { echo "Error: -sweep requires an argument: t or r" >&2; usage; exit 1; }
      SWEEP_MODE="$2"; shift 2 ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

if [[ -z "$SWEEP_MODE" ]]; then
  echo "Error: -sweep t|r is required" >&2
  usage; exit 1
fi

case "$SWEEP_MODE" in
  t|r) ;;
  *) echo "Error: -sweep argument must be 't' or 'r', got '$SWEEP_MODE'" >&2; usage; exit 1 ;;
esac

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

resolve_path() {
  local p="$1"
  if [[ "$p" == /* ]]; then
    echo "$p"
  else
    echo "${ROOT_DIR}/${p#./}"
  fi
}

BINARY="$(resolve_path "$BINARY")"
TARGET="$(resolve_path "$TARGET")"
OUT_DIR="$(resolve_path "$OUT_DIR")"

_log_dir="$(dirname "$LOG_FILE")"
if mkdir -p "$_log_dir" 2>/dev/null; then
  exec > >(tee -a "$LOG_FILE") 2>&1
else
  _fallback_log="${OUT_DIR}/search_log_$(date +%Y%m%d).txt"
  mkdir -p "$OUT_DIR" 2>/dev/null || true
  echo "WARN: Cannot create log directory '$_log_dir'; logging to '$_fallback_log' instead" >&2
  LOG_FILE="$_fallback_log"
  exec > >(tee -a "$LOG_FILE") 2>&1
fi


TIMESTAMP_START=$(date -Iseconds)
echo "========================================================"
echo "  vaultx Search Benchmark  |  sweep: -${SWEEP_MODE}"
echo "  Started: $TIMESTAMP_START"
echo "========================================================"
echo "  Binary:          $BINARY"
echo "  Target:          $TARGET"
echo "  N_LOOKUPS (-S):  $N_LOOKUPS"
echo "  DIFFICULTY (-D): $DIFFICULTY"
echo "  K_VALUE:         $K_VALUE"
echo "  PREVIOUS_SEARCH: $PREVIOUS_SEARCH"
echo "  KEEP_OPEN (-O):  $KEEP_OPEN  (SWEEP_O=$SWEEP_O)"
echo "  OUT_DIR:         $OUT_DIR"
echo "  LOG_FILE:        $LOG_FILE"
echo "========================================================"
echo ""

if [[ ! -x "$BINARY" ]]; then
  echo "Error: vaultx binary not found or not executable at '$BINARY'" >&2
  exit 1
fi

if [[ "$SWEEP_MODE" == "t" ]]; then
  if [[ ! -d "$TARGET" ]]; then
    echo "Error: t-sweep requires a directory.  TARGET='$TARGET' is not a directory." >&2
    exit 1
  fi
  file_count=$(find "$TARGET" -maxdepth 1 -name "k${K_VALUE}-*.plot" | wc -l | tr -d ' ')
  if (( file_count < 1 )); then
    echo "Error: no k${K_VALUE}-*.plot files found in '$TARGET'" >&2
    exit 1
  fi
  file_size_bytes="N/A"
  echo "t-sweep target directory: $TARGET"
  echo "Plot files matching k${K_VALUE}-*.plot: $file_count"
else
  if [[ ! -f "$TARGET" ]]; then
    echo "Error: r-sweep requires a file.  TARGET='$TARGET' is not a file." >&2
    exit 1
  fi
  file_count=1
  file_size_bytes=$(stat -c '%s' "$TARGET" 2>/dev/null || echo "N/A")
  echo "r-sweep target file: $TARGET  ($file_size_bytes bytes)"
fi


core_count=$(nproc --all 2>/dev/null || printf '1')
echo "CPU cores available: $core_count"

raw_thread_vals=()
val=1
while (( val < core_count )); do
  raw_thread_vals+=("$val")
  val=$(( val * 2 ))
done
raw_thread_vals+=("$core_count")

declare -A _seen_t
thread_values=()
for v in "${raw_thread_vals[@]}"; do
  if [[ -z "${_seen_t[$v]:-}" ]]; then
    thread_values+=("$v")
    _seen_t[$v]=1
  fi
done
unset _seen_t

echo "Thread sweep values: ${thread_values[*]}"


if [[ "$SWEEP_O" == "true" ]]; then
  keep_values=(false true)
else
  keep_values=("$KEEP_OPEN")
fi
echo "Keep-open values to test: ${keep_values[*]}"
echo ""

mkdir -p "$OUT_DIR"
RUN_TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT="${OUT_DIR}/search_${SWEEP_MODE}_sweep_${RUN_TIMESTAMP}.csv"

printf "sweep_mode,target,k_value,file_count,file_size_bytes," \
       >> "$OUTPUT"
printf "t,r,keep_open,difficulty,previous_search," \
       >> "$OUTPUT"
printf "lookups,found,not_found,matches," \
       >> "$OUTPUT"
printf "open_close_ms,seek_ms,read_ms,hash_ms," \
       >> "$OUTPUT"
printf "avg_ms_per_lookup,total_ms,total_s,peak_memory_mb\n" \
       >> "$OUTPUT"

echo "Results CSV: $OUTPUT"
echo ""


DROP_WARNED=""
drop_caches() {
  if [[ -w /proc/sys/vm/drop_caches ]]; then
    sync
    echo 3 > /proc/sys/vm/drop_caches
  elif command -v sudo >/dev/null 2>&1; then
    echo "$SUDO_PASS" | sudo -S sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null \
      || { [[ -z "$DROP_WARNED" ]] && echo "WARN: sudo cache drop failed, falling back to sync" >&2 && DROP_WARNED=1; sync; }
  else
    if [[ -z "$DROP_WARNED" ]]; then
      echo "WARN: cannot drop caches (no write access and sudo unavailable); skipping" >&2
      DROP_WARNED=1
    fi
  fi
}


best_avg_ms="999999"
best_avg_label=""
best_total_ms="999999"
best_total_label=""


drop_caches

for thread_val in "${thread_values[@]}"; do
  # Assign to the appropriate flag depending on sweep mode
  if [[ "$SWEEP_MODE" == "t" ]]; then
    t="$thread_val"
    r=1
  else
    t=1
    r="$thread_val"
  fi

  drop_caches

  for keep in "${keep_values[@]}"; do

    # Build vaultx command
    cmd=("$BINARY"
         -S "$N_LOOKUPS"
         -D "$DIFFICULTY"
         -f "$TARGET"
         -t "$t"
         -r "$r"
         -O "$keep")

    # Only add -ps when true (flag alone enables it; --ps=false is default)
    [[ "$PREVIOUS_SEARCH" == "true" ]] && cmd+=(-ps)

    label="${SWEEP_MODE}=${thread_val} keep_open=${keep}"
    echo "------------------------------------------------------------"
    echo "  Run: $label"
    echo "  Cmd: ${cmd[*]}"
    echo "------------------------------------------------------------"

    set +e
    output="$("${cmd[@]}" 2>&1)"
    exit_status=$?
    set -e

    # Echo vaultx output so it goes to terminal and log file via tee
    echo "$output"
    echo ""

    if (( exit_status != 0 )); then
      echo "WARN: vaultx exited with status $exit_status; skipping CSV entry" >&2
      continue
    fi

    # ----------------------------------------------------------
    # Parse TIMING line
    # Format: TIMING open_close_ms seek_ms read_ms hash_ms total_wall_ms avg_per_lookup_ms
    # All values are per-lookup proportional wall-clock averages except
    # total_wall_ms which is the full run wall clock.
    # ----------------------------------------------------------
    timing_line=$(grep '^TIMING' <<<"$output" || true)
    if [[ -z "$timing_line" ]]; then
      echo "WARN: TIMING line not found in output (PREVIOUS_SEARCH=true uses legacy path without TIMING); skipping CSV entry" >&2
      continue
    fi

    open_close_ms=$(awk '{print $2}' <<<"$timing_line")
    seek_ms=$(       awk '{print $3}' <<<"$timing_line")
    read_ms=$(       awk '{print $4}' <<<"$timing_line")
    hash_ms=$(       awk '{print $5}' <<<"$timing_line")
    total_ms=$(      awk '{print $6}' <<<"$timing_line")
    avg_ms=$(        awk '{print $7}' <<<"$timing_line")

    # ----------------------------------------------------------
    # Parse TOTAL line
    # Format (positional after "TOTAL (all lookups)"):
    #   lookups  found  not_found  matches  avg_ms  total_ms
    # We extract all numbers in order using grep -oE.
    # ----------------------------------------------------------
    total_line=$(grep 'TOTAL (all lookups)' <<<"$output" || true)
    if [[ -n "$total_line" ]]; then
      # Extract numbers in document order: int int int int float float
      mapfile -t _nums < <(grep -oE '[0-9]+(\.[0-9]+)?' <<<"$total_line")
      lookups_f="${_nums[0]:-$N_LOOKUPS}"
      found_f="${_nums[1]:-0}"
      not_found_f="${_nums[2]:-0}"
      matches_f="${_nums[3]:-0}"
      # _nums[4] and _nums[5] are avg_ms and total_ms — already from TIMING
    else
      lookups_f="$N_LOOKUPS"
      found_f=0
      not_found_f=0
      matches_f=0
    fi

    # ----------------------------------------------------------
    # Parse Peak Memory Usage line (optional)
    # Format: Peak Memory Usage: <value> MB
    # ----------------------------------------------------------
    peak_mem=$(grep -oP 'Peak Memory Usage:\s+\K[0-9.]+' <<<"$output" || echo "")

    # ----------------------------------------------------------
    # Derive total seconds
    # ----------------------------------------------------------
    total_s=$(awk -v ms="$total_ms" 'BEGIN { printf "%.6f", ms/1000.0 }')

    # ----------------------------------------------------------
    # Write CSV row
    # ----------------------------------------------------------
    printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
      "$SWEEP_MODE" "$TARGET" "$K_VALUE" \
      "$file_count" "$file_size_bytes" \
      "$t" "$r" "$keep" \
      "$DIFFICULTY" "$PREVIOUS_SEARCH" \
      "$lookups_f" "$found_f" "$not_found_f" "$matches_f" \
      "$open_close_ms" "$seek_ms" "$read_ms" "$hash_ms" \
      "$avg_ms" "$total_ms" "$total_s" "$peak_mem" \
      >> "$OUTPUT"

    echo "  -> CSV row written | avg_ms_per_lookup=${avg_ms}  total_ms=${total_ms}  total_s=${total_s}"

    # Track bests
    if awk -v cur="$avg_ms" -v best="$best_avg_ms" 'BEGIN{exit(cur<best?0:1)}'; then
      best_avg_ms="$avg_ms"
      best_avg_label="$label  (avg=${avg_ms} ms/lookup)"
    fi
    if awk -v cur="$total_ms" -v best="$best_total_ms" 'BEGIN{exit(cur<best?0:1)}'; then
      best_total_ms="$total_ms"
      best_total_label="$label  (total=${total_ms} ms)"
    fi

  done
done


# ----------------------------------------------------------------
# Summary
# ----------------------------------------------------------------
echo ""
echo "========================================================"
echo "  Benchmark complete"
echo "  Finished: $(date -Iseconds)"
echo "  Results:  $OUTPUT"
if [[ -n "$best_avg_label" ]]; then
  echo "  Best avg/lookup:  $best_avg_label"
fi
if [[ -n "$best_total_label" ]]; then
  echo "  Best total time:  $best_total_label"
fi
echo "========================================================"
