#!/usr/bin/env bash
# vaultxplot_varying_threads.sh
# Benchmarks vaultx plot generation by varying compute or IO threads (IM mode only).
# k32 is run fully in-memory; no memory limit is specified.
# Compute threads are swept over powers of 2 up to nproc.
# IO threads are swept over 1, 2, 4, 8.
#
# Usage:
#   ./vaultxplot_varying_threads.sh -thread CP
#   ./vaultxplot_varying_threads.sh -thread IO
#
# CSV naming convention:
#   varying_CP_k32_<drive>_IM.csv
#   varying_IO_k29_<drive>_IM.csv
set -euo pipefail


ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-${ROOT_DIR}/vaultx}"
EXPERIMENTS_DIR="${ROOT_DIR}/newexperiments/$(hostname)"

FINAL_DRIVES=(
  "/data-l/sfatunmbi"
  "/ssd-raid0/sfatunmbi"
  "/sfatunmbi"
)

# Source machine-local drive config if present (gitignored, pushed by gatherdata.sh -setup).
_drives_local="${ROOT_DIR}/scripts/.drives.local"
[[ -f "$_drives_local" ]] && source "$_drives_local"
unset _drives_local
# Allow the orchestrator to override drives at runtime via VAULTX_DRIVES=path1;path2
[[ -n "${VAULTX_DRIVES:-}" ]] && IFS=';' read -ra FINAL_DRIVES <<< "$VAULTX_DRIVES"

K_VALUES=(29 32)

CLI_THREAD="CP"   # IO support removed — only compute threads are varied

usage() {
  cat <<'EOF'
Usage: vaultxplot_varying_threads.sh -thread IO|CP [-h]

Options:
  -thread IO|CP      Thread type to vary (required)
                       IO = vary I/O threads (-i), keep compute threads at nproc
                       CP = vary compute threads (-t), keep I/O threads at 1
  -h                 Show this help and exit

Runs all k-values fully in-memory (no memory cap, no temp directory).
Compute threads sweep powers of 2 from 1 up to nproc.
IO threads sweep 1, 2, 4, 8.
Caches are cleared and generated plot files deleted after every run.

CSV naming:
  varying_<threadtype>_k<k>_<drive>_IM.csv
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -thread)
      CLI_THREAD="${2:-}"
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

if [[ "$CLI_THREAD" != "CP" ]]; then
  echo "Error: only -thread CP is supported" >&2
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

mkdir -p "${EXPERIMENTS_DIR}"

MAX_THREADS="$(nproc)"

# Powers of 2 from 1 up to max; appends max if it is not itself a power of 2.
generate_thread_counts() {
  local max="$1"
  local -a raw=()
  local val=1
  while (( val <= max )); do
    raw+=("$val")
    val=$(( val * 2 ))
  done
  raw+=("$max")
  printf '%s\n' "${raw[@]}" | sort -n -u
}

# IO_THREAD_COUNTS removed — IO thread variation is not used.

COMPUTE_THREAD_COUNTS=()
while IFS= read -r t; do
  COMPUTE_THREAD_COUNTS+=("$t")
done < <(generate_thread_counts "$MAX_THREADS")

THREAD_COUNTS=("${COMPUTE_THREAD_COUNTS[@]}")
CONSTANT_THREADS=1

echo "Max threads (nproc): ${MAX_THREADS}" >&2
echo "Thread counts to test: ${THREAD_COUNTS[*]}" >&2
echo "Total runs per k-value per drive: ${#THREAD_COUNTS[@]}" >&2

drop_caches() {
  if echo "sfatunmbi" | sudo -S sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null; then
    return 0
  fi
  echo "Warning: Failed to drop caches via sudo; falling back to sync only" >&2
  sync
}

drive_id() { echo "$1" | cut -d'/' -f2; }

extract_field() {
  local pattern="$1" file="$2"
  sed -n "${pattern}" "${file}" 2>/dev/null | head -1 || true
}

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
  if [[ -d "${final_drive}/plots" ]]; then
    rm -f "${final_drive}/plots"/*.plot 2>/dev/null || true
  fi
}

CSV_HEADER="varying_threads,k,threads_compute,threads_io,file_size_gb,write_batch_mb,read_batch,storage_efficiency_pct,overall_io_throughput_mb_s,total_time_s,total_time_min,peak_memory_mb,total_throughput_mh_s"

run_once() {
  local k="$1"
  local num_threads="$2"
  local final_drive="$3"
  local csv_file="$4"

  local plots_dir="${final_drive}/plots"
  mkdir -p "${plots_dir}"

  local compute_threads io_threads
  if [[ "$CLI_THREAD" == "IO" ]]; then
    compute_threads="$CONSTANT_THREADS"
    io_threads="${num_threads}"
  else
    compute_threads="${num_threads}"
    io_threads="$CONSTANT_THREADS"
  fi

  local log
  log="$(mktemp --suffix=".vaultx.log")"

  echo "" >&2
  echo "  → k=${k}  varying_${CLI_THREAD}=${num_threads}  compute=${compute_threads}  io=${io_threads}" >&2

  drop_caches

  if ! "${BIN}" \
        -k "${k}" \
        -f "${plots_dir}" \
        -t "${compute_threads}" \
        -i "${io_threads}" \
        2>&1 | tee "${log}"; then
    echo "Warning: vaultx failed for k=${k} ${CLI_THREAD}=${num_threads}, skipping." >&2
    rm -f "${log}"
    cleanup_files "${final_drive}"
    return 1
  fi

  drop_caches

  parse_log "${log}" "${num_threads}" >> "${csv_file}"
  rm -f "${log}"

  cleanup_files "${final_drive}"
}

n_final="${#FINAL_DRIVES[@]}"

for (( di=0; di<n_final; di++ )); do
  final_drive="${FINAL_DRIVES[$di]}"
  did="$(drive_id "${final_drive}")"

  for k in "${K_VALUES[@]}"; do
    csv_file="${EXPERIMENTS_DIR}/varying_${CLI_THREAD}_k${k}_${did}_IM.csv"

    echo "" >&2
    echo "============================================================" >&2
    echo " Experiment  : Varying ${CLI_THREAD} threads (IM)" >&2
    echo " k           : ${k}" >&2
    echo " Drive       : ${final_drive}" >&2
    echo " CSV         : ${csv_file}" >&2
    echo " Thread sweep: ${THREAD_COUNTS[*]}" >&2
    echo "============================================================" >&2

    printf "%s\n" "${CSV_HEADER}" > "${csv_file}"

    for tc in "${THREAD_COUNTS[@]}"; do
      run_once "${k}" "${tc}" "${final_drive}" "${csv_file}" || \
        echo "Warning: Skipping failed experiment k=${k} ${CLI_THREAD}=${tc} drive=${final_drive}" >&2
    done

    echo "" >&2
    echo "  Results written → ${csv_file}" >&2
  done
done

echo "" >&2
echo "All experiments complete." >&2
echo "Results directory: ${EXPERIMENTS_DIR}" >&2
