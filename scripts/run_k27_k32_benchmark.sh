#!/usr/bin/env bash
# run_k27_k32_benchmark.sh
# Benchmarks vaultx plot generation from k27 to k32 across multiple drives.
# Supports three experiment modes: IM (in-memory), OOM-2batch, OOM-4batch.
# Results are saved as individual CSVs inside <repo>/experiments/.
#
# Usage:
#   ./run_k27_k32_benchmark.sh                   # run all modes on all drives
#   ./run_k27_k32_benchmark.sh -mode IM           # in-memory only, all drives
#   ./run_k27_k32_benchmark.sh -mode OOM          # both OOM batch sizes, all drives
#   ./run_k27_k32_benchmark.sh -mode OOM -batch 2 # OOM 2-batch only, all drives
#   ./run_k27_k32_benchmark.sh -mode OOM -batch 4 # OOM 4-batch only, all drives
set -euo pipefail


ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-${ROOT_DIR}/vaultx}"
EXPERIMENTS_DIR="${ROOT_DIR}/experiments"


# Drive configuration
#
# FINAL_DRIVES  – destinations for finished .plot files.
#                 Final plots are always written to ${FINAL_DRIVES[i]}/plots.
#                 TEMP_DRIVES is not used at all for IM runs.
#
# TEMP_DRIVES   – fast storage for OOM intermediate files only.
#                 Two valid configurations:
#
#   A) Same length as FINAL_DRIVES  →  strict 1-to-1 pairing:
#        temp for FINAL_DRIVES[i] goes to TEMP_DRIVES[i]/temp
#
#   B) Exactly one entry  →  single shared temp drive:
#        all OOM temp files go to TEMP_DRIVES[0]/temp
#        final plots still go to each respective FINAL_DRIVES[i]/plots
#
#   Any other count mismatch will abort with an error.

FINAL_DRIVES=(
  "/stor/auxiliary/sfatunmbi"
  "/ssd-raid0/sfatunmbi"
)

TEMP_DRIVES=(
  "/stor/auxiliary/sfatunmbi"
  "/ssd-raid0/sfatunmbi"
)


K_VALUES=(27 28 29 30 31 32)
COMPUTE_THREADS=16
IO_THREADS=1

# Memory limits (GB) that force ~2 rounds per k  (OOM-2batch)
declare -A OOM2_MEM=([27]=2.5 [28]=3 [29]=4.5 [30]=7.5 [31]=13.5 [32]=25.5)

# Memory limits (GB) that force ~4 rounds per k  (OOM-4batch)
# Values are roughly 1/4 of full in-memory requirement; floor at 2.0 (vaultx minimum)
declare -A OOM4_MEM=([27]=2.0 [28]=2.0 [29]=2.5 [30]=4.0 [31]=7.0 [32]=13.0)


CLI_MODE=""    # IM | OOM | empty → all modes
CLI_BATCH=""   # 2  | 4   | empty → all batch sizes

usage() {
  cat <<'EOF'
Usage: run_k27_k32_benchmark.sh [-mode IM|OOM] [-batch 2|4] [-h]

Options:
  -mode IM|OOM    Run only the specified mode (default: all modes)
  -batch 2|4      For OOM: run only the specified batch count (default: both 2 and 4)
  -h              Show this help and exit

Without arguments the script runs all three experiment types
(IM, OOM-2batch, OOM-4batch) sequentially for every drive in
FINAL_DRIVES. Each drive+mode combination produces its own CSV
inside <repo>/experiments/.

CSV naming convention:
  k27-k32_<drive>_IM.csv
  k27-k32_<drive>_OM_2batch.csv
  k27-k32_<drive>_OM_4batch.csv
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -mode)
      CLI_MODE="${2:-}"
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

if [[ -n "$CLI_MODE" && "$CLI_MODE" != "IM" && "$CLI_MODE" != "OOM" ]]; then
  echo "Error: -mode must be IM or OOM (got: '${CLI_MODE}')" >&2
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


if [[ ! -x "${BIN}" ]]; then
  echo "Error: vaultx binary not found or not executable at ${BIN}" >&2
  exit 1
fi

if [[ ${#FINAL_DRIVES[@]} -eq 0 ]]; then
  echo "Error: FINAL_DRIVES array is empty" >&2
  exit 1
fi

# Validate TEMP_DRIVES only when OOM experiments are going to run
# (CLI_MODE empty means all modes, which includes OOM)
if [[ -z "$CLI_MODE" || "$CLI_MODE" == "OOM" ]]; then
  if [[ ${#TEMP_DRIVES[@]} -eq 0 ]]; then
    echo "Error: TEMP_DRIVES array is empty (required for OOM runs)" >&2
    exit 1
  fi
  if [[ ${#TEMP_DRIVES[@]} -gt 1 && ${#TEMP_DRIVES[@]} -ne ${#FINAL_DRIVES[@]} ]]; then
    echo "Error: TEMP_DRIVES must have either 1 entry (shared) or the same number of entries as FINAL_DRIVES (${#FINAL_DRIVES[@]}) for 1-to-1 pairing. Got ${#TEMP_DRIVES[@]}." >&2
    exit 1
  fi
fi

mkdir -p "${EXPERIMENTS_DIR}"


# Flush page-cache for clean, reproducible timing.
drop_caches() {
  if echo "sfatunmbi" | sudo -S sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null; then
    return 0
  fi
  echo "Warning: Failed to drop caches via sudo; falling back to sync only" >&2
  sync
}


# Extract the drive name (first path component after /) from a path like /data-c/sfatunmbi
drive_id() { echo "$1" | cut -d'/' -f2; }


extract_field() {
  local pattern="$1" file="$2"
  sed -n "${pattern}" "${file}" 2>/dev/null | head -1 || true
}

parse_log() {
  local log="$1"
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

  # Convert total time to minutes  (e.g. 122 s → 2.03 min)
  if [[ -n "${total_time_s}" ]]; then
    total_time_min="$(awk "BEGIN { printf \"%.2f\", ${total_time_s} / 60 }")"
  else
    total_time_s="NA"
    total_time_min="NA"
  fi

  echo "${k_value:-NA},${compute_threads:-NA},${io_threads:-NA},${file_size_gb:-NA},${write_batch_mb:-NA},${read_batch:-NA},${storage_eff:-NA},${overall_io_mb_s:-NA},${total_time_s:-NA},${total_time_min},${peak_mem_mb:-NA},${total_throughput_mh_s:-NA}"
}

CSV_HEADER="k,threads_compute,threads_io,file_size_gb,write_batch_mb,read_batch,storage_efficiency_pct,overall_io_throughput_mb_s,total_time_s,total_time_min,peak_memory_mb,total_throughput_mh_s"

run_once() {
  local k="$1" mode="$2" batch="$3" final_drive="$4" temp_drive="$5" csv_file="$6"

  local plots_dir="${final_drive}/plots"
  mkdir -p "${plots_dir}"

  local temp_arg=()
  if [[ "$mode" == "OOM" ]]; then
    local temp_dir="${temp_drive}/temp"
    mkdir -p "${temp_dir}"
    temp_arg=(-g "${temp_dir}")
  fi

  local log
  log="$(mktemp --suffix=".vaultx.log")"

  local label="${mode}${batch:+-${batch}batch}  drive=$(drive_id "${final_drive}")"
  echo "" >&2
  echo "  → k=${k}  ${label}" >&2

  drop_caches

  local mem_arg=()
  if [[ "$mode" == "OOM" ]]; then
    local mem_gb
    if [[ "$batch" == "2" ]]; then
      mem_gb="${OOM2_MEM[$k]}"
    else
      mem_gb="${OOM4_MEM[$k]}"
    fi
    mem_arg=(-m "${mem_gb}")
  fi

  if ! "${BIN}" \
        -k "${k}" \
        "${temp_arg[@]}" \
        -f "${plots_dir}" \
        -t "${COMPUTE_THREADS}" \
        --threads_io "${IO_THREADS}" \
        "${mem_arg[@]}" 2>&1 | tee "${log}"; then
    echo "Error: vaultx failed for k=${k}" >&2
    rm -f "${log}"
    return 1
  fi

  # Drop caches again after the run to neutralise any lingering cache effects
  drop_caches

  parse_log "${log}" >> "${csv_file}"
  rm -f "${log}"
}

RUN_LIST=()

if [[ -z "$CLI_MODE" || "$CLI_MODE" == "IM" ]]; then
  RUN_LIST+=("IM:")
fi

if [[ -z "$CLI_MODE" || "$CLI_MODE" == "OOM" ]]; then
  if [[ -z "$CLI_BATCH" || "$CLI_BATCH" == "2" ]]; then
    RUN_LIST+=("OOM:2")
  fi
  if [[ -z "$CLI_BATCH" || "$CLI_BATCH" == "4" ]]; then
    RUN_LIST+=("OOM:4")
  fi
fi

if [[ ${#RUN_LIST[@]} -eq 0 ]]; then
  echo "Error: No experiments match the given -mode/-batch combination." >&2
  exit 1
fi

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

    if [[ "$exp_mode" == "IM" ]]; then
      csv_label="IM"
    else
      csv_label="OM_${exp_batch}batch"
    fi

    csv_file="${EXPERIMENTS_DIR}/k27-k32_${did}_${csv_label}.csv"

    echo "" >&2
    echo "============================================================" >&2
    echo " Experiment : ${csv_label}" >&2
    echo " Drive      : ${final_drive}" >&2
    if [[ "$exp_mode" == "OOM" ]]; then
      echo " Temp drive : ${temp_drive}" >&2
    fi
    echo " CSV        : ${csv_file}" >&2
    echo "============================================================" >&2

    # Write CSV header (creates / overwrites the file for this run)
    printf "%s\n" "${CSV_HEADER}" > "${csv_file}"

    for k in "${K_VALUES[@]}"; do
      run_once "${k}" "${exp_mode}" "${exp_batch}" "${final_drive}" "${temp_drive}" "${csv_file}"
    done

    echo "" >&2
    echo "  Results written → ${csv_file}" >&2
  done

  # Cleanup: delete plot and temp files from all drives after this experiment mode completes
  echo "" >&2
  echo "Cleaning up plots and temp directories for ${exp_mode}${exp_batch:+-${exp_batch}batch} mode..." >&2
  for (( ci=0; ci<n_final; ci++ )); do
    cleanup_final="${FINAL_DRIVES[$ci]}"
    if [[ $n_temp -eq 1 ]]; then
      cleanup_temp="${TEMP_DRIVES[0]}"
    else
      cleanup_temp="${TEMP_DRIVES[$ci]}"
    fi

    # Remove plot files from final drive
    if [[ -d "${cleanup_final}/plots" ]]; then
      rm -f "${cleanup_final}/plots"/*.plot 2>/dev/null || true
      echo "  Cleaned ${cleanup_final}/plots" >&2
    fi

    # Remove temp files from temp drive
    if [[ -d "${cleanup_temp}/temp" ]]; then
      rm -f "${cleanup_temp}/temp"/* 2>/dev/null || true
      echo "  Cleaned ${cleanup_temp}/temp" >&2
    fi
  done
  echo "Cleanup complete for ${exp_mode}${exp_batch:+-${exp_batch}batch} mode." >&2
done

echo "" >&2
echo "All experiments complete." >&2
echo "Results directory: ${EXPERIMENTS_DIR}" >&2
