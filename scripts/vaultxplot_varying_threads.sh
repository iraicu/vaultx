#!/usr/bin/env bash
# vaultx_plot_varying_threads.sh
# Benchmarks vaultx plot generation by varying either IO or Compute threads
# while holding the other thread type constant (32 for IO, 1 for Compute).
#
# For each specified k-value and drive, the chosen thread type is swept from
# 1 up to nproc in common increments (1,2,4,8,16,32,64,96,128,192,256,384,…,n).
# When IO threads are varied, compute threads are fixed at 32; when compute
# threads are varied, IO threads are fixed at 1.
#
# Usage:
#   # Run all experiments for both thread types and batches:
#   ./vaultx_plot_varying_threads.sh -thread IO
#   ./vaultx_plot_varying_threads.sh -mode IM  -thread IO
#   ./vaultx_plot_varying_threads.sh -mode IM  -thread CP
#   ./vaultx_plot_varying_threads.sh -mode OOM -thread IO -batch 2
#   ./vaultx_plot_varying_threads.sh -mode OOM -thread CP -batch 4
#
# CSV naming convention:
#   varying_IO_k27_<drive>_IM.csv
#   varying_CP_k32_<drive>_OOM_2batch.csv
set -euo pipefail


ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-${ROOT_DIR}/vaultx}"
EXPERIMENTS_DIR="${ROOT_DIR}/experiments"


FINAL_DRIVES=(
  "./plots"
)

TEMP_DRIVES=(
  "./temp"
)

# (27 28 29 30 31 32) for the full range)

K_VALUES=(27)

declare -A OOM2_MEM=([27]=2.5 [28]=3 [29]=5 [30]=8 [31]=14 [32]=26)

declare -A OOM4_MEM=([27]=2.0 [28]=2.5 [29]=3 [30]=5 [31]=8 [32]=14)

CLI_MODE=""      # IM | OOM
CLI_THREAD=""    # IO | CP
CLI_BATCH=""     # 2  | 4 

usage() {
  cat <<'EOF'
Usage: vaultx_plot_varying_threads.sh -mode IM|OOM -thread IO|CP [-batch 2|4] [-h]

Options:
  -mode IM|OOM       Experiment mode (required)
  -thread IO|CP      Thread type to vary (required)
                       IO = vary I/O threads (-i), keep compute threads (-t) at 1
                       CP = vary compute threads (-t), keep I/O threads (-i) at 1
  -batch 2|4         Batch count for OOM mode (required when -mode OOM)
  -h                 Show this help and exit

For each k-value and drive combination the script varies the Compute
thread type from 1 up to the number of available CPU cores in common
increments (1, 2, 4, 8, 16, 32, 64, 96, 128, 192, 256, 384, …, nproc).
While for IO, it goes from 1,2,4,8.
Caches are cleared and generated files deleted after every run.

CSV naming:
  varying_<threadtype>_k<k>_<drive>_<mode>[_<batch>batch].csv
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -mode)
      CLI_MODE="${2:-}"
      shift 2
      ;;
    -thread)
      CLI_THREAD="${2:-}"
      shift 2
      ;;
    -batch)
      CLI_BATCH="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

# --- Validate required arguments -------------------------------------------
if [[ -z "$CLI_THREAD" ]]; then
  echo "Error: -thread is required" >&2
  usage >&2
  exit 1
fi
if [[ "$CLI_THREAD" != "IO" && "$CLI_THREAD" != "CP" ]]; then
  echo "Error: -thread must be IO or CP (got: '${CLI_THREAD}')" >&2
  usage >&2
  exit 1
fi

# Default to running all modes and batches if only the thread type is specified
if [[ -z "$CLI_MODE" && -z "$CLI_BATCH" ]]; then
  RUN_LIST=("IM" "OOM:2" "OOM:4")
else
  if [[ -z "$CLI_MODE" ]]; then
    echo "Error: -mode is required" >&2
    usage >&2
    exit 1
  fi
  RUN_LIST=("$CLI_MODE${CLI_BATCH:+:$CLI_BATCH}")
fi

if [[ "$CLI_MODE" == "OOM" && -z "$CLI_BATCH" ]]; then
  echo "Error: -batch is required when -mode is OOM" >&2
  usage >&2
  exit 1
fi
if [[ -n "$CLI_BATCH" && "$CLI_BATCH" != "2" && "$CLI_BATCH" != "4" ]]; then
  echo "Error: -batch must be 2 or 4 (got: '${CLI_BATCH}')" >&2
  usage >&2
  exit 1
fi
if [[ -n "$CLI_BATCH" && "$CLI_MODE" != "OOM" ]]; then
  echo "Error: -batch is only valid with -mode OOM" >&2
  usage >&2
  exit 1
fi

# --- Validate binary & drives -
if [[ ! -x "${BIN}" ]]; then
  echo "Error: vaultx binary not found or not executable at ${BIN}" >&2
  exit 1
fi

if [[ ${#FINAL_DRIVES[@]} -eq 0 ]]; then
  echo "Error: FINAL_DRIVES array is empty" >&2
  exit 1
fi

if [[ "$CLI_MODE" == "OOM" ]]; then
  if [[ ${#TEMP_DRIVES[@]} -eq 0 ]]; then
    echo "Error: TEMP_DRIVES array is empty (required for OOM runs)" >&2
    exit 1
  fi
  if [[ ${#TEMP_DRIVES[@]} -gt 1 && ${#TEMP_DRIVES[@]} -ne ${#FINAL_DRIVES[@]} ]]; then
    echo "Error: TEMP_DRIVES must have either 1 entry (shared) or the same number of entries as FINAL_DRIVES (${#FINAL_DRIVES[@]}). Got ${#TEMP_DRIVES[@]}." >&2
    exit 1
  fi
fi

mkdir -p "${EXPERIMENTS_DIR}"

# Produces the sorted, deduplicated list of thread counts to sweep through:
MAX_THREADS="$(nproc)"

generate_thread_counts() {
  local max="$1"
  local -a raw=()
  local val=1

  while (( val <= max )); do
    raw+=("$val")
    val=$(( val * 2 ))
  done

  val=64
  while (( val <= max )); do
    local mid=$(( val * 3 / 2 ))
    if (( mid <= max )); then
      raw+=("$mid")
    fi
    val=$(( val * 2 ))
  done

  raw+=("$max")

  printf '%s\n' "${raw[@]}" | sort -n -u
}

IO_THREAD_COUNTS=(1 2 4 8)

COMPUTE_THREAD_COUNTS=()
while IFS= read -r t; do
  COMPUTE_THREAD_COUNTS+=("$t")
done < <(generate_thread_counts "$MAX_THREADS")

if [[ "$CLI_THREAD" == "IO" ]]; then
  THREAD_COUNTS=("${IO_THREAD_COUNTS[@]}")
  CONSTANT_THREADS=32
else
  THREAD_COUNTS=("${COMPUTE_THREAD_COUNTS[@]}")
  CONSTANT_THREADS=1
fi

echo "Thread counts to test: ${THREAD_COUNTS[*]}" >&2
echo "Total runs per k-value per drive: ${#THREAD_COUNTS[@]}" >&2

# Flush page-cache for clean, reproducible timing.
drop_caches() {
  if echo "sfatunmbi" | sudo -S sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null; then
    return 0
  fi
  echo "Warning: Failed to drop caches via sudo; falling back to sync only" >&2
  sync
}

# Extract the drive name 
drive_id() { echo "$1" | cut -d'/' -f2; }

extract_field() {
  local pattern="$1" file="$2"
  sed -n "${pattern}" "${file}" 2>/dev/null | head -1 || true
}

# Parse a vaultx log file and emit a single CSV row.
parse_log() {
  local log="$1"
  local varying_threads="$2"

  local k_value compute_threads io_threads file_size_gb write_batch_mb read_batch
  local storage_eff overall_io_mb_s total_time_s total_time_min peak_mem_mb total_throughput_mh_s

  k_value=$(extract_field         's/.*Exponent k[[:space:]]*:[[:space:]]*\([0-9]\+\).*/\1/p' "${log}")
  compute_threads=$(extract_field 's/.*Threads (Hash\/Sort)[[:space:]]*:[[:space:]]*\([0-9]\+\).*/\1/p' "${log}")
  io_threads=$(extract_field      's/.*Threads (I\/O)[[:space:]]*:[[:space:]]*\([0-9]\+\).*/\1/p' "${log}")
  file_size_gb=$(extract_field    's/.*File Size (GB)[[:space:]]*:[[:space:]]*\([0-9.]*\).*/\1/p' "${log}")
  write_batch_mb=$(extract_field  's/.*WRITE_BATCH_SIZE[[:space:]]*:[[:space:]]*\([0-9]\+\).*/\1/p' "${log}")
  read_batch=$(extract_field      's/.*READ_BATCH_SIZE[[:space:]]*:[[:space:]]*\([0-9]\+\).*/\1/p' "${log}")
  storage_eff=$(extract_field     's/.*storage_efficiency_table2=\([0-9.]*\).*/\1/p' "${log}")
  overall_io_mb_s=$(extract_field 's/.*Overall I\/O Throughput:[[:space:]]*\([0-9.]*\)[[:space:]]*MB\/s.*/\1/p' "${log}")
  total_time_s=$(extract_field    's/.*Total Time:[[:space:]]*\([0-9.]*\).*/\1/p' "${log}")
  peak_mem_mb=$(extract_field     's/.*Peak Memory Usage:[[:space:]]*\([0-9.]*\)[[:space:]]*MB.*/\1/p' "${log}")
  total_throughput_mh_s=$(extract_field 's/.*Total Throughput:[[:space:]]*\([0-9.]*\)[[:space:]]*MH\/s.*/\1/p' "${log}")

  # Convert total time to minutes
  if [[ -n "${total_time_s}" ]]; then
    total_time_min="$(awk "BEGIN { printf \"%.2f\", ${total_time_s} / 60 }")"
  else
    total_time_s="NA"
    total_time_min="NA"
  fi

  echo "${varying_threads},${k_value:-NA},${compute_threads:-NA},${io_threads:-NA},${file_size_gb:-NA},${write_batch_mb:-NA},${read_batch:-NA},${storage_eff:-NA},${overall_io_mb_s:-NA},${total_time_s:-NA},${total_time_min},${peak_mem_mb:-NA},${total_throughput_mh_s:-NA}"
}

cleanup_files() {
  local final_drive="$1"
  local temp_drive="$2"

  if [[ -d "${final_drive}/plots" ]]; then
    rm -f "${final_drive}/plots"/*.plot 2>/dev/null || true
  fi
  if [[ -n "${temp_drive}" && -d "${temp_drive}/temp" ]]; then
    rm -f "${temp_drive}/temp"/* 2>/dev/null || true
  fi
}


# CSV header
CSV_HEADER="varying_threads,k,threads_compute,threads_io,file_size_gb,write_batch_mb,read_batch,storage_efficiency_pct,overall_io_throughput_mb_s,total_time_s,total_time_min,peak_memory_mb,total_throughput_mh_s"

# Run a single vaultx invocation
run_once() {
  local k="$1"
  local num_threads="$2"
  local final_drive="$3"
  local temp_drive="$4"
  local csv_file="$5"

  local plots_dir="${final_drive}/plots"
  mkdir -p "${plots_dir}"

  # Determine compute / IO thread counts based on which type is being varied
  local compute_threads io_threads
  if [[ "$CLI_THREAD" == "IO" ]]; then
    compute_threads="$CONSTANT_THREADS"
    io_threads="${num_threads}"
  else
    compute_threads="${num_threads}"
    io_threads="$CONSTANT_THREADS"
  fi

  # OOM temp directory
  local temp_arg=()
  if [[ "$CLI_MODE" == "OOM" ]]; then
    local temp_dir="${temp_drive}/temp"
    mkdir -p "${temp_dir}"
    temp_arg=(-g "${temp_dir}")
  fi

  # OOM memory cap
  local mem_arg=()
  if [[ "$CLI_MODE" == "OOM" ]]; then
    local mem_gb
    if [[ "$CLI_BATCH" == "2" ]]; then
      mem_gb="${OOM2_MEM[$k]}"
    else
      mem_gb="${OOM4_MEM[$k]}"
    fi
    mem_arg=(-m "${mem_gb}")
  fi

  local log
  log="$(mktemp --suffix=".vaultx.log")"

  echo "" >&2
  echo "  → k=${k}  varying_${CLI_THREAD}=${num_threads}  compute=${compute_threads}  io=${io_threads}" >&2

  # Clear caches before
  drop_caches

  if ! "${BIN}" \
        -k "${k}" \
        "${temp_arg[@]}" \
        -f "${plots_dir}" \
        -t "${compute_threads}" \
        -i "${io_threads}" \
        "${mem_arg[@]}" 2>&1 | tee "${log}"; then
    echo "Error: vaultx failed for k=${k} ${CLI_THREAD}=${num_threads}" >&2
    rm -f "${log}"
    # Still attempt cleanup so disk space is reclaimed
    cleanup_files "${final_drive}" "${temp_drive}"
    return 1
  fi

  drop_caches

  parse_log "${log}" "${num_threads}" >> "${csv_file}"
  rm -f "${log}"

  cleanup_files "${final_drive}" "${temp_drive}"
}

n_final="${#FINAL_DRIVES[@]}"
n_temp="${#TEMP_DRIVES[@]}"

for experiment in "${RUN_LIST[@]}"; do
  IFS=':' read -r exp_mode exp_batch <<< "${experiment}"

  for (( di=0; di<n_final; di++ )); do
    final_drive="${FINAL_DRIVES[$di]}"
    # OOM temp drive: 1-to-1 when counts match, single shared drive otherwise
    if [[ $n_temp -eq 1 ]]; then
      temp_drive="${TEMP_DRIVES[0]}"
    else
      temp_drive="${TEMP_DRIVES[$di]}"
    fi
    did="$(drive_id "${final_drive}")"

    for k in "${K_VALUES[@]}"; do
      if [[ "$exp_mode" == "IM" ]]; then
        csv_label="IM"
      else
        csv_label="OM_${exp_batch}batch"
      fi

      csv_file="${EXPERIMENTS_DIR}/varying_${CLI_THREAD}_k${k}_${did}_${csv_label}.csv"

      echo "" >&2
      echo "============================================================" >&2
      echo " Experiment  : Varying ${CLI_THREAD} threads" >&2
      echo " Mode        : ${exp_mode}${exp_batch:+ (${exp_batch}-batch)}" >&2
      echo " k           : ${k}" >&2
      echo " Drive       : ${final_drive}" >&2
      if [[ "$exp_mode" == "OOM" ]]; then
        echo " Temp drive  : ${temp_drive}" >&2
      fi
      echo " CSV         : ${csv_file}" >&2
      echo " Thread sweep: ${THREAD_COUNTS[*]}" >&2
      echo "============================================================" >&2

      # Write CSV header (creates / overwrites the file)
      printf "%s\n" "${CSV_HEADER}" > "${csv_file}"

      for tc in "${THREAD_COUNTS[@]}"; do
        run_once "${k}" "${tc}" "${final_drive}" "${temp_drive}" "${csv_file}"
      done

      echo "" >&2
      echo "  ✓ Results written → ${csv_file}" >&2
    done
  done
done

echo "" >&2
echo "All experiments complete." >&2
echo "Results directory: ${EXPERIMENTS_DIR}" >&2
