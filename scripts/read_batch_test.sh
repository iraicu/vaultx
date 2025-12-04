#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# VaultX Read-Batch Test Script
# - Memory fixed at 24 GB (24576 MB)
# - Varies the -R (Read) parameter across a set of values
# - Uses repository-local directories for plots/tmps/data and writes per-run logs and a results CSV

# Determine repository root (script is expected at <repo>/scripts/read_batch_test.sh)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLOTS_DIR="$REPO_ROOT/plots"
LOG_DIR="$REPO_ROOT/tmps"
DATA_DIR="$REPO_ROOT/data"

mkdir -p "$PLOTS_DIR" "$LOG_DIR" "$DATA_DIR"

# Fixed memory value in MB (24 GB)
MEMORY=16384

# Read values to test (the -R flag stands for Read)
READ_VALUES=(64 128 256 512 1024)

# Fixed parameters
K=32
MATCH_FACTOR=0.83706

echo "Repo root: $REPO_ROOT"
echo "Testing VaultX K=$K with match_factor=$MATCH_FACTOR"
echo "Memory fixed: ${MEMORY} MB"
echo "Read values: ${READ_VALUES[*]}"
echo ""


RESULTS_FILE="$DATA_DIR/read_batch_test_results.csv"
echo "read,mem_mb,total_time,throughput_mh,io_throughput,storage_efficiency" > "$RESULTS_FILE"

# Build the binary once (log output)
BUILD_LOG="$LOG_DIR/build_$(date +%Y%m%d-%H%M%S).log"
echo "Running make (clean + build). Build log: $BUILD_LOG"
(
  cd "$REPO_ROOT"
  make clean
  make vaultx_x86_c NONCE_SIZE=4 RECORD_SIZE=16
) 2>&1 | tee "$BUILD_LOG"

# Locate vaultx binary: prefer repo-local binary, fallback to PATH
if [ -x "$REPO_ROOT/vaultx" ]; then
  VAULTX_BIN="$REPO_ROOT/vaultx"
else
  VAULTX_BIN="$(command -v vaultx || true)"
fi

if [ -z "$VAULTX_BIN" ]; then
  echo "ERROR: vaultx binary not found in repo root or PATH. Aborting." | tee -a "$LOG_DIR/error_$(date +%Y%m%d-%H%M%S).log"
  exit 1
fi

echo "Using vaultx: $VAULTX_BIN"

for read in "${READ_VALUES[@]}"; do
    echo "=== Testing Read: $read (Memory: $MEMORY MB) ==="
    RUN_LOG="$LOG_DIR/read_${read}_$(date +%Y%m%d-%H%M%S).log"
    echo "Run log: $RUN_LOG"

    # Safe cleanup: remove files inside plots directory but never remove directories outside repo
    if [ -d "$PLOTS_DIR" ]; then
        find "$PLOTS_DIR" -mindepth 1 -maxdepth 1 -exec rm -rf {} + || true
    fi

    # Run VaultX and capture stdout+stderr to per-run log
    echo "Running: $VAULTX_BIN -a for -K $K -m $MEMORY -M $MATCH_FACTOR -R $read -f $PLOTS_DIR -g $LOG_DIR -j $PLOTS_DIR -t 128 -x true" | tee -a "$RUN_LOG"
    "$VAULTX_BIN" -a for -K "$K" -m "$MEMORY" -M "$MATCH_FACTOR" -R "$read" -f "$PLOTS_DIR" -g "$LOG_DIR" -j "$PLOTS_DIR" -t 128 -x true 2>&1 | tee -a "$RUN_LOG"
    rc=${PIPESTATUS[0]}

    if [ "$rc" -ne 0 ]; then
        echo "  FAILED - VaultX error (exit $rc)" | tee -a "$RUN_LOG"
        echo "$read,$MEMORY,FAILED,FAILED,FAILED,FAILED" >> "$RESULTS_FILE"
        echo "" >> "$RUN_LOG"
        continue
    fi

    # Attempt to extract CSV-style metrics. Prefer lines from run log, fallback to CSV files in plots dir.
    csv_line=""
    csv_line="$(grep -E '^[^,]+(,.*){4,}' "$RUN_LOG" | tail -n 1 || true)"

    if [ -z "$csv_line" ]; then
        csv_file="$(ls -1t "$PLOTS_DIR"/*.csv 2>/dev/null | head -n1 || true)"
        if [ -n "$csv_file" ]; then
            csv_line="$(tail -n 1 "$csv_file" || true)"
        fi
    fi

    if [ -n "$csv_line" ]; then
        total_time="$(echo "$csv_line" | awk -F, '{print $(NF-2)}' | tr -d '[:space:]' )"
        throughput="$(echo "$csv_line" | awk -F, '{print $(NF-11)}' | tr -d '[:space:]' )"
        io_throughput="$(echo "$csv_line" | awk -F, '{print $(NF-8)}' | tr -d '[:space:]' )"
        storage_eff_raw="$(echo "$csv_line" | awk -F, '{print $(NF)}' | tr -d '[:space:]' )"
        storage_eff="${storage_eff_raw//%/}"

        [[ "$total_time" =~ [0-9] ]] || total_time="UNKNOWN"
        [[ "$throughput" =~ [0-9] ]] || throughput="UNKNOWN"
        [[ "$io_throughput" =~ [0-9] ]] || io_throughput="UNKNOWN"
        [[ "$storage_eff" =~ [0-9] ]] || storage_eff="UNKNOWN"

        echo "  Time: ${total_time}s, Throughput: ${throughput} MH/s, Storage: ${storage_eff}%" | tee -a "$RUN_LOG"
        echo "$read,$MEMORY,$total_time,$throughput,$io_throughput,$storage_eff" >> "$RESULTS_FILE"
    else
        echo "  FAILED - No CSV output found" | tee -a "$RUN_LOG"
        echo "$read,$MEMORY,FAILED,FAILED,FAILED,FAILED" >> "$RESULTS_FILE"
    fi

    echo "" >> "$RUN_LOG"
done

echo "Results saved to: $RESULTS_FILE"
echo "Done!"
