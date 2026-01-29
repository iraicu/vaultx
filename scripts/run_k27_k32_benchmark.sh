#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-${ROOT_DIR}/vaultx}"
PLOTS_DIR="/data-m/sfatunmbi/plots"
TMP_DIR="/data-m/sfatunmbi/tmp"
DATA_DIR="/data-m/sfatunmbi/data"
CSV_OUT="${DATA_DIR}/gen_k27_k32_$(date +%Y%m%d_%H%M%S).csv"

usage() {
  cat <<'EOF'
Usage: run_k27_k32_benchmark.sh [-t TMP_DIR] [-f FINAL_DIR] [-mode IM|OOM]

Options:
  -t TMP_DIR     Temporary directory for intermediate files (passed to -g)
  -f FINAL_DIR   Final directory for plot outputs (passed to -f)
  -mode IM|OOM   IM = in-memory (no -m); OOM = out-of-memory, set -m per-k

Environment overrides: BIN, PLOTS_DIR, TMP_DIR, DATA_DIR, CSV_OUT
EOF
}

MODE="IM"

# Simple manual arg parser to allow -mode
while [[ $# -gt 0 ]]; do
  case "$1" in
    -t)
      TMP_DIR="$2"
      shift 2
      ;;
    -f)
      PLOTS_DIR="$2"
      shift 2
      ;;
    -mode)
      MODE="$2"
      shift 2
      ;;
    -h)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    *)
      usage >&2
      exit 1
      ;;
  esac
done

if [[ "$MODE" != "IM" && "$MODE" != "OOM" ]]; then
  echo "Error: -mode must be IM or OOM" >&2
  usage >&2
  exit 1
fi

    mkdir -p "${PLOTS_DIR}" "${TMP_DIR}" "${DATA_DIR}"

if [[ ! -x "${BIN}" ]]; then
  echo "Error: vaultx binary not found or not executable at ${BIN}" >&2
  exit 1
fi

# Drop caches to minimize cross-run noise; best-effort if sudo -n fails.
drop_caches() {
  if command -v sudo >/dev/null 2>&1 && sudo -n true >/dev/null 2>&1; then
    sudo sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches'
  else
    echo "Warning: sudo not available (or needs password); skipping drop_caches" >&2
    sync
  fi
}

# Extract single numeric field with a sed pattern; empty if missing.
extract_sed() {
  local pattern="$1" file="$2"
  sed -n "${pattern}" "${file}" || true
}

# Parse a vaultx generation log into CSV fields.
parse_log() {
  local log="$1"
  local k_value compute_threads io_threads file_size_gb write_batch_mb read_batch
  local storage_eff overall_io_mb_s total_time_s peak_mem_mb total_throughput_mh_s

  k_value=$(extract_sed 's/Exponent k[[:space:]]*:[[:space:]]*\([0-9]\+\)/\1/p' "${log}")
  compute_threads=$(extract_sed 's/Threads (Hash\/Sort)[[:space:]]*:[[:space:]]*\([0-9]\+\)/\1/p' "${log}")
  io_threads=$(extract_sed 's/Threads (I\/O)[[:space:]]*:[[:space:]]*\([0-9]\+\)/\1/p' "${log}")
  file_size_gb=$(extract_sed 's/File Size (GB)[[:space:]]*:[[:space:]]*\([0-9.]*\)/\1/p' "${log}")
  write_batch_mb=$(extract_sed 's/WRITE_BATCH_SIZE[[:space:]]*:[[:space:]]*\([0-9]\+\)/\1/p' "${log}")
  read_batch=$(extract_sed 's/READ_BATCH_SIZE[[:space:]]*:[[:space:]]*\([0-9]\+\)/\1/p' "${log}")
  storage_eff=$(extract_sed 's/.*storage_efficiency_table2=\([0-9.]*\).*/\1/p' "${log}")
  overall_io_mb_s=$(extract_sed 's/Overall I\/O Throughput:[[:space:]]*\([0-9.]*\)[[:space:]]*MB\/s.*/\1/p' "${log}")
  total_time_s=$(extract_sed 's/Total Time:[[:space:]]*\([0-9.]*\).*/\1/p' "${log}")
  peak_mem_mb=$(extract_sed 's/Peak Memory Usage:[[:space:]]*\([0-9.]*\)[[:space:]]*MB.*/\1/p' "${log}")
  total_throughput_mh_s=$(extract_sed 's/Total Throughput:[[:space:]]*\([0-9.]*\)[[:space:]]*MH\/s.*/\1/p' "${log}")

  echo "${k_value:-NA},${compute_threads:-NA},${io_threads:-NA},${file_size_gb:-NA},${write_batch_mb:-NA},${read_batch:-NA},${storage_eff:-NA},${overall_io_mb_s:-NA},${total_time_s:-NA},${peak_mem_mb:-NA},${total_throughput_mh_s:-NA}"
}

# Write CSV header
printf "k,threads_compute,threads_io,file_size_gb,write_batch_mb,read_batch,storage_efficiency_pct,overall_io_throughput_mb_s,total_time_s,peak_memory_mb,total_throughput_mh_s\n" > "${CSV_OUT}"

run_once() {
  local k="$1"
  local log
  log="$(mktemp)"

  local mem_arg=()
  if [[ "$MODE" == "OOM" ]]; then
    # Per-k memory (GB) to force ~2 rounds: 27->2.5, 28->3, 29->4.5, 30->7.5, 31->13.5, 32->25.5
    local mem_gb
    case "$k" in
      27) mem_gb=2.5 ;;
      28) mem_gb=3 ;;
      29) mem_gb=4.5 ;;
      30) mem_gb=7.5 ;;
      31) mem_gb=13.5 ;;
      32) mem_gb=25.5 ;;
      *) mem_gb=0 ;;
    esac
    # Convert GB -> MB (integer) for the -m flag expected by vaultx
    local mem_mb
    mem_mb=$(awk "BEGIN{printf(\"%d\", ${mem_gb}*1024)}")
    mem_arg=(-m "${mem_mb}")
  fi

  echo "=== Running k=${k} ===" >&2
  drop_caches
  # Use long form for I/O threads to avoid getopt issues with short -i parsing.
  if ! "${BIN}" -k "${k}" -g "${TMP_DIR}" -f "${PLOTS_DIR}" -t 40 --threads_io 1 "${mem_arg[@]}" | tee "${log}"; then
    echo "Error: vaultx run failed for k=${k}" >&2
    rm -f "${log}"
    return 1
  fi
  drop_caches

  parse_log "${log}" >> "${CSV_OUT}"
  rm -f "${log}"
}

for k in 27 28 29 30 31 32; do
  run_once "${k}"
done

echo "CSV written to ${CSV_OUT}" >&2
