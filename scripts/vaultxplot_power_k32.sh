#!/usr/bin/env bash
# vaultxplot_power_k32.sh
#
# Wall-power benchmark for vaultx: k=32 only, run fully in-memory, fixed at
# this machine's max compute thread count (nproc) with IO threads held at 1
# -- no thread sweep. The only things that vary are drive (FINAL_DRIVES --
# set it to a single entry to stay on one drive), N (repetitions per drive),
# and SHUTDOWN.
#
# This is meant to be read against an EXTERNAL WALL POWER METER, not just the
# internal ipmitool numbers. Watch for the ">>> WALL METER <<<" banners
# telling you exactly when to note the meter's reading. The ipmitool-derived
# power/energy columns in the CSV (and the idle baseline) are a supplementary
# automated cross-check only -- they are not the number of record.
#
# SHUTDOWN=true powers the machine off after the last run so nothing keeps
# drawing power afterwards -- the meter's post-shutdown reading is then the
# true, final total for the whole session (no idle-time accumulation to
# second-guess).
#
# Derived from scripts/vaultxplot_varying_threads.sh (same power_monitor /
# summarize_power / idle-baseline methodology, same log-parsing), with the
# thread sweep collapsed to a single max-threads value and repetition/
# shutdown support added.
#
# Usage:
#   ./vaultxplot_power_k32.sh
#   N=3 SHUTDOWN=true ./vaultxplot_power_k32.sh
#
# CSV naming convention:
#   power_k32_<drive>_IM.csv
set -euo pipefail


ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-${ROOT_DIR}/vaultx}"
EXPERIMENTS_DIR="${ROOT_DIR}/newexperiments/$(hostname)/vaultx_power"

# --- SUDO PASSWORD (for ipmitool power reads and shutdown; override via env) ---
SUDO_PASS="${SUDO_PASS:-sfatunmbi}"

# --- IDLE BASELINE SAMPLING WINDOW (seconds) ---
IDLE_SAMPLE_SECONDS="${IDLE_SAMPLE_SECONDS:-60}"

# --- WHAT TO VARY ---
K=32

# Number of k32 plots to make per drive (repetitions).
N="${N:-1}"

# Set to a single entry to keep everything on one drive.
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

# Shut the machine down once the benchmark finishes?
#   true  -> auto power-off after the last run, so power stops accumulating
#            and the meter's post-shutdown reading is stable/final.
#   false -> leave the machine running/idle; note the meter reading yourself.
SHUTDOWN="${SHUTDOWN:-false}"
SHUTDOWN_DELAY_S="${SHUTDOWN_DELAY_S:-15}"   # grace period to Ctrl+C before it fires

COMPUTE_THREADS="$(nproc)"   # fixed at this machine's max thread count
IO_THREADS=1                 # constant, matching the other vaultxplot_*.sh scripts

if [[ ! -x "${BIN}" ]]; then
  echo "Error: vaultx binary not found or not executable at ${BIN}" >&2
  exit 1
fi

if [[ ${#FINAL_DRIVES[@]} -eq 0 ]]; then
  echo "Error: FINAL_DRIVES array is empty" >&2
  exit 1
fi

if ! [[ "$N" =~ ^[0-9]+$ ]] || [ "$N" -lt 1 ]; then
  echo "Error: N must be a positive integer (got '$N')." >&2
  exit 1
fi

if [[ "$SHUTDOWN" != "true" && "$SHUTDOWN" != "false" ]]; then
  echo "Error: SHUTDOWN must be 'true' or 'false' (got '$SHUTDOWN')." >&2
  exit 1
fi

mkdir -p "${EXPERIMENTS_DIR}"

echo "Max threads (nproc)  : ${COMPUTE_THREADS}" >&2
echo "IO threads (constant): ${IO_THREADS}" >&2
echo "Reps per drive (N)   : ${N}" >&2
echo "Drives               : ${FINAL_DRIVES[*]}" >&2
echo "Shutdown after       : ${SHUTDOWN}" >&2

drop_caches() {
  if echo "${SUDO_PASS}" | sudo -S sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null; then
    return 0
  fi
  echo "Warning: Failed to drop caches via sudo; falling back to sync only" >&2
  sync
}

# =============================================================================
# POWER MONITOR (background; same method as madmax_power_k32.sh /
# vaultxplot_varying_threads.sh). Supplementary automated cross-check -- the
# wall meter is the number of record.
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
# Samples power for IDLE_SAMPLE_SECONDS with no vaultx workload running,
# before the benchmark starts. Supplementary only -- see idle_power_*.txt.
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

# =============================================================================
# COUNTDOWN (log-friendly, no \r) -- Ctrl+C during this cancels whatever it's
# guarding (e.g. shutdown) since sleep just dies with the script.
# =============================================================================
countdown() {
  local secs="$1" msg="$2"
  echo "${msg} (Ctrl+C to cancel)" >&2
  while (( secs > 0 )); do
    echo "  ...${secs}s" >&2
    local step=5
    (( secs < step )) && step=$secs
    sleep "${step}"
    secs=$(( secs - step ))
  done
}

drive_id() { echo "$1" | cut -d'/' -f2; }

extract_field() {
  local pattern="$1" file="$2"
  sed -n "${pattern}" "${file}" 2>/dev/null | head -1 || true
}

parse_log() {
  local log="$1"
  local rep="$2"

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

  echo "${rep},${k_value:-NA},${compute_threads:-NA},${io_threads:-NA},${file_size_gb:-NA},${write_batch_mb:-NA},${read_batch:-NA},${storage_eff:-NA},${overall_io_mb_s:-NA},${total_time_s:-NA},${total_time_min},${peak_mem_mb:-NA},${total_throughput_mh_s:-NA}"
}

cleanup_files() {
  local final_drive="$1"
  if [[ -d "${final_drive}/plots" ]]; then
    rm -f "${final_drive}/plots"/*.plot 2>/dev/null || true
  fi
}

CSV_HEADER="rep,k,threads_compute,threads_io,file_size_gb,write_batch_mb,read_batch,storage_efficiency_pct,overall_io_throughput_mb_s,total_time_s,total_time_min,peak_memory_mb,total_throughput_mh_s,min_power_w,max_power_w,avg_power_w,total_energy_wh,total_energy_kwh,start_time,end_time"

run_once() {
  local k="$1"
  local rep="$2"
  local final_drive="$3"
  local csv_file="$4"

  local plots_dir="${final_drive}/plots"
  mkdir -p "${plots_dir}"

  local log
  log="$(mktemp --suffix=".vaultx.log")"

  echo "" >&2
  echo "  → k=${k}  rep=${rep}/${N}  compute=${COMPUTE_THREADS}  io=${IO_THREADS}" >&2

  drop_caches

  # --- Start power monitor in background ---
  local power_tmp power_sentinel
  power_tmp="$(mktemp /tmp/power_readings.XXXXXX)"
  power_sentinel="$(mktemp /tmp/power_sentinel.XXXXXX)"
  power_monitor "${power_tmp}" "${power_sentinel}" &
  local power_pid=$!

  local exp_start exp_end exp_duration start_time end_time
  start_time="$(date +"%Y-%m-%d %H:%M:%S")"
  exp_start=$(date +%s)

  "${BIN}" \
      -k "${k}" \
      -f "${plots_dir}" \
      -t "${COMPUTE_THREADS}" \
      -i "${IO_THREADS}" \
      > "${log}" 2>&1 &
  local plot_pid=$!

  # stdout of this function is captured via $(...) by the caller (to get the
  # run duration back), so tail must go to stderr, not stdout.
  tail -f "${log}" >&2 &
  local tail_pid=$!

  local plot_exit=0
  wait "${plot_pid}" || plot_exit=$?

  end_time="$(date +"%Y-%m-%d %H:%M:%S")"
  exp_end=$(date +%s)
  exp_duration=$(( exp_end - exp_start ))

  rm -f "${power_sentinel}"
  wait "${power_pid}" 2>/dev/null
  kill "${tail_pid}" 2>/dev/null
  wait "${tail_pid}" 2>/dev/null

  local min_w max_w avg_w total_wh total_kwh
  read -r min_w max_w avg_w total_wh total_kwh <<< "$(summarize_power "${power_tmp}" "${exp_duration}")"
  rm -f "${power_tmp}"

  if [[ "${plot_exit}" -ne 0 ]]; then
    echo "Warning: vaultx failed for k=${k} rep=${rep}, skipping." >&2
    rm -f "${log}"
    cleanup_files "${final_drive}"
    return 1
  fi

  drop_caches

  local row
  row="$(parse_log "${log}" "${rep}")"
  echo "${row},${min_w},${max_w},${avg_w},${total_wh},${total_kwh},${start_time},${end_time}" >> "${csv_file}"
  rm -f "${log}"

  cleanup_files "${final_drive}"
  echo "${exp_duration}"
}

WALL_NOTE_FILE="${EXPERIMENTS_DIR}/wall_meter_notes_$(date +%Y%m%d_%H%M%S).txt"
TOTAL_RUNS=$(( N * ${#FINAL_DRIVES[@]} ))

{
  echo "=============================================="
  echo " VAULTX K32 WALL-POWER BENCHMARK"
  echo " Host                : $(hostname)"
  echo " Compute threads (-t): ${COMPUTE_THREADS} (nproc)"
  echo " IO threads (-i)     : ${IO_THREADS}"
  echo " Drives              : ${#FINAL_DRIVES[@]}"
  echo " Reps per drive      : ${N}"
  echo " Total runs          : ${TOTAL_RUNS}"
  echo " Shutdown after      : ${SHUTDOWN}"
  echo " Start time          : $(date)"
  echo " Logs saved to       : ${EXPERIMENTS_DIR}"
  echo "=============================================="
  echo ""
  echo ">>> WALL METER: note the STARTING reading now. <<<"
} | tee "${WALL_NOTE_FILE}" >&2

countdown 8 "Starting benchmark in"

capture_idle_baseline

GRAND_DURATION_S=0

for final_drive in "${FINAL_DRIVES[@]}"; do
  did="$(drive_id "${final_drive}")"
  csv_file="${EXPERIMENTS_DIR}/power_k${K}_${did}_IM.csv"

  echo "" >&2
  echo "============================================================" >&2
  echo " Drive : ${final_drive}" >&2
  echo " CSV   : ${csv_file}" >&2
  echo "============================================================" >&2

  printf "%s\n" "${CSV_HEADER}" > "${csv_file}"

  for (( rep=1; rep<=N; rep++ )); do
    run_duration="$(run_once "${K}" "${rep}" "${final_drive}" "${csv_file}")" && \
      GRAND_DURATION_S=$(( GRAND_DURATION_S + run_duration )) || \
      echo "Warning: Skipping failed run rep=${rep} drive=${final_drive}" >&2
  done

  echo "" >&2
  echo "  Results written → ${csv_file}" >&2
done

{
  echo ""
  echo "=============================================="
  echo " ALL ${TOTAL_RUNS} RUNS COMPLETE"
  echo " End time        : $(date)"
  echo " Total wall time : ${GRAND_DURATION_S}s ($(( GRAND_DURATION_S / 60 ))m)"
  echo " Results dir     : ${EXPERIMENTS_DIR}"
  echo "=============================================="
  echo ""
  if [[ "${SHUTDOWN}" == "true" ]]; then
    echo ">>> WALL METER: machine is about to shut down. Note the FINAL"
    echo "    reading once it is fully powered off. <<<"
  else
    echo ">>> WALL METER: note the FINAL reading now. <<<"
  fi
} | tee -a "${WALL_NOTE_FILE}" >&2

if [[ "${SHUTDOWN}" == "true" ]]; then
  countdown "${SHUTDOWN_DELAY_S}" "Shutting down in"
  echo "${SUDO_PASS}" | sudo -S shutdown -h now
fi
