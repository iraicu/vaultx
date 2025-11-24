#!/bin/bash

# Script to measure plotting metrics for k=32
# Usage: ./measure_plotting_metrics.sh [output_file] [num_plots] [threads] [source_dir] [dest_dir]

set -e

OUTPUT_FILE="${1:-plotting_metrics_k32.txt}"
NUM_PLOTS="${2:-1}"
THREADS="${3:-128}"
K=32

# Configuration
F="${4:-.}"          # Source filesystem (default: current directory)
T="${5:-.}"          # Destination filesystem (default: current directory)
MEMORY="1024"        # Memory per batch (MB)

# Check if vaultx executable exists
if [[ ! -f "../vaultx" ]]; then
  echo "Error: vaultx executable not found in parent directory."
  echo "Please compile with: make vaultx_x86_c NONCE_SIZE=4 RECORD_SIZE=16"
  exit 1
fi

echo "Starting plotting metrics measurement for k=$K..."
echo ""
echo "Parameters:"
echo "  K: $K"
echo "  Number of plots: $NUM_PLOTS"
echo "  Threads: $THREADS"
echo "  Source: $F"
echo "  Destination: $T"
echo "  Output file: $OUTPUT_FILE"
echo ""

# Initialize output file with header
cat > "$OUTPUT_FILE" << EOF
Plotting Metrics Measurement - k=$K
Generated: $(date)
================================================================================

Parameters:
  K: $K
  Number of plots: $NUM_PLOTS
  Threads: $THREADS
  Memory: $MEMORY MB
  Source: $F
  Destination: $T

================================================================================

GENERATION PHASE:
EOF

# Run generation with k=32
GEN_CMD="../vaultx -t $THREADS -a for -K $K -m $MEMORY -n $NUM_PLOTS -F $F -T $T"
echo "Executing: $GEN_CMD"
echo "" >> "$OUTPUT_FILE"
echo "Generation Command: $GEN_CMD" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Capture output and timing
START_TIME=$(date +%s%N)
eval "$GEN_CMD" 2>&1 | tee -a "$OUTPUT_FILE"
END_TIME=$(date +%s%N)
ELAPSED_MS=$(( (END_TIME - START_TIME) / 1000000 ))
ELAPSED_S=$(echo "scale=2; $ELAPSED_MS / 1000" | bc)

echo "" >> "$OUTPUT_FILE"
echo "Generation Phase Total Time: ${ELAPSED_S}s" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "Generation phase completed in ${ELAPSED_S}s"
echo ""

# Run merge phase
echo "" >> "$OUTPUT_FILE"
echo "================================================================================"  >> "$OUTPUT_FILE"
echo "MERGE PHASE:" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Run merge with approach 0
MERGE_APPROACH=0
MERGE_CMD="../vaultx -t $THREADS -a for -K $K -m $MEMORY -n $NUM_PLOTS -F $F -T $T -M $MERGE_APPROACH"
echo "Executing: $MERGE_CMD"
echo "Merge Command: $MERGE_CMD" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Capture output and timing
START_TIME=$(date +%s%N)
eval "$MERGE_CMD" 2>&1 | tee -a "$OUTPUT_FILE"
MERGE_EXIT_CODE=$?
END_TIME=$(date +%s%N)
ELAPSED_MS=$(( (END_TIME - START_TIME) / 1000000 ))
ELAPSED_S=$(echo "scale=2; $ELAPSED_MS / 1000" | bc)

echo "" >> "$OUTPUT_FILE"
echo "Merge Phase Total Time: ${ELAPSED_S}s" >> "$OUTPUT_FILE"
if [[ $MERGE_EXIT_CODE -eq 0 ]]; then
  echo "Merge completed successfully." >> "$OUTPUT_FILE"
else
  echo "Merge exited with code: $MERGE_EXIT_CODE" >> "$OUTPUT_FILE"
fi
echo "" >> "$OUTPUT_FILE"

echo "Merge phase completed in ${ELAPSED_S}s"
echo ""

echo "================================================================================"
echo "Plotting metrics saved to: $OUTPUT_FILE"
