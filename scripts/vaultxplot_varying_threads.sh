#!/usr/bin/env bash
# vaultxplot_varying_threads.sh
# Benchmarks vaultx plot generation by varying compute or IO threads (IM mode only).
# k32 is run fully in-memory; no memory limit is specified.
# Compute threads are swept over powers of 2 up to nproc.
# IO threads are swept over 1, 2, 4, 8.
# Also samples system power (ipmitool dcmi power reading, 1Hz) over each run's
# wall-clock duration, and a one-time idle baseline before the sweep starts, so
# results are directly comparable to newexperiments/*/madmax/madmaxvaryingthreads.sh
# (same sampling method and CSV power/energy columns).
#
# Usage:
#   ./vaultxplot_varying_threads.sh -thread CP
#   ./vaultxplot_varying_threads.sh -thread IO
#
# CSV naming convention:
#   varying_CP_k32_<drive>_IM.csv
#   varying_IO_k29_<drive>_IM.csv
#
# Idle baseline (one-time, before the sweep): written to
#   idle_power_<hostname>.txt in EXPERIMENTS_DIR.
set -euo pipefail


ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-${ROOT_DIR}/vaultx}"
EXPERIMENTS_DIR="${ROOT_DIR}/newexperiments/$(hostname)"

# --- SUDO PASSWORD (for ipmitool power reads; override via env if needed) ---
SUDO_PASS="${SUDO_PASS:-sfatunmbi}"

# --- IDLE BASELINE SAMPLING WINDOW (seconds) ---
IDLE_SAMPLE_SECONDS="${IDLE_SAMPLE_SECONDS:-60}"

FINAL_DRIVES=(
  "/data-l/sfatunmbi"
  "/ssd-raid0/sfatunmbi"
  "/sfatunmbi"
)

K_VALUES=(32)
# Source machine-local drive config if present (gitignored, pushed by gatherdata.sh -setup).
_drives_local="${ROOT_DIR}/scripts/.drives.local"
[[ -f "$_drives_local" ]] && source "$_drives_local"
unset _drives_local
# Allow the orchestrator to override drives at runtime via VAULTX_DRIVES=path1;path2
[[ -n "${VAULTX_DRIVES:-}" ]] && IFS=';' read -ra FINAL_DRIVES <<< "$VAULTX_DRIVES"

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

# =============================================================================
# POWER MONITOR (background; same method as madmaxvaryingthreads.sh)
# Polls ipmitool at 1Hz and appends each Watt reading to power_tmp until
# pid_file is removed.
# =============================================================================
power_monitor() {
  local power_tmp="$1"
  local pid_file="$2"

  > "${power_tmp}"

  while [[ -f "${pid_file}" ]]; do
    reading=$(echo "${SUDO_PASS}" | sudo -S ipmitool dcmi power reading 2>/dev/null \
        | grep -i "Instantaneous power reading" \
        | grep -oP '\d+(?=\s+Watts)')
    if [[ -n "${reading}" ]]; then
      echo "${reading}" >> "${power_tmp}"
    fi
    sleep 1
  done
}

# =============================================================================
# Reduce a file of 1Hz Watt readings to "min max avg total_wh total_kwh"
# given the wall-clock duration (seconds) the readings were collected over.
# Prints "N/A N/A N/A N/A N/A" if no readings were collected.
# =============================================================================
summarize_power() {
  local power_tmp="$1"
  local duration_s="$2"

  local count=0 total=0 min=999999 max=0
  while IFS= read -r w; do
    [[ "${w}" =~ ^[0-9]+$ ]] || continue
    count=$(( count + 1 ))
    total=$(( total + w ))
    (( w < min )) && min=$w
    (( w > max )) && max=$w
  done < "${power_tmp}"

  if [[ "${count}" -gt 0 ]]; then
    local avg=$(( total / count ))
    local wh kwh
    wh=$(awk "BEGIN { printf \"%.4f\", (${avg} * ${duration_s}) / 3600 }")
    kwh=$(awk "BEGIN { printf \"%.6f\", ${wh} / 1000 }")
    echo "${min} ${max} ${avg} ${wh} ${kwh}"
  else
    echo "N/A N/A N/A N/A N/A"
  fi
}

# =============================================================================
# ONE-TIME IDLE POWER BASELINE
# Samples power for IDLE_SAMPLE_SECONDS with no vaultx workload running, before
# the thread sweep starts. This is NOT the same as the min_power seen during a
# run (a busy process rarely idles all the way down to true baseline), and
# min_power varies across runs precisely because it is a workload trough, not
# a hardware idle floor -- so it has to be measured directly, once per machine.
# =============================================================================
capture_idle_baseline() {
  local out_file="${EXPERIMENTS_DIR}/idle_power_$(hostname).txt"
  if [[ -f "${out_file}" ]]; then
    echo "Idle baseline already captured: ${out_file} (delete it to re-sample)" >&2
    return 0
  fi

  echo "Sampling idle power baseline for ${IDLE_SAMPLE_SECONDS}s (no workload running)..." >&2
  local power_tmp sentinel
  power_tmp=$(mktemp /tmp/idle_power.XXXXXX)
  sentinel=$(mktemp /tmp/idle_sentinel.XXXXXX)

  power_monitor "${power_tmp}" "${sentinel}" &
  local mon_pid=$!
  sleep "${IDLE_SAMPLE_SECONDS}"
  rm -f "${sentinel}"
  wait "${mon_pid}" 2>/dev/null

  read -r min max avg wh kwh <<< "$(summarize_power "${power_tmp}" "${IDLE_SAMPLE_SECONDS}")"
  rm -f "${power_tmp}"

  {
    echo "host=$(hostname)"
    echo "sample_seconds=${IDLE_SAMPLE_SECONDS}"
    echo "min_power_w=${min}"
    echo "max_power_w=${max}"
    echo "avg_power_w=${avg}"
  } > "${out_file}"

  echo "Idle baseline: min=${min}W max=${max}W avg=${avg}W -> ${out_file}" >&2
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

CSV_HEADER="varying_threads,k,threads_compute,threads_io,file_size_gb,write_batch_mb,read_batch,storage_efficiency_pct,overall_io_throughput_mb_s,total_time_s,total_time_min,peak_memory_mb,total_throughput_mh_s,min_power_w,max_power_w,avg_power_w,total_energy_wh,total_energy_kwh"

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

  # --- Start power monitor in background ---
  local power_tmp power_sentinel
  power_tmp="$(mktemp /tmp/power_readings.XXXXXX)"
  power_sentinel="$(mktemp /tmp/power_sentinel.XXXXXX)"
  power_monitor "${power_tmp}" "${power_sentinel}" &
  local power_pid=$!

  # --- Run vaultx in background so wall-clock duration matches the power sample window ---
  local exp_start exp_end exp_duration
  exp_start=$(date +%s)

  "${BIN}" \
      -k "${k}" \
      -f "${plots_dir}" \
      -t "${compute_threads}" \
      -i "${io_threads}" \
      > "${log}" 2>&1 &
  local plot_pid=$!

  tail -f "${log}" &
  local tail_pid=$!

  local plot_exit=0
  wait "${plot_pid}" || plot_exit=$?

  exp_end=$(date +%s)
  exp_duration=$(( exp_end - exp_start ))

  # --- Stop monitors ---
  rm -f "${power_sentinel}"
  wait "${power_pid}" 2>/dev/null
  kill "${tail_pid}" 2>/dev/null
  wait "${tail_pid}" 2>/dev/null

  local min_w max_w avg_w total_wh total_kwh
  read -r min_w max_w avg_w total_wh total_kwh <<< "$(summarize_power "${power_tmp}" "${exp_duration}")"
  rm -f "${power_tmp}"

  if [[ "${plot_exit}" -ne 0 ]]; then
    echo "Warning: vaultx failed for k=${k} ${CLI_THREAD}=${num_threads}, skipping." >&2
    rm -f "${log}"
    cleanup_files "${final_drive}"
    return 1
  fi

  drop_caches

  local row
  row="$(parse_log "${log}" "${num_threads}")"
  echo "${row},${min_w},${max_w},${avg_w},${total_wh},${total_kwh}" >> "${csv_file}"
  rm -f "${log}"

  cleanup_files "${final_drive}"
}

capture_idle_baseline

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
