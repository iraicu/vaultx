#!/usr/bin/env bash
# chiapos_power_oneshot.sh
# One-time power-instrumented ChiaPoS K=32 plot run, for the IV-H economics
# comparison (VaultX vs. ChiaPoS 5-year cost). You already have real
# wall-clock/memory numbers for ChiaPoS on s8, EpycBox, Torus, and OPI5
# (newexperiments/<machine>/posresults*.txt / chiaposresults_nvme.txt) --
# what's missing is power, since none of those runs had ipmitool attached.
#
# All four existing runs show ChiaPoS pinned at 151-199% CPU regardless of
# machine thread count (it's I/O-bound, not compute-scaling), so a single
# steady-state power sample -- not a thread sweep -- is enough; there's no
# reason to expect power to vary much with -r on this plotter.
#
# This script re-runs the EXACT command already used to produce this
# machine's posresults.txt (paste it in below) wrapped with the same
# ipmitool power_monitor used by vaultxplot_varying_threads.sh and
# madmaxvaryingthreads.sh, so the numbers are directly comparable. It also
# picks up this machine's idle_power_<hostname>.txt baseline if
# vaultxplot_varying_threads.sh has already been run here.
#
# Usage:
#   1. Edit CHIAPOS_CMD below to the exact command from this machine's
#      posresults.txt / chiaposresults_*.txt (same -r, -t, -d, -c, -f).
#   2. ./chiapos_power_oneshot.sh
#
# Output: newexperiments/$(hostname)/chiapos_power_$(hostname).txt
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXPERIMENTS_DIR="${ROOT_DIR}/newexperiments/$(hostname)"
mkdir -p "${EXPERIMENTS_DIR}"

SUDO_PASS="${SUDO_PASS:-sfatunmbi}"

# =============================================================================
# EDIT THIS: paste the exact ProofOfSpace command already used on this
# machine (see newexperiments/<machine>/posresults*.txt for the one to copy).
# Example (s8, from chiaposresults_nvme.txt):
#   CHIAPOS_CMD=(./ProofOfSpace create -k 32 -n 1 -r 192 -u 128 \
#     -t /nvme-raid0/sfatunmbi/temp/ -2 /nvme-raid0/sfatunmbi/temp/ \
#     -d /nvme-raid0/sfatunmbi/plots/ \
#     -c xch1ta2vz9qddvlhn6sahade07nfe9vf86jz8v0k3p4n62208rl377esytyjx3 \
#     -f 8fa93bb7fe1808cb781955d810a3f49e46e66359252e8a8c07ae1b06755f115956d4d99479ffe19c8ee2c4295eee6f07.plot)
# =============================================================================
CHIAPOS_CMD=(./ProofOfSpace create -k 32 -n 1 -r "${THREADS:?set THREADS or edit CHIAPOS_CMD directly}" -u 128 \
  -t "${TEMP_DIR:?set TEMP_DIR}" -2 "${TEMP_DIR:?}" -d "${FINAL_DIR:?set FINAL_DIR}" \
  -c "${CONTRACT:?set CONTRACT}" -f "${PLOT_ID:?set PLOT_ID}.plot")

# =============================================================================
# POWER MONITOR (same method as vaultxplot_varying_threads.sh /
# madmaxvaryingthreads.sh: ipmitool dcmi power reading at 1Hz).
# =============================================================================
power_monitor() {
  local power_tmp="$1"
  local pid_file="$2"
  > "${power_tmp}"
  while [[ -f "${pid_file}" ]]; do
    reading=$(echo "${SUDO_PASS}" | sudo -S ipmitool dcmi power reading 2>/dev/null \
        | grep -i "Instantaneous power reading" \
        | grep -oP '\d+(?=\s+Watts)')
    [[ -n "${reading}" ]] && echo "${reading}" >> "${power_tmp}"
    sleep 1
  done
}

summarize_power() {
  local power_tmp="$1" duration_s="$2"
  local count=0 total=0 min=999999 max=0
  while IFS= read -r w; do
    [[ "${w}" =~ ^[0-9]+$ ]] || continue
    count=$(( count + 1 )); total=$(( total + w ))
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

echo "Running: ${CHIAPOS_CMD[*]}" >&2
echo "(This matches an existing multi-hour ChiaPoS run -- expect several hours.)" >&2

LOG="${EXPERIMENTS_DIR}/chiapos_power_$(hostname)_$(date +%Y%m%d_%H%M%S).log"
power_tmp=$(mktemp /tmp/power_readings.XXXXXX)
power_sentinel=$(mktemp /tmp/power_sentinel.XXXXXX)

power_monitor "${power_tmp}" "${power_sentinel}" &
power_pid=$!

exp_start=$(date +%s)
/usr/bin/time -v "${CHIAPOS_CMD[@]}" > "${LOG}" 2>&1 &
plot_pid=$!

plot_exit=0
wait "${plot_pid}" || plot_exit=$?
exp_end=$(date +%s)
exp_duration=$(( exp_end - exp_start ))

rm -f "${power_sentinel}"
wait "${power_pid}" 2>/dev/null

read -r min_w max_w avg_w total_wh total_kwh <<< "$(summarize_power "${power_tmp}" "${exp_duration}")"
rm -f "${power_tmp}"

idle_file="${EXPERIMENTS_DIR}/idle_power_$(hostname).txt"
idle_avg="N/A"
[[ -f "${idle_file}" ]] && idle_avg="$(awk -F= '/avg_power_w/{print $2}' "${idle_file}")"

OUT_FILE="${EXPERIMENTS_DIR}/chiapos_power_$(hostname).txt"
{
  echo "host=$(hostname)"
  echo "command=${CHIAPOS_CMD[*]}"
  echo "duration_s=${exp_duration}"
  echo "duration_min=$(awk "BEGIN { printf \"%.2f\", ${exp_duration} / 60 }")"
  echo "idle_power_w=${idle_avg}"
  echo "min_power_w=${min_w}"
  echo "max_power_w=${max_w}"
  echo "avg_power_w=${avg_w}"
  echo "total_energy_wh=${total_wh}"
  echo "total_energy_kwh=${total_kwh}"
  echo "plot_exit_code=${plot_exit}"
} | tee "${OUT_FILE}"

echo "" >&2
echo "Full time -v log: ${LOG}" >&2
echo "Summary written : ${OUT_FILE}" >&2
if [[ "${plot_exit}" -ne 0 ]]; then
  echo "Warning: ProofOfSpace exited non-zero (${plot_exit})." >&2
fi
