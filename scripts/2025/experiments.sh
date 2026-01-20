#!/bin/bash

usage() {
  echo "Usage: $0 -f <num_files> -t \"<thread_counts>\" -b \"<batch_sizes>\" -n <num_runs> -d <output_path> -M <M_flag_value> [-T <T_flag>] [-F <F_flag>]"
  echo "Example: $0 -f 256 -t \"4 8\" -b \"2 4 8 16\" -n 10 -d /path/to/folder -M 1 -T -1 -F -32"
  exit 1
}

THREADS=()
BATCH_SIZES=()
FILES=""
OUTDIR=""
NUM_RUNS=""
M_FLAG=""
T_FLAG=""
F_FLAG=""

while getopts "f:t:b:d:n:M:T:F:" opt; do
  case "$opt" in
    f) FILES="$OPTARG" ;;
    t) IFS=' ' read -r -a THREADS <<< "$OPTARG" ;;
    b) IFS=' ' read -r -a BATCH_SIZES <<< "$OPTARG" ;;
    d) OUTDIR="${OPTARG%/}" ;;
    n) NUM_RUNS="$OPTARG" ;;
    M) M_FLAG="$OPTARG" ;;
    T) T_FLAG="$OPTARG" ;;
    F) F_FLAG="$OPTARG" ;;
    *) usage ;;
  esac
done

if [ -z "$FILES" ] || [ ${#THREADS[@]} -eq 0 ] || [ ${#BATCH_SIZES[@]} -eq 0 ] || \
   [ -z "$OUTDIR" ] || [ -z "$NUM_RUNS" ] || [ -z "$M_FLAG" ]; then
  usage
fi

if [ ! -d "$OUTDIR" ]; then
  mkdir -p "$OUTDIR"
  USER_NAME=$(whoami)
  sudo chown -R "$USER_NAME:$USER_NAME" "$OUTDIR"
  sudo chmod -R 777 "$OUTDIR"
fi

get_time() {
  awk -F': ' -v key="$1" '$0 ~ key {gsub(/s/, "", $2); printf "%.2f", $2; exit}' "$2"
}

check_csv_done_new_format() {
  local csv="$1"
  local t="$2"
  local b="$3"
  grep -q "^$t,$b," "$csv"
}

check_csv_done_old_format() {
  local csv="$1"
  local t="$2"
  local b="$3"
  local header
  header=$(head -1 "$csv")
  local col=0
  IFS=',' read -ra columns <<< "$header"
  for i in "${!columns[@]}"; do
    if [[ "${columns[$i]}" == "Threads=$t" ]]; then
      col="$i"
      break
    fi
  done
  val=$(awk -F',' -v b="$b" -v c="$col" '$1==b {print $c}' "$csv")
  [[ -n "$val" && "$val" != "-1" ]]
}

write_csv_header_new() {
  echo "Threads,BatchSize,ReadTime,WriteTime,MergeTime,TotalTime" > "$1"
}

write_csv_header_old() {
  echo -n "BatchSize" > "$1"
  for t in "${THREADS[@]}"; do
    echo -n ",Threads=$t" >> "$1"
  done
  echo "" >> "$1"
}

is_run_complete() {
  local run_dir="$1"
  local csv_file="${run_dir}/results.csv"

  if [ ! -f "$csv_file" ]; then
    return 1
  fi

  for m in "${BATCH_SIZES[@]}"; do
    for t in "${THREADS[@]}"; do
      if [ "$M_FLAG" == "1" ] || [ "$M_FLAG" == "2" ]; then
        if ! check_csv_done_new_format "$csv_file" "$t" "$m"; then
          return 1
        fi
      else
        if ! check_csv_done_old_format "$csv_file" "$t" "$m"; then
          return 1
        fi
      fi
    done
  done
  return 0
}

run_test() {
  local run_idx="$1"
  local run_dir="$2"
  local csv_file="${run_dir}/results.csv"

  mkdir -p "$run_dir"
  USER_NAME=$(whoami)
  sudo chown -R "$USER_NAME:$USER_NAME" "$run_dir"
  sudo chmod -R 777 "$run_dir"

  if [ ! -f "$csv_file" ]; then
    if [ "$M_FLAG" == "1" ] || [ "$M_FLAG" == "2" ]; then
      write_csv_header_new "$csv_file"
    else
      write_csv_header_old "$csv_file"
    fi
  fi

  for m in "${BATCH_SIZES[@]}"; do
    if [ "$M_FLAG" != "1" ] && [ "$M_FLAG" != "2" ]; then
      row="$m"
      skip_row=false
    fi

    for t in "${THREADS[@]}"; do
      if [ "$M_FLAG" == "1" ] || [ "$M_FLAG" == "2" ]; then
        if check_csv_done_new_format "$csv_file" "$t" "$m"; then
          echo ">>> Skipping Threads=$t, BatchSize=$m (already in results.csv)"
          continue
        fi
      else
        if check_csv_done_old_format "$csv_file" "$t" "$m"; then
          echo ">>> Skipping Threads=$t, BatchSize=$m (already in results.csv)"
          row+=","
          continue
        fi
      fi

      OUTPUT_FILE="${run_dir}/vaultx_t${t}_m${m}.log"
      CMD="./vaultx -a for -K 28 -v false -M $M_FLAG -n $FILES -t $t -m $m"

      if [ -n "$T_FLAG" ]; then
        CMD+=" -T $T_FLAG"
      fi
      if [ -n "$F_FLAG" ]; then
        CMD+=" -F $F_FLAG"
      fi

      echo -e "\n>>> Run $run_idx: Threads=$t, BatchSize=${m}MB"
      echo "Output: $OUTPUT_FILE"
      
      echo "Running command: $CMD"

      {
        echo "Start Time: $(date)"
        eval "$CMD"
        echo "Completed at: $(date)"
      } > "$OUTPUT_FILE" 2>&1

      if [ "$M_FLAG" == "1" ] || [ "$M_FLAG" == "2" ]; then
        READ_TIME=$(get_time "Read Time:" "$OUTPUT_FILE")
        WRITE_TIME=$(get_time "Write Time:" "$OUTPUT_FILE")
        MERGE_TIME=$(get_time "Merge Time:" "$OUTPUT_FILE")
        TOTAL_TIME=$(get_time "Merge complete:" "$OUTPUT_FILE")

        READ_TIME=${READ_TIME:-"-1"}
        WRITE_TIME=${WRITE_TIME:-"-1"}
        MERGE_TIME=${MERGE_TIME:-"-1"}
        TOTAL_TIME=${TOTAL_TIME:-"-1"}

        echo "$t,$m,$READ_TIME,$WRITE_TIME,$MERGE_TIME,$TOTAL_TIME" >> "$csv_file"
      else
        TOTAL_TIME=$(get_time "Merge complete:" "$OUTPUT_FILE")
        TOTAL_TIME=${TOTAL_TIME:-"-1"}
        row+=",$TOTAL_TIME"
        skip_row=false
      fi

      echo "Dropping caches..."
      sync
      sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
      sleep 2
    done

    if [ "$M_FLAG" != "1" ] && [ "$M_FLAG" != "2" ] && [ "$skip_row" = false ]; then
      echo "$row" >> "$csv_file"
    fi
  done

  if [ "$M_FLAG" == "1" ] || [ "$M_FLAG" == "2" ]; then
    header=$(head -1 "$csv_file")
    tail -n +2 "$csv_file" | sort -t',' -k1,1n -k2,2n > "${csv_file}.sorted"
    echo "$header" | cat - "${csv_file}.sorted" > "$csv_file"
    rm "${csv_file}.sorted"
  fi
}

# === MAIN ===
for run in $(seq 1 "$NUM_RUNS"); do
  RUN_DIR="${OUTDIR}/run_${run}"

  if [ -d "$RUN_DIR" ] && is_run_complete "$RUN_DIR"; then
    echo ">>> Run $run is complete. Skipping."
    continue
  fi

  echo ">>> Running run $run in $RUN_DIR"
  run_test "$run" "$RUN_DIR"
done

echo -e "\n=== Benchmarking complete at $(date) ==="
