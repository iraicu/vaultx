#!/bin/bash

# =============================================================================
# BLADEBIT CHIA PLOTTING EXPERIMENT SCRIPT
# Varies cache (RAM) size across drive pairs with constant thread count.
# Logs output, power, and peak memory per run; writes results to CSV.
# =============================================================================

# --- FARMER KEY & CONTRACT ADDRESS ---
FARMER_KEY="96854d93efe790622d1a872ae3055eee65b2908bc3a55c203e1b81021179d797dfa219303a173808d76f15ce38efc1fa"
CONTRACT="xch1ta2vz9qddvlhn6sahade07nfe9vf86jz8v0k3p4n62208rl377esytyjx3"

# --- CONSTANT PARAMS ---
K=32
N_THREADS=$(nproc)   # f1-threads and fp-threads both set to this

# --- CACHE (MEMORY) SIZES TO TEST (add/remove as needed) ---
MEMORY_SIZES=("4G" "8G" "16G" "32G" "64G" "128G")

# --- DRIVE PAIRS (temp[i] paired with final[i], must be same length) ---
# Each temp dir is used for both -t1 and -t2 in bladebit diskplot.
TEMP_DRIVES=(
    "/data-fast/sfatunmbi/temp"
)
FINAL_DRIVES=(
    "/data-fast/sfatunmbi/plots"
)

# --- DESTINATION FOR FINISHED PLOTS ---
CEPH_DEST="/ceph/sfatunmbi/bladebit"

# --- LOG OUTPUT DIRECTORY ---
LOG_DIR="$HOME/vaultx/newexperiments/eightsocket/bladebit"

# --- CSV OUTPUT FILE (all experiments appended here) ---
CSV_FILE="$LOG_DIR/bladebit_varying_memory_k${K}.csv"

# --- SUDO PASSWORD ---
SUDO_PASS="sfatunmbi"

# --- PATH TO BLADEBIT BINARY (must be run from its containing directory) ---
BLADEBIT_DIR="$(pwd)"
BLADEBIT_BIN="./bladebit"

# =============================================================================
# VALIDATION
# =============================================================================
if [ ${#TEMP_DRIVES[@]} -ne ${#FINAL_DRIVES[@]} ]; then
    echo "ERROR: TEMP_DRIVES and FINAL_DRIVES arrays must be the same length."
    exit 1
fi

mkdir -p "$LOG_DIR"
mkdir -p "$CEPH_DEST"

# --- Write CSV header if file does not yet exist ---
if [ ! -f "$CSV_FILE" ]; then
    echo "cache_size,k,f1_threads,fp_threads,temp_dir,final_dir,total_plot_time(s),total_plot_time(min),min_power,max_power,avg_power,total_energy(W),total_energy(kW),peak_memory(MB)" \
        > "$CSV_FILE"
fi

# =============================================================================
# HELPER: POWER MONITOR
# =============================================================================
power_monitor() {
    local power_tmp="$1"
    local sentinel="$2"

    > "$power_tmp"

    while [ -f "$sentinel" ]; do
        reading=$(echo "$SUDO_PASS" | sudo -S ipmitool dcmi power reading 2>/dev/null \
            | grep -i "Instantaneous power reading" \
            | grep -oP '\d+(?=\s+Watts)')
        if [ -n "$reading" ]; then
            echo "$reading" >> "$power_tmp"
        fi
        sleep 1
    done
}

# =============================================================================
# HELPER: PEAK MEMORY MONITOR
# Polls /proc/<pid>/status for VmRSS (kB) every second, tracks maximum.
# =============================================================================
peak_memory_monitor() {
    local plot_pid="$1"
    local mem_file="$2"
    local sentinel="$3"

    local peak_kb=0
    > "$mem_file"

    while [ -f "$sentinel" ]; do
        if [ -d "/proc/$plot_pid" ]; then
            mem_kb=$(awk '/VmRSS/{print $2}' /proc/$plot_pid/status 2>/dev/null)
            if [[ "$mem_kb" =~ ^[0-9]+$ ]] && (( mem_kb > peak_kb )); then
                peak_kb=$mem_kb
            fi
        fi
        sleep 1
    done

    echo "$peak_kb" > "$mem_file"
}

# =============================================================================
# HELPER: CLEAN UP DRIVES & CACHES BETWEEN EXPERIMENTS
# =============================================================================
cleanup_drives() {
    local temp_dir="$1"
    local final_dir="$2"

    echo "  [cleanup] Removing contents of temp dir: $temp_dir"
    echo "$SUDO_PASS" | sudo -S rm -rf "${temp_dir:?}/"* 2>/dev/null

    echo "  [cleanup] Removing contents of final dir: $final_dir"
    echo "$SUDO_PASS" | sudo -S rm -rf "${final_dir:?}/"* 2>/dev/null

    echo "  [cleanup] Dropping caches (sync + drop_caches)..."
    sync
    echo "$SUDO_PASS" | sudo -S sh -c 'echo 3 > /proc/sys/vm/drop_caches'

    echo "  [cleanup] Done."
}

# =============================================================================
# HELPER: MOVE PLOTS TO CEPH
# =============================================================================
move_plots() {
    local final_dir="$1"

    echo "  [move] Scanning $final_dir for .plot files..."
    for plot_file in "$final_dir"/*.plot; do
        [ -f "$plot_file" ] || continue
        plot_name=$(basename "$plot_file")
        dest_path="$CEPH_DEST/$plot_name"

        if [ -f "$dest_path" ]; then
            echo "  [move] $plot_name already exists in destination — deleting local copy."
            rm -f "$plot_file"
        else
            echo "  [move] Moving $plot_name to $CEPH_DEST"
            mv "$plot_file" "$CEPH_DEST/"
        fi
    done
}

# =============================================================================
# HELPER: APPEND ONE ROW TO CSV
# =============================================================================
append_csv_row() {
    local cache="$1"
    local temp_dir="$2"
    local final_dir="$3"
    local duration_s="$4"
    local min_w="$5"
    local max_w="$6"
    local avg_w="$7"
    local total_wh="$8"
    local total_kwh="$9"
    local peak_mem_mb="${10}"

    local duration_min
    duration_min=$(awk "BEGIN { printf \"%.4f\", $duration_s / 60 }")

    echo "${cache},${K},${N_THREADS},${N_THREADS},${temp_dir},${final_dir},${duration_s},${duration_min},${min_w},${max_w},${avg_w},${total_wh},${total_kwh},${peak_mem_mb}" \
        >> "$CSV_FILE"
}

# =============================================================================
# MAIN EXPERIMENT LOOP
# =============================================================================
TOTAL_EXPERIMENTS=$(( ${#MEMORY_SIZES[@]} * ${#TEMP_DRIVES[@]} ))
EXP_NUM=0

echo "=============================================="
echo " BLADEBIT EXPERIMENT RUNNER"
echo " Memory configs : ${#MEMORY_SIZES[@]}"
echo " Drive pairs    : ${#TEMP_DRIVES[@]}"
echo " Total runs     : $TOTAL_EXPERIMENTS"
echo " Threads (f1/fp): $N_THREADS"
echo " Logs saved to  : $LOG_DIR"
echo " CSV output     : $CSV_FILE"
echo "=============================================="

for CACHE_SIZE in "${MEMORY_SIZES[@]}"; do
    for i in "${!TEMP_DRIVES[@]}"; do
        EXP_NUM=$(( EXP_NUM + 1 ))
        TEMP_DIR="${TEMP_DRIVES[$i]}"
        FINAL_DIR="${FINAL_DRIVES[$i]}"

        TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
        DRIVE_PAIR_LABEL="pair$(( i + 1 ))"
        LOG_FILE="$LOG_DIR/exp${EXP_NUM}_c${CACHE_SIZE}_${DRIVE_PAIR_LABEL}_${TIMESTAMP}.txt"

        echo ""
        echo "=============================================="
        echo " Experiment $EXP_NUM / $TOTAL_EXPERIMENTS"
        echo " Cache size : $CACHE_SIZE"
        echo " Threads    : $N_THREADS (f1 + fp)"
        echo " Temp dir   : $TEMP_DIR"
        echo " Final dir  : $FINAL_DIR"
        echo " Log file   : $LOG_FILE"
        echo "=============================================="

        # --- Write experiment header to log ---
        {
            echo "=============================="
            echo "EXPERIMENT $EXP_NUM / $TOTAL_EXPERIMENTS"
            echo "Date/Time      : $(date)"
            echo "Cache size     : $CACHE_SIZE"
            echo "Threads (f1/fp): $N_THREADS"
            echo "Temp dir       : $TEMP_DIR"
            echo "Final dir      : $FINAL_DIR"
            echo "K              : $K"
            echo "=============================="
            echo ""
        } > "$LOG_FILE"

        # --- Ensure dirs exist ---
        mkdir -p "$TEMP_DIR" "$FINAL_DIR"

        # --- Start power monitor ---
        POWER_TMP=$(mktemp /tmp/power_readings.XXXXXX)
        POWER_SENTINEL=$(mktemp /tmp/power_sentinel.XXXXXX)
        power_monitor "$POWER_TMP" "$POWER_SENTINEL" &
        POWER_BG_PID=$!

        # --- Temp files for memory tracking ---
        MEM_TMP=$(mktemp /tmp/peak_mem.XXXXXX)
        MEM_SENTINEL=$(mktemp /tmp/mem_sentinel.XXXXXX)

        # --- Record start time ---
        EXP_START=$(date +%s)

        # --- Run bladebit in background to capture PID for memory tracking ---
        echo "  [run] Starting bladebit diskplot (cache=$CACHE_SIZE, threads=$N_THREADS)..."
        "$BLADEBIT_BIN" \
            -f "$FARMER_KEY" \
            -c "$CONTRACT" \
            diskplot \
            --f1-threads "$N_THREADS" \
            --fp-threads "$N_THREADS" \
            -a \
            --cache "$CACHE_SIZE" \
            -t1 "$TEMP_DIR" \
            -t2 "$TEMP_DIR" \
            "$FINAL_DIR" \
            >> "$LOG_FILE" 2>&1 &
        PLOT_PID=$!

        # --- Start peak memory monitor ---
        peak_memory_monitor "$PLOT_PID" "$MEM_TMP" "$MEM_SENTINEL" &
        MEM_BG_PID=$!

        # --- Stream log to terminal while waiting ---
        tail -f "$LOG_FILE" &
        TAIL_PID=$!

        # --- Wait for bladebit to finish ---
        wait "$PLOT_PID"
        PLOT_EXIT_CODE=$?

        # --- Record end time ---
        EXP_END=$(date +%s)
        EXP_DURATION=$(( EXP_END - EXP_START ))

        # --- Stop monitors and tail ---
        rm -f "$POWER_SENTINEL" "$MEM_SENTINEL"
        wait "$POWER_BG_PID" 2>/dev/null
        wait "$MEM_BG_PID" 2>/dev/null
        kill "$TAIL_PID" 2>/dev/null
        wait "$TAIL_PID" 2>/dev/null

        # --- Calculate power stats ---
        READING_COUNT=0
        TOTAL_WATTS=0
        MIN_W=999999
        MAX_W=0

        while IFS= read -r w; do
            [[ "$w" =~ ^[0-9]+$ ]] || continue
            READING_COUNT=$(( READING_COUNT + 1 ))
            TOTAL_WATTS=$(( TOTAL_WATTS + w ))
            (( w < MIN_W )) && MIN_W=$w
            (( w > MAX_W )) && MAX_W=$w
        done < "$POWER_TMP"

        if [ "$READING_COUNT" -gt 0 ]; then
            AVG_W=$(( TOTAL_WATTS / READING_COUNT ))
            TOTAL_WH=$(awk "BEGIN { printf \"%.4f\", ($AVG_W * $EXP_DURATION) / 3600 }")
            TOTAL_KWH=$(awk "BEGIN { printf \"%.6f\", $TOTAL_WH / 1000 }")
        else
            AVG_W="N/A"
            TOTAL_WH="N/A"
            TOTAL_KWH="N/A"
            MIN_W="N/A"
            MAX_W="N/A"
        fi

        # --- Read peak memory (kB -> MB) ---
        PEAK_MEM_KB=$(cat "$MEM_TMP" 2>/dev/null)
        if [[ "$PEAK_MEM_KB" =~ ^[0-9]+$ ]] && [ "$PEAK_MEM_KB" -gt 0 ]; then
            PEAK_MEM_MB=$(awk "BEGIN { printf \"%.2f\", $PEAK_MEM_KB / 1024 }")
        else
            PEAK_MEM_MB="N/A"
        fi

        rm -f "$POWER_TMP" "$MEM_TMP"

        # --- Append power + memory summary to log ---
        {
            echo ""
            echo "=============================="
            echo "POWER CONSUMPTION SUMMARY"
            echo "=============================="
            echo "Experiment duration : ${EXP_DURATION}s ($(( EXP_DURATION / 60 ))m $(( EXP_DURATION % 60 ))s)"
            echo "Readings collected  : $READING_COUNT"
            echo "Min power           : ${MIN_W} W"
            echo "Max power           : ${MAX_W} W"
            echo "Avg power           : ${AVG_W} W"
            echo "Total energy used   : ${TOTAL_WH} Wh  /  ${TOTAL_KWH} kWh"
            echo "Peak memory (RSS)   : ${PEAK_MEM_MB} MB"
            echo "Plot exit code      : $PLOT_EXIT_CODE"
            echo "=============================="
        } | tee -a "$LOG_FILE"

        # --- Append row to CSV ---
        append_csv_row \
            "$CACHE_SIZE" \
            "$TEMP_DIR" \
            "$FINAL_DIR" \
            "$EXP_DURATION" \
            "$MIN_W" \
            "$MAX_W" \
            "$AVG_W" \
            "$TOTAL_WH" \
            "$TOTAL_KWH" \
            "$PEAK_MEM_MB"

        echo "  [csv] Row appended to $CSV_FILE"

        # --- Move plots to Ceph ---
        echo "  [post] Moving plots to Ceph..."
        move_plots "$FINAL_DIR"

        # --- Cleanup between experiments ---
        if [ "$EXP_NUM" -lt "$TOTAL_EXPERIMENTS" ]; then
            echo "  [post] Cleaning up drives and caches before next experiment..."
            cleanup_drives "$TEMP_DIR" "$FINAL_DIR"
        else
            echo "  [post] Last experiment complete — skipping drive wipe."
        fi

        echo "  [done] Experiment $EXP_NUM finished. Log: $LOG_FILE"
    done
done

echo ""
echo "=============================================="
echo " ALL $TOTAL_EXPERIMENTS EXPERIMENTS COMPLETE"
echo " Logs in : $LOG_DIR"
echo " CSV     : $CSV_FILE"
echo "=============================================="
