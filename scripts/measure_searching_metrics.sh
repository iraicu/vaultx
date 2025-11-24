#!/bin/bash

# Script to measure searching metrics for k=32
# Usage: ./measure_searching_metrics.sh [output_file] [num_searches] [num_plots] [threads] [difficulty] [source_dir] [dest_dir]

set -e

OUTPUT_FILE="${1:-searching_metrics_k32.txt}"
NUM_SEARCHES="${2:-10}"
NUM_PLOTS="${3:-1}"
THREADS="${4:-32}"
DIFFICULTY="${5:-0}"
K=32

# Configuration
F="${6:-ssd-raid0}"  # Source filesystem (default: ssd-raid0)
T="${7:-ssd-raid0}"  # Destination filesystem (default: ssd-raid0)
MEMORY="1024"        # Memory per batch (MB)

# Check if vaultx executable exists
if [[ ! -f "../vaultx" ]]; then
  echo "Error: vaultx executable not found in parent directory."
  echo "Please compile with: make vaultx_x86_c NONCE_SIZE=4 RECORD_SIZE=16"
  exit 1
fi

echo "Starting searching metrics measurement for k=$K..."
echo ""
echo "Parameters:"
echo "  K: $K"
echo "  Number of searches: $NUM_SEARCHES"
echo "  Number of plots: $NUM_PLOTS"
echo "  Threads: $THREADS"
echo "  Difficulty: $DIFFICULTY (0 = full hash, >0 = bytes to compare)"
echo "  Source: $F"
echo "  Destination: $T"
echo "  Output file: $OUTPUT_FILE"
echo ""

# Initialize output file with header
cat > "$OUTPUT_FILE" << EOF
Searching Metrics Measurement - k=$K
Generated: $(date)
================================================================================

Parameters:
  K: $K
  Number of searches: $NUM_SEARCHES
  Number of plots: $NUM_PLOTS
  Threads: $THREADS
  Difficulty: $DIFFICULTY (0 = full hash, >0 = bytes to compare)
  Memory: $MEMORY MB
  Source: $F
  Destination: $T

Note: This assumes plots have already been generated at:
  $T/data-l/vaultx_merged_for_*_${K}.plot

================================================================================

SEARCHING PHASE:
EOF

# Optional: Drop caches before search
read -p "Drop filesystem caches before search? (requires sudo) [y/N]: " DROP_CACHE
if [[ "$DROP_CACHE" =~ ^[Yy]$ ]]; then
  echo "Dropping caches..."
  sudo sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches'
  echo "Caches dropped." | tee -a "$OUTPUT_FILE"
  echo "" >> "$OUTPUT_FILE"
fi

# Run search with k=32
SEARCH_CMD="../vaultx -t $THREADS -a for -K $K -m $MEMORY -n $NUM_PLOTS -T $T -F $F -S $NUM_SEARCHES -D $DIFFICULTY"
echo "Executing: $SEARCH_CMD"
echo "" >> "$OUTPUT_FILE"
echo "Search Command: $SEARCH_CMD" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Capture output and timing
START_TIME=$(date +%s%N)
eval "$SEARCH_CMD" 2>&1 | tee -a "$OUTPUT_FILE"
SEARCH_EXIT_CODE=$?
END_TIME=$(date +%s%N)
ELAPSED_MS=$(( (END_TIME - START_TIME) / 1000000 ))
ELAPSED_S=$(echo "scale=2; $ELAPSED_MS / 1000" | bc)

echo "" >> "$OUTPUT_FILE"
echo "Search Phase Total Time: ${ELAPSED_S}s" >> "$OUTPUT_FILE"
if [[ $SEARCH_EXIT_CODE -eq 0 ]]; then
  echo "Search completed successfully." >> "$OUTPUT_FILE"
else
  echo "Search exited with code: $SEARCH_EXIT_CODE" >> "$OUTPUT_FILE"
fi
echo "" >> "$OUTPUT_FILE"

echo "Search phase completed in ${ELAPSED_S}s"
echo ""

echo "================================================================================"
echo "Searching metrics saved to: $OUTPUT_FILE"
