#!/usr/bin/env bash
# vaultxplot_varying_memory.sh
# Benchmarks vaultx plot generation by varying memory allocation.
# Memory size is reported in the first column of the CSV output.
#
# The script automatically detects available system memory and caps
# the memory sizes to run. For example, if max available is 7.5 GB,
# it runs memory sizes 2, 4, 7 (rounded down).
#
# Usage:
#   ./vaultxplot_varying_memory.sh
#
# No flags required - just edit the configurable arrays at the top.
#
# CSV naming convention:
#   varying_memory_k<k>_CP<cp>_IO<io>_<drive>.csv
set -euo pipefail


ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-${ROOT_DIR}/vaultx}"
EXPERIMENTS_DIR="${ROOT_DIR}/newexperiments/$(hostname)"

# Memory sizes to test (GB).
MEMORY_SIZES=(2 2.5 3 5 8 14 26 50)

# K values to test
K_VALUES=(32)

# Compute thread counts to test
COMPUTE_THREAD_COUNTS=($(nproc))

# IO thread counts (array for extensibility)
IO_THREAD_COUNTS=(1)

FINAL_DRIVES=(
  "/stor/auxiliary/sfatunmbi"
  "/ssd-raid0/sfatunmbi"
  "/data-fast/sfatunmbi"
)

TEMP_DRIVES=(
  "/stor/auxiliary/sfatunmbi"
  "/ssd-raid0/sfatunmbi"
  "/data-fast/sfatunmbi"
)


if [[ ! -x "${BIN}" ]]; then
  echo "Error: vaultx binary not found or not executable at ${BIN}" >&2
  exit 1
fi

if [[ ${#FINAL_DRIVES[@]} -eq 0 ]]; then
  echo "Error: FINAL_DRIVES array is empty" >&2
  exit 1
fi

mkdir -p "${EXPERIMENTS_DIR}"

get_available_memory_gb() {
  # Get available memory in GB from /proc/meminfo
  local avail_kb
  avail_kb=$(grep -i '^MemAvailable:' /proc/meminfo | awk '{print $2}')
  if [[ -z "$avail_kb" ]]; then
    # Fallback to MemFree + Buffers + Cached if MemAvailable not present
    local mem_free buffers cached
    mem_free=$(grep -i '^MemFree:' /proc/meminfo | awk '{print $2}')
    buffers=$(grep -i '^Buffers:' /proc/meminfo | awk '{print $2}')
    cached=$(grep -i '^Cached:' /proc/meminfo | awk '{print $2}')
    avail_kb=$(( mem_free + buffers + cached ))
  fi
  # Convert KB to GB (with one decimal)
  awk "BEGIN { printf \"%.1f\", ${avail_kb} / 1024 / 1024 }"
}

filter_memory_sizes() {
  local max_mem_gb="$1"
  local max_mem_int
  max_mem_int=$(awk "BEGIN { printf \"%d\", ${max_mem_gb} }")
  
  local -a filtered=()
  for mem in "${MEMORY_SIZES[@]}"; do
    if (( mem <= max_mem_int )); then
      filtered+=("$mem")
    fi
  done
  
  # If max_mem_int is not in MEMORY_SIZES but is > min(MEMORY_SIZES), add it
  # This handles cases like 7.5 GB -> add 7 if not present
  local found=false
  for mem in "${filtered[@]:-}"; do
    if [[ "$mem" == "$max_mem_int" ]]; then
      found=true
      break
    fi
  done
  
  if ! $found && (( max_mem_int >= 2 )); then
    # Only add if it's larger than the largest filtered value
    local largest_filtered=0
    for mem in "${filtered[@]:-}"; do
      if (( mem > largest_filtered )); then
        largest_filtered=$mem
      fi
    done
    if (( max_mem_int > largest_filtered )); then
      filtered+=("$max_mem_int")
    fi
  fi
  
  # Sort and output
  printf '%s\n' "${filtered[@]:-}" | sort -n -u
}

AVAILABLE_MEM_GB=$(get_available_memory_gb)
echo "Detected available memory: ${AVAILABLE_MEM_GB} GB" >&2

FILTERED_MEMORY_SIZES=()
while IFS= read -r m; do
  [[ -n "$m" ]] && FILTERED_MEMORY_SIZES+=("$m")
done < <(filter_memory_sizes "$AVAILABLE_MEM_GB")

if [[ ${#FILTERED_MEMORY_SIZES[@]} -eq 0 ]]; then
  echo "Error: No memory sizes available. System has ${AVAILABLE_MEM_GB} GB available, minimum required is 2 GB." >&2
  exit 1
fi

echo "Memory sizes to test: ${FILTERED_MEMORY_SIZES[*]}" >&2
echo "Total memory configurations: ${#FILTERED_MEMORY_SIZES[@]}" >&2


# Flush page-cache for clean, reproducible timing
drop_caches() {
  if echo "sfatunmbi" | sudo -S sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null; then
    return 0
  fi
  echo "Warning: Failed to drop caches via sudo; falling back to sync only" >&2
  sync
}

# Extract the drive name (first path component after /)
drive_id() { echo "$1" | cut -d'/' -f2; }

extract_field() {
  local pattern="$1" file="$2"
  sed -n "${pattern}" "${file}" 2>/dev/null | head -1 || true
}

# Parse a vaultx log file and emit a single CSV row
# First column is the memory allocation size
parse_log() {
  local log="$1"
  local memory_gb="$2"

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

  echo "${memory_gb},${k_value:-NA},${compute_threads:-NA},${io_threads:-NA},${file_size_gb:-NA},${write_batch_mb:-NA},${read_batch:-NA},${storage_eff:-NA},${overall_io_mb_s:-NA},${total_time_s:-NA},${total_time_min},${peak_mem_mb:-NA},${total_throughput_mh_s:-NA}"
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

# CSV HEADER

CSV_HEADER="memory_gb,k,threads_compute,threads_io,file_size_gb,write_batch_mb,read_batch,storage_efficiency_pct,overall_io_throughput_mb_s,total_time_s,total_time_min,peak_memory_mb,total_throughput_mh_s"


run_once() {
  local k="$1"
  local memory_gb="$2"
  local compute_threads="$3"
  local io_threads="$4"
  local final_drive="$5"
  local temp_drive="$6"
  local csv_file="$7"

  local plots_dir="${final_drive}/plots"
  mkdir -p "${plots_dir}"

  # Temp directory for OOM runs
  local temp_dir="${temp_drive}/temp"
  mkdir -p "${temp_dir}"

  # Memory argument
  local mem_arg=(-m "${memory_gb}")

  local log
  log="$(mktemp --suffix=".vaultx.log")"

  echo "" >&2
  echo "  → k=${k}  memory=${memory_gb}GB  compute=${compute_threads}  io=${io_threads}" >&2

  # Clear caches before
  drop_caches

  if ! "${BIN}" \
        -k "${k}" \
        -g "${temp_dir}" \
        -f "${plots_dir}" \
        -t "${compute_threads}" \
        -i "${io_threads}" \
        "${mem_arg[@]}" 2>&1 | tee "${log}"; then
    echo "Error: vaultx failed for k=${k} memory=${memory_gb}GB" >&2
    rm -f "${log}"
    cleanup_files "${final_drive}" "${temp_drive}"
    return 1
  fi

  drop_caches

  parse_log "${log}" "${memory_gb}" >> "${csv_file}"
  rm -f "${log}"

  cleanup_files "${final_drive}" "${temp_drive}"
}

n_final="${#FINAL_DRIVES[@]}"
n_temp="${#TEMP_DRIVES[@]}"

for (( di=0; di<n_final; di++ )); do
  final_drive="${FINAL_DRIVES[$di]}"
  # Temp drive: 1-to-1 when counts match, single shared drive otherwise
  if [[ $n_temp -eq 1 ]]; then
    temp_drive="${TEMP_DRIVES[0]}"
  else
    temp_drive="${TEMP_DRIVES[$di]}"
  fi
  did="$(drive_id "${final_drive}")"

  for k in "${K_VALUES[@]}"; do
    for cp_threads in "${COMPUTE_THREAD_COUNTS[@]}"; do
      for io_threads in "${IO_THREAD_COUNTS[@]}"; do
        
        # CSV filename: varying_memory_k<k>_CP<cp>_IO<io>_<drive>.csv
        csv_file="${EXPERIMENTS_DIR}/varying_memory_k${k}_CP${cp_threads}_IO${io_threads}_${did}.csv"

        echo "" >&2
        echo "============================================================" >&2
        echo " Experiment  : Varying Memory" >&2
        echo " k           : ${k}" >&2
        echo " CP threads  : ${cp_threads}" >&2
        echo " IO threads  : ${io_threads}" >&2
        echo " Drive       : ${final_drive}" >&2
        echo " Temp drive  : ${temp_drive}" >&2
        echo " CSV         : ${csv_file}" >&2
        echo " Memory sweep: ${FILTERED_MEMORY_SIZES[*]} GB" >&2
        echo "============================================================" >&2

        # Write CSV header (creates / overwrites the file)
        printf "%s\n" "${CSV_HEADER}" > "${csv_file}"

        for mem in "${FILTERED_MEMORY_SIZES[@]}"; do
          run_once "${k}" "${mem}" "${cp_threads}" "${io_threads}" \
                   "${final_drive}" "${temp_drive}" "${csv_file}" || \
            echo "Warning: experiment failed for k=${k} memory=${mem}GB drive=${final_drive}, skipping." >&2
        done

        echo "" >&2
        echo "  ✓ Results written → ${csv_file}" >&2
      done
    done
  done
done

echo "" >&2
echo "All experiments complete." >&2
echo "Results directory: ${EXPERIMENTS_DIR}" >&2
