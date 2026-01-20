#!/bin/bash

usage() {
  echo "Usage: $0 -S <final_merge_plot_size_gb> [-l <log_folder_name>]"
  exit 1
}

# Parse -S and -l arguments
while getopts ":S:l:" opt; do
  case $opt in
    S)
      FINAL_SIZE_GB=$OPTARG
      ;;
    l)
      LOG_ROOT=$OPTARG
      ;;
    *)
      usage
      ;;
  esac
done

if [[ -z "$FINAL_SIZE_GB" || ! "$FINAL_SIZE_GB" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
  echo "Error: -S argument (final merge plot size in GB) is required and must be a positive number."
  usage
fi

# Default log folder if -l not provided
LOG_ROOT=${LOG_ROOT:-"logs"}
mkdir -p "$LOG_ROOT"

CONFIGS=(
  "ssd-raid0 ssd-raid0"
  # Add more (F,T) pairs here
)

get_merge_params() {
  local F=$1
  local T=$2
  local N=$3

  local BIG_M=0
  local SMALL_m=1024
  local THREADS=128

  if [[ "$F" == "ssd-raid0" && "$T" == "ssd-raid0" ]]; then
    SMALL_m=256
  else
    SMALL_m=1024
  fi

  if [[ "$F" == "$T" ]]; then
    BIG_M=1
  else
    BIG_M=2
  fi

  echo "$BIG_M $SMALL_m $THREADS"
}

echo "Final merge plot size (S): $FINAL_SIZE_GB GB"
echo "Logging to: $LOG_ROOT"
echo

declare -A generated_sources

for config in "${CONFIGS[@]}"; do
  read -r F T <<< "$config"
  echo "======================================"
  echo "Running experiment for F=$F, T=$T"

  LOG_DIR="${LOG_ROOT}/${F}_to_${T}"
  mkdir -p "$LOG_DIR"

  # Check if we already generated for this F
  run_generation=true
  if [[ -n "${generated_sources[$F]}" ]]; then
    run_generation=false
  fi

  for ((K = 26; K <= 32; K++)); do
    denom=$(echo "2^$K * 8 / (1024*1024*1024)" | bc -l)
    N=$(echo "scale=6; $FINAL_SIZE_GB / $denom" | bc -l)
    N_int=$(printf "%.0f" "$N")
    read BIG_M SMALL_m MERGE_THREADS < <(get_merge_params "$F" "$T" "$N_int")

    if $run_generation; then
      GEN_CMD="./vaultx -t 128 -a for -K $K -m 1024 -n $N_int -F $F -T $T"
      GEN_LOG="${LOG_DIR}/gen_K${K}.log"
      echo "  [GEN] K=$K, N=$N_int"
      echo "    Executing generation, logging to $GEN_LOG"
      echo "$GEN_CMD"
      eval "$GEN_CMD" > "$GEN_LOG" 2>&1
    else
      echo "  [SKIP GEN] Already generated for F=$F, skipping generation for K=$K"
    fi

    # echo "    Dropping caches after merge..."
    # sudo sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches'

    # MERGE_CMD="./vaultx -t $MERGE_THREADS -a for -K $K -m $SMALL_m -n $N_int -F $F -T $T -M $BIG_M"
    # MERGE_LOG="${LOG_DIR}/merge_K${K}.log"
    # echo "  [MERGE] K=$K, M=$BIG_M, m=$SMALL_m, threads=$MERGE_THREADS, N=$N_int"
    # echo "    Executing merge, logging to $MERGE_LOG"
    # echo "$MERGE_CMD"
    # eval "$MERGE_CMD" > "$MERGE_LOG" 2>&1

    echo "    Dropping caches after merge..."
    sudo sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches'

    
    # Restored commented search block exactly as before
    # if [[ "$F" == "$T" && "$F" == "ssd-raid0" ]]; then
    #   SEARCH_CMD="sudo ./vaultx -t 32 -a for -K $K -m 1024 -n $N_int -T $T -F $F -S 10"
    #   SEARCH_LOG="${LOG_DIR}/search_k${K}.log"
    #   echo "  [SEARCH] K=$K, N=$N_int"
    #   echo "    Executing search, logging to $SEARCH_LOG"
    #   echo "$SEARCH_CMD"
    #   eval "$SEARCH_CMD" > "$SEARCH_LOG" 2>&1
    # fi

    echo "    Done with K=$K"
  done

  # Mark F as generated after the first config that runs generation for it
  if $run_generation; then
    generated_sources[$F]=1
  fi

  echo "Finished all K for F=$F, T=$T"
  echo
done
