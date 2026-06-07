#!/bin/bash

# =============================================================================
# CHIA PLOTTING EXPERIMENT SCRIPT
# Varies thread counts across drive pairs, logs output and power per run.
# Also tracks peak memory usage (VmRSS) of the chia_plot process.
# =============================================================================

# --- FARMER KEY & CONTRACT ADDRESS ---
FARMER_KEY="96854d93efe790622d1a872ae3055eee65b2908bc3a55c203e1b81021179d797dfa219303a173808d76f15ce38efc1fa"
CONTRACT="xch1ta2vz9qddvlhn6sahade07nfe9vf86jz8v0k3p4n62208rl377esytyjx3"

# --- CONSTANT PARAMS ---
K=32
BUCKETS_U=128
BUCKETS_V=128

# --- THREAD COUNTS TO TEST (add/remove as needed) ---
THREAD_COUNTS=(8 16 32 64 128 256 384)

# --- DRIVE PAIRS (temp[i] paired with final[i], must be same length) ---
TEMP_DRIVES=(
    "/nvme-raid0/sfatunmbi/temp"
    "/ceph/sfatunmbi/temp"
)
FINAL_DRIVES=(
    "/nvme-raid0/sfatunmbi/plots"
    "/ceph/sfatunmbi/plots"
)

# --- DESTINATION FOR FINISHED PLOTS ---
CEPH_DEST="/ceph/sfatunmbi/madmax"

# --- LOG OUTPUT DIRECTORY ---
LOG_DIR="$HOME/vaultx/newexperiments/eightsocket/madmax"

# --- CSV OUTPUT FILE (all experiments appended here) ---
CSV_FILE="$LOG_DIR/varying_threads_k${K}_madmax.csv"

# --- SUDO PASSWORD ---
SUDO_PASS="sfatunmbi"

# --- PATH TO CHIA PLOT BINARY (adjust if needed) ---
CHIA_PLOT_BIN="./chia_plot"

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
    echo "thread_count_r,k,buckets_u,buckets_v,temp_dir,final_dir,total_plot_time(s),total_plot_time(min),min_power,max_power,avg_power,total_energy(W),total_energy(kW),peak_memory(MB)" \
        > "$CSV_FILE"
fi

# =============================================================================
# HELPER: APPEND ONE ROW TO CSV
# =============================================================================
append_csv_row() {
    local threads="$1"
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

    echo "${threads},${K},${BUCKETS_U},${BUCKETS_V},${temp_dir},${final_dir},${duration_s},${duration_min},${min_w},${max_w},${avg_w},${total_wh},${total_kwh},${peak_mem_mb}" \
        >> "$CSV_FILE"
}

# =============================================================================
# HELPER: POWER MONITOR (runs in background, writes readings to a temp file)
# Kills itself when the sentinel file is removed.
# =============================================================================
power_monitor() {
    local power_tmp="$1"
    local pid_file="$2"

    > "$power_tmp"

    while [ -f "$pid_file" ]; do
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
# Polls /proc/<pid>/status every second for VmRSS (kB), tracks the maximum.
# Writes the peak value (in kB) to mem_file when the sentinel is removed.
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
# HELPER: MOVE PLOTS TO CEPH (skip if same name exists, delete local copy)
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
# MAIN EXPERIMENT LOOP
# =============================================================================
TOTAL_EXPERIMENTS=$(( ${#THREAD_COUNTS[@]} * ${#TEMP_DRIVES[@]} ))
EXP_NUM=0

echo "=============================================="
echo " CHIA EXPERIMENT RUNNER"
echo " Thread configs : ${#THREAD_COUNTS[@]}"
echo " Drive pairs    : ${#TEMP_DRIVES[@]}"
echo " Total runs     : $TOTAL_EXPERIMENTS"
echo " Logs saved to  : $LOG_DIR"
echo "=============================================="

for THREADS in "${THREAD_COUNTS[@]}"; do
    for i in "${!TEMP_DRIVES[@]}"; do
        EXP_NUM=$(( EXP_NUM + 1 ))
        TEMP_DIR="${TEMP_DRIVES[$i]}"
        FINAL_DIR="${FINAL_DRIVES[$i]}"

        TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
        DRIVE_PAIR_LABEL="pair$(( i + 1 ))"
        LOG_FILE="$LOG_DIR/exp${EXP_NUM}_t${THREADS}_${DRIVE_PAIR_LABEL}_${TIMESTAMP}.txt"

        echo ""
        echo "=============================================="
        echo " Experiment $EXP_NUM / $TOTAL_EXPERIMENTS"
        echo " Threads    : $THREADS"
        echo " Temp dir   : $TEMP_DIR"
        echo " Final dir  : $FINAL_DIR"
        echo " Log file   : $LOG_FILE"
        echo "=============================================="

        # --- Write experiment header to log ---
        {
            echo "=============================="
            echo "EXPERIMENT $EXP_NUM / $TOTAL_EXPERIMENTS"
            echo "Date/Time   : $(date)"
            echo "Threads (-r): $THREADS"
            echo "Temp dir    : $TEMP_DIR"
            echo "Final dir   : $FINAL_DIR"
            echo "Buckets (-u): $BUCKETS_U"
            echo "Buckets (-v): $BUCKETS_V"
            echo "=============================="
            echo ""
        } > "$LOG_FILE"

        # --- Ensure dirs exist ---
        mkdir -p "$TEMP_DIR" "$FINAL_DIR"

        # --- Start power monitor in background ---
        POWER_TMP=$(mktemp /tmp/power_readings.XXXXXX)
        POWER_SENTINEL=$(mktemp /tmp/power_sentinel.XXXXXX)

        power_monitor "$POWER_TMP" "$POWER_SENTINEL" &
        POWER_BG_PID=$!

        # --- Start memory sentinel (shared with both monitors) ---
        MEM_TMP=$(mktemp /tmp/peak_mem.XXXXXX)
        MEM_SENTINEL=$(mktemp /tmp/mem_sentinel.XXXXXX)

        # --- Record experiment start time ---
        EXP_START=$(date +%s)

        # --- Run chia_plot in background so we can capture its PID for memory tracking ---
        echo "  [run] Starting chia_plot..."
        "$CHIA_PLOT_BIN" \
            -k "$K" \
            -r "$THREADS" \
            -u "$BUCKETS_U" \
            -v "$BUCKETS_V" \
            -t "$TEMP_DIR/" \
            -d "$FINAL_DIR/" \
            -f "$FARMER_KEY" \
            -c "$CONTRACT" \
            >> "$LOG_FILE" 2>&1 &
        PLOT_PID=$!

        # --- Start peak memory monitor now that we have the PID ---
        peak_memory_monitor "$PLOT_PID" "$MEM_TMP" "$MEM_SENTINEL" &
        MEM_BG_PID=$!

        # --- Stream log to terminal while waiting ---
        tail -f "$LOG_FILE" &
        TAIL_PID=$!

        # --- Wait for chia_plot to finish ---
        wait "$PLOT_PID"
        PLOT_EXIT_CODE=$?

        # --- Record experiment end time ---
        EXP_END=$(date +%s)
        EXP_DURATION=$(( EXP_END - EXP_START ))

        # --- Stop monitors ---
        rm -f "$POWER_SENTINEL" "$MEM_SENTINEL"
        wait "$POWER_BG_PID" 2>/dev/null
        wait "$MEM_BG_PID" 2>/dev/null

        # --- Stop tail ---
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
            "$THREADS" \
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

        # --- Cleanup (skip after the very last experiment if you prefer) ---
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
echo " Logs in: $LOG_DIR"
echo "=============================================="
