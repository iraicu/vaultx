#!/usr/bin/env bash
set -euo pipefail

# Benchmark helper for VAULTX search (-S batch mode).
# Sweeps thread counts for -t (I/O) and -r (hash/record) plus -O keep-open flag
# and writes a CSV of summary metrics to ./data/ by default.

# How to run
# chmod +x <script name>
# Example # Run benchmarks (defaults: 1000 lookups, 3-byte difficulty, ./plots target)
# ./scripts/run_search_benchmark.sh -l 1500 -d 4 -f ./plots/ -o ./data/my_run.csv -b ./vaultx
# Plot results
# ./scripts/plot_search_results.py ./data/my_run.csv --title "vaultx search benchmark"

usage() {
  cat <<'EOF'
Run randomized search benchmarks and save results to CSV.

Options:
  -l, --lookups NUM       Number of random lookups to perform (default: 1000)
  -d, --difficulty NUM    Prefix length in bytes for each lookup (default: 3)
  -f, --file PATH         Plot file or directory to search (default: ./plots/)
  -o, --output FILE       CSV output path (default: ./data/search_benchmark_<timestamp>.csv)
  -b, --binary PATH       vaultx binary to run (default: ./vaultx)
  -T, --threads LIST      Space/comma separated thread counts to test (default: "1 2 4 8 16 <ncores>")
  -h, --help              Show this message

Environment overrides:
  LOOKUPS, DIFFICULTY, TARGET, OUTPUT, VAULTX_BIN, THREAD_VALUES

The script parses the "SUM" line from the program's batch search summary:
  Avg Time/Lookup (ms)  -> avg_ms_per_lookup
  Total Time (ms)       -> total_ms
It works whether -f points to a single file or a directory of plots.
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_BIN="${VAULTX_BIN:-${ROOT_DIR}/vaultx}"
DEFAULT_LOOKUPS="${LOOKUPS:-1000}"
DEFAULT_DIFFICULTY="${DIFFICULTY:-3}"
DEFAULT_TARGET="${TARGET:-${ROOT_DIR}/plots/}"
DEFAULT_OUTPUT="${OUTPUT:-${ROOT_DIR}/data/search_benchmark_$(date +%Y%m%d_%H%M%S).csv}"
THREAD_SPEC="${THREAD_VALUES:-}"  # optional env override

LOOKUPS_VAL="$DEFAULT_LOOKUPS"
DIFFICULTY_VAL="$DEFAULT_DIFFICULTY"
TARGET_PATH="$DEFAULT_TARGET"
OUTPUT_PATH="$DEFAULT_OUTPUT"
BINARY_PATH="$DEFAULT_BIN"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -l|--lookups)
      LOOKUPS_VAL="$2"; shift 2;;
    -d|--difficulty)
      DIFFICULTY_VAL="$2"; shift 2;;
    -f|--file)
      TARGET_PATH="$2"; shift 2;;
    -o|--output)
      OUTPUT_PATH="$2"; shift 2;;
    -b|--binary)
      BINARY_PATH="$2"; shift 2;;
    -T|--threads)
      THREAD_SPEC="$2"; shift 2;;
    -h|--help)
      usage; exit 0;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1;;
  esac
done

if [[ ! -x "$BINARY_PATH" ]]; then
  echo "Error: vaultx binary not found or not executable at '$BINARY_PATH'" >&2
  exit 1
fi

if [[ ! -d "$TARGET_PATH" && ! -f "$TARGET_PATH" ]]; then
  echo "Error: target '$TARGET_PATH' is not a file or directory" >&2
  exit 1
fi

# Discover core count and build thread list (deduplicated, >=1).
core_count=$(nproc --all 2>/dev/null || printf '1')
default_threads="1 2 4 8 16 ${core_count}"
if [[ -n "$THREAD_SPEC" ]]; then
  THREAD_SPEC=${THREAD_SPEC//,/ }  # allow comma separated
else
  THREAD_SPEC="$default_threads"
fi

declare -A seen
thread_values=()
for val in $THREAD_SPEC; do
  if [[ "$val" =~ ^[0-9]+$ ]] && (( val > 0 )); then
    if [[ -z "${seen[$val]:-}" ]]; then
      thread_values+=("$val")
      seen[$val]=1
    fi
  fi
done

if [[ ${#thread_values[@]} -eq 0 ]]; then
  echo "Error: no valid thread counts resolved from '$THREAD_SPEC'" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUTPUT_PATH")"

printf "timestamp,binary,file,t,r,keep_open,lookups,difficulty,found,not_found,matches,avg_ms_per_lookup,total_ms,total_s\n" > "$OUTPUT_PATH"

best_avg_ms=999999
best_avg_cmd=""
best_total_s=999999
best_total_cmd=""

keep_values=(false true)

echo "Running benchmarks with t/r in: ${thread_values[*]} (cores detected: ${core_count}), keep_open in: ${keep_values[*]}" >&2

for t in "${thread_values[@]}"; do
  for r in "${thread_values[@]}"; do
    for keep in "${keep_values[@]}"; do
      cmd=("$BINARY_PATH" -S "$LOOKUPS_VAL" -D "$DIFFICULTY_VAL" -f "$TARGET_PATH" -t "$t" -r "$r" -O "$keep" -b true)
      echo "--> ${cmd[*]}" >&2

      set +e
      output="$(${cmd[@]} 2>&1)"
      status=$?
      set -e

      if (( status != 0 )); then
        echo "WARN: command failed (exit ${status}); skipping entry" >&2
        echo "$output" >&2
        continue
      fi

      sum_line=$(grep '^SUM' <<<"$output" || true)
      if [[ -z "$sum_line" ]]; then
        echo "WARN: could not find SUM line in output; skipping entry" >&2
        echo "$output" >&2
        continue
      fi

      lookups_field=$(awk '{print $(NF-5)}' <<<"$sum_line")
      found_field=$(awk '{print $(NF-4)}' <<<"$sum_line")
      not_found_field=$(awk '{print $(NF-3)}' <<<"$sum_line")
      matches_field=$(awk '{print $(NF-2)}' <<<"$sum_line")
      avg_ms=$(awk '{print $(NF-1)}' <<<"$sum_line")
      total_ms=$(awk '{print $NF}' <<<"$sum_line")

      timestamp=$(date -Iseconds)
      total_s=$(awk -v ms="$total_ms" 'BEGIN { printf "%.6f", ms/1000.0 }')
      printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
        "$timestamp" "$BINARY_PATH" "$TARGET_PATH" "$t" "$r" "$keep" \
        "$lookups_field" "$DIFFICULTY_VAL" "$found_field" "$not_found_field" \
        "$matches_field" "$avg_ms" "$total_ms" "$total_s" >> "$OUTPUT_PATH"

      cmd_repr="$BINARY_PATH -S $LOOKUPS_VAL -D $DIFFICULTY_VAL -f $TARGET_PATH -t $t -r $r -O $keep"
      if awk -v avg="$avg_ms" -v best="$best_avg_ms" 'BEGIN { exit (avg < best ? 0 : 1) }'; then
        best_avg_ms="$avg_ms"
        best_avg_cmd="$cmd_repr"
      fi
      if awk -v ts="$total_s" -v best="$best_total_s" 'BEGIN { exit (ts < best ? 0 : 1) }'; then
        best_total_s="$total_s"
        best_total_cmd="$cmd_repr"
      fi
    done
  done
done

echo "Benchmark complete. Results saved to $OUTPUT_PATH" >&2
if [[ -n "$best_avg_cmd" ]]; then
  echo "Best avg lookup time: ${best_avg_ms} ms via: $best_avg_cmd" >&2
fi
if [[ -n "$best_total_cmd" ]]; then
  echo "Best total time: ${best_total_s} s via: $best_total_cmd" >&2
fi
