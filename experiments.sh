#!/bin/bash

# Help message
usage() {
  echo "Usage: $0 -f <num_files> -t \"<thread_counts>\" -b \"<batch_sizes>\" -n <num_runs> [-d <full_output_path>] [-M <M_flag_value>]"
  echo "Example: $0 -f 256 -t \"4 8\" -b \"2 4 8 16 32\" -n 3 -d /full/path/to/folder -M true"
  exit 1
}

# Default values
THREADS=()
BATCH_SIZES=()
FILES=""
OUTDIR=""
NUM_RUNS=1
M_FLAG=""

# Parse options
while getopts "f:t:b:d:n:M:" opt; do
  case "$opt" in
    f) FILES="$OPTARG" ;;
    t) IFS=' ' read -r -a THREADS <<< "$OPTARG" ;;
    b) IFS=' ' read -r -a BATCH_SIZES <<< "$OPTARG" ;;
    d) OUTDIR="$OPTARG" ;;
    n) NUM_RUNS="$OPTARG" ;;
    M) M_FLAG="$OPTARG" ;;
    *) usage ;;
  esac
done

# Check mandatory args
if [ -z "$FILES" ] || [ ${#THREADS[@]} -eq 0 ] || [ ${#BATCH_SIZES[@]} -eq 0 ]; then
  usage
fi

# Set default output directory if not provided
if [ -z "$OUTDIR" ]; then
  OUTDIR="./vaultx_${FILES}_tests"
fi

# Create full output directory if it doesn't exist
mkdir -p "$OUTDIR"
sudo chown -R asirigere:asirigere "$OUTDIR"

echo "=== Starting vaultx benchmark at $(date) ==="
echo "Running $NUM_RUNS repetitions into $OUTDIR"

for run in $(seq 1 "$NUM_RUNS"); do
  RUN_DIR="${OUTDIR}/run_${run}"
  mkdir -p "$RUN_DIR"
  CSV_FILE="${RUN_DIR}/results.csv"

  echo "Starting run $run -> output in $RUN_DIR"

  # Write CSV header
  echo -n "BatchSize" > "$CSV_FILE"
  for t in "${THREADS[@]}"; do
    echo -n ",Threads=$t" >> "$CSV_FILE"
  done
  echo "" >> "$CSV_FILE"

  # Run benchmarks
  for m in "${BATCH_SIZES[@]}"; do
    echo -n "$m" >> "$CSV_FILE"

    for t in "${THREADS[@]}"; do
      OUTPUT_FILE="${RUN_DIR}/vaultx_t${t}_m${m}.log"
      CMD="./vaultx -f memo.t -g memo.x -j memo.xx -a for -K 28 -v true"

      if [ -n "$M_FLAG" ]; then
        CMD="$CMD -M $M_FLAG"
      fi

      CMD="$CMD -n $FILES -t $t -m $m"

      echo -e "\n>>> Run $run: Threads=$t, BatchSize=${m}MB"
      echo "Output: $OUTPUT_FILE"

      {
        echo "Start Time: $(date)"
        $CMD 2>&1
        echo "Completed at: $(date)"
      } > "$OUTPUT_FILE" 2>&1

      # Extract merge time
      RAW_MERGE_TIME=$(grep "Merge complete:" "$OUTPUT_FILE" | awk -F': ' '{print $2}' | sed 's/s//')

      if [ -n "$RAW_MERGE_TIME" ]; then
        MERGE_TIMESTAMP=$(printf "%.0f" "$RAW_MERGE_TIME")
      else
        MERGE_TIMESTAMP=-1
      fi

      echo -n ",$MERGE_TIMESTAMP" >> "$CSV_FILE"

      # Drop caches
      echo "Dropping caches..."
      sync
      sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
      sleep 2
    done

    echo "" >> "$CSV_FILE"
  done
done

echo -e "\n=== All $NUM_RUNS benchmark runs completed at $(date) ==="
