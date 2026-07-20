#!/bin/bash
# madmax_power_k32.sh
#
# Wall-power benchmark for madmax: k=32 only, fixed at this machine's max
# thread count (nproc) -- no thread sweep. The only things that vary are
# drive (TEMP_DRIVES/FINAL_DRIVES -- set both to a single entry to stay on
# one drive), N (repetitions per drive), and SHUTDOWN.
#
# This is meant to be read against an EXTERNAL WALL POWER METER, not just
# the internal ipmitool numbers. Watch for the ">>> WALL METER <<<" banners
# telling you exactly when to note the meter's reading. The ipmitool-derived
# power/energy columns in the CSV are a supplementary automated cross-check
# only -- they are not the number of record.
#
# SHUTDOWN=true powers the machine off after the last run so nothing keeps
# drawing power afterwards -- the meter's post-shutdown reading is then the
# true, final total for the whole session (no idle-time accumulation to
# second-guess).
#
# Derived from newexperiments/s8/others/madmaxvaryingthreads.sh (same
# power_monitor / peak_memory_monitor methodology, same CSV power columns),
# with the thread sweep collapsed to a single max-threads value and
# repetition/shutdown support added.
#
# Usage:
#   ./madmax_power_k32.sh
#   N=3 SHUTDOWN=true ./madmax_power_k32.sh
#
# Configure TEMP_DRIVES / FINAL_DRIVES below (edit directly, like the
# original script) -- both arrays must be the same length.
# =============================================================================

# --- FARMER KEY & CONTRACT ADDRESS ---
FARMER_KEY="96854d93efe790622d1a872ae3055eee65b2908bc3a55c203e1b81021179d797dfa219303a173808d76f15ce38efc1fa"
CONTRACT="xch1ta2vz9qddvlhn6sahade07nfe9vf86jz8v0k3p4n62208rl377esytyjx3"

# --- CONSTANT PARAMS ---
K=32
BUCKETS_U=128
BUCKETS_V=128
THREADS="$(nproc)"          # fixed at this machine's max thread count

# --- WHAT TO VARY ---
# Number of k32 plots to make per drive pair (repetitions).
N="${N:-1}"

# Drive pairs (temp[i] paired with final[i], must be same length).
# Set both arrays to a single entry to keep everything on one drive.
TEMP_DRIVES=(
    "/nvme-raid0/sfatunmbi/temp"
)
FINAL_DRIVES=(
    "/nvme-raid0/sfatunmbi/plots"
)

# Shut the machine down once the benchmark finishes?
#   true  -> auto power-off after the last run, so power stops accumulating
#            and the meter's post-shutdown reading is stable/final.
#   false -> leave the machine running/idle; note the meter reading yourself.
SHUTDOWN="${SHUTDOWN:-false}"
SHUTDOWN_DELAY_S="${SHUTDOWN_DELAY_S:-15}"   # grace period to Ctrl+C before it fires

# --- LOG OUTPUT DIRECTORY ---
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${ROOT_DIR}/newexperiments/$(hostname)/madmax_power"

# --- CSV OUTPUT FILE (all runs appended here) ---
CSV_FILE="$LOG_DIR/power_k${K}_madmax_$(hostname).csv"

# --- SUDO PASSWORD (for ipmitool power reads and shutdown; override via env) ---
SUDO_PASS="${SUDO_PASS:-sfatunmbi}"

# --- PATH TO CHIA PLOT BINARY (adjust if needed) ---
CHIA_PLOT_BIN="${CHIA_PLOT_BIN:-./chia_plot}"

# =============================================================================
# VALIDATION
# =============================================================================
if [ ${#TEMP_DRIVES[@]} -ne ${#FINAL_DRIVES[@]} ]; then
    echo "ERROR: TEMP_DRIVES and FINAL_DRIVES arrays must be the same length."
    exit 1
fi
if ! [[ "$N" =~ ^[0-9]+$ ]] || [ "$N" -lt 1 ]; then
    echo "ERROR: N must be a positive integer (got '$N')."
    exit 1
fi
if [[ "$SHUTDOWN" != "true" && "$SHUTDOWN" != "false" ]]; then
    echo "ERROR: SHUTDOWN must be 'true' or 'false' (got '$SHUTDOWN')."
    exit 1
fi

mkdir -p "$LOG_DIR"

if [ ! -f "$CSV_FILE" ]; then
    echo "drive_pair,rep,k,threads_r,buckets_u,buckets_v,temp_dir,final_dir,start_time,end_time,total_plot_time(s),total_plot_time(min),min_power,max_power,avg_power,total_energy(Wh),total_energy(kWh),peak_memory(MB)" \
        > "$CSV_FILE"
fi

# =============================================================================
# HELPER: APPEND ONE ROW TO CSV
# =============================================================================
append_csv_row() {
    local drive_pair="$1" rep="$2" temp_dir="$3" final_dir="$4"
    local start_time="$5" end_time="$6" duration_s="$7"
    local min_w="$8" max_w="$9" avg_w="${10}" total_wh="${11}" total_kwh="${12}" peak_mem_mb="${13}"

    local duration_min
    duration_min=$(awk "BEGIN { printf \"%.4f\", $duration_s / 60 }")

    echo "${drive_pair},${rep},${K},${THREADS},${BUCKETS_U},${BUCKETS_V},${temp_dir},${final_dir},${start_time},${end_time},${duration_s},${duration_min},${min_w},${max_w},${avg_w},${total_wh},${total_kwh},${peak_mem_mb}" \
        >> "$CSV_FILE"
}

# =============================================================================
# HELPER: POWER MONITOR (runs in background, writes readings to a temp file)
# Kills itself when the sentinel file is removed. (Supplementary automated
# cross-check -- the wall meter is the number of record.)
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
# HELPER: CLEAN UP DRIVES & CACHES BETWEEN RUNS
# Wipes temp/final dir contents (a fresh k32 madmax run needs the space, and
# there is no reason to keep N identical plots around) and drops caches.
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
# HELPER: COUNTDOWN (log-friendly, no \r) -- Ctrl+C during this cancels
# whatever it's guarding (e.g. shutdown) since sleep just dies with the script.
# =============================================================================
countdown() {
    local secs="$1" msg="$2"
    echo "$msg (Ctrl+C to cancel)"
    while (( secs > 0 )); do
        echo "  ...${secs}s"
        local step=5
        (( secs < step )) && step=$secs
        sleep "$step"
        secs=$(( secs - step ))
    done
}

# =============================================================================
# MAIN
# =============================================================================
TOTAL_RUNS=$(( N * ${#TEMP_DRIVES[@]} ))
RUN_NUM=0
GRAND_DURATION_S=0

WALL_NOTE_FILE="$LOG_DIR/wall_meter_notes_$(date +%Y%m%d_%H%M%S).txt"

{
    echo "=============================================="
    echo " MADMAX K32 WALL-POWER BENCHMARK"
    echo " Host           : $(hostname)"
    echo " Threads (-r)   : $THREADS (nproc)"
    echo " Drive pairs    : ${#TEMP_DRIVES[@]}"
    echo " Reps per drive : $N"
    echo " Total runs     : $TOTAL_RUNS"
    echo " Shutdown after : $SHUTDOWN"
    echo " Start time     : $(date)"
    echo " Logs saved to  : $LOG_DIR"
    echo "=============================================="
    echo ""
    echo ">>> WALL METER: note the STARTING reading now. <<<"
} | tee "$WALL_NOTE_FILE"

countdown 8 "Starting benchmark in"

for i in "${!TEMP_DRIVES[@]}"; do
    TEMP_DIR="${TEMP_DRIVES[$i]}"
    FINAL_DIR="${FINAL_DRIVES[$i]}"
    DRIVE_PAIR_LABEL="pair$(( i + 1 ))"
    mkdir -p "$TEMP_DIR" "$FINAL_DIR"

    for (( rep=1; rep<=N; rep++ )); do
        RUN_NUM=$(( RUN_NUM + 1 ))

        TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
        LOG_FILE="$LOG_DIR/run${RUN_NUM}_${DRIVE_PAIR_LABEL}_rep${rep}_${TIMESTAMP}.txt"

        echo ""
        echo "=============================================="
        echo " Run $RUN_NUM / $TOTAL_RUNS  (drive $DRIVE_PAIR_LABEL, rep $rep/$N)"
        echo " Temp dir   : $TEMP_DIR"
        echo " Final dir  : $FINAL_DIR"
        echo " Log file   : $LOG_FILE"
        echo "=============================================="

        {
            echo "=============================="
            echo "RUN $RUN_NUM / $TOTAL_RUNS  (drive $DRIVE_PAIR_LABEL, rep $rep/$N)"
            echo "Date/Time   : $(date)"
            echo "Threads (-r): $THREADS"
            echo "Temp dir    : $TEMP_DIR"
            echo "Final dir   : $FINAL_DIR"
            echo "Buckets (-u): $BUCKETS_U"
            echo "Buckets (-v): $BUCKETS_V"
            echo "=============================="
            echo ""
        } > "$LOG_FILE"

        # --- Start power monitor in background ---
        POWER_TMP=$(mktemp /tmp/power_readings.XXXXXX)
        POWER_SENTINEL=$(mktemp /tmp/power_sentinel.XXXXXX)
        power_monitor "$POWER_TMP" "$POWER_SENTINEL" &
        POWER_BG_PID=$!

        MEM_TMP=$(mktemp /tmp/peak_mem.XXXXXX)
        MEM_SENTINEL=$(mktemp /tmp/mem_sentinel.XXXXXX)

        START_TIME=$(date +"%Y-%m-%d %H:%M:%S")
        EXP_START=$(date +%s)

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

        peak_memory_monitor "$PLOT_PID" "$MEM_TMP" "$MEM_SENTINEL" &
        MEM_BG_PID=$!

        tail -f "$LOG_FILE" &
        TAIL_PID=$!

        wait "$PLOT_PID"
        PLOT_EXIT_CODE=$?

        END_TIME=$(date +"%Y-%m-%d %H:%M:%S")
        EXP_END=$(date +%s)
        EXP_DURATION=$(( EXP_END - EXP_START ))
        GRAND_DURATION_S=$(( GRAND_DURATION_S + EXP_DURATION ))

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
            AVG_W="N/A"; TOTAL_WH="N/A"; TOTAL_KWH="N/A"; MIN_W="N/A"; MAX_W="N/A"
        fi

        PEAK_MEM_KB=$(cat "$MEM_TMP" 2>/dev/null)
        if [[ "$PEAK_MEM_KB" =~ ^[0-9]+$ ]] && [ "$PEAK_MEM_KB" -gt 0 ]; then
            PEAK_MEM_MB=$(awk "BEGIN { printf \"%.2f\", $PEAK_MEM_KB / 1024 }")
        else
            PEAK_MEM_MB="N/A"
        fi

        rm -f "$POWER_TMP" "$MEM_TMP"

        {
            echo ""
            echo "=============================="
            echo "POWER CONSUMPTION SUMMARY (supplementary -- see wall meter for the real reading)"
            echo "=============================="
            echo "Run duration        : ${EXP_DURATION}s ($(( EXP_DURATION / 60 ))m $(( EXP_DURATION % 60 ))s)"
            echo "Readings collected  : $READING_COUNT"
            echo "Min power           : ${MIN_W} W"
            echo "Max power           : ${MAX_W} W"
            echo "Avg power           : ${AVG_W} W"
            echo "Total energy used   : ${TOTAL_WH} Wh  /  ${TOTAL_KWH} kWh"
            echo "Peak memory (RSS)   : ${PEAK_MEM_MB} MB"
            echo "Plot exit code      : $PLOT_EXIT_CODE"
            echo "=============================="
        } | tee -a "$LOG_FILE"

        append_csv_row \
            "$DRIVE_PAIR_LABEL" "$rep" "$TEMP_DIR" "$FINAL_DIR" \
            "$START_TIME" "$END_TIME" "$EXP_DURATION" \
            "$MIN_W" "$MAX_W" "$AVG_W" "$TOTAL_WH" "$TOTAL_KWH" "$PEAK_MEM_MB"

        echo "  [csv] Row appended to $CSV_FILE"

        echo "  [post] Cleaning up drives and caches..."
        cleanup_drives "$TEMP_DIR" "$FINAL_DIR"

        echo "  [done] Run $RUN_NUM finished. Log: $LOG_FILE"
    done
done

{
    echo ""
    echo "=============================================="
    echo " ALL $TOTAL_RUNS RUNS COMPLETE"
    echo " End time         : $(date)"
    echo " Total wall time  : ${GRAND_DURATION_S}s ($(( GRAND_DURATION_S / 60 ))m)"
    echo " CSV              : $CSV_FILE"
    echo "=============================================="
    echo ""
    if [ "$SHUTDOWN" = "true" ]; then
        echo ">>> WALL METER: machine is about to shut down. Note the FINAL"
        echo "    reading once it is fully powered off. <<<"
    else
        echo ">>> WALL METER: note the FINAL reading now. <<<"
    fi
} | tee -a "$WALL_NOTE_FILE"

if [ "$SHUTDOWN" = "true" ]; then
    countdown "$SHUTDOWN_DELAY_S" "Shutting down in"
    echo "$SUDO_PASS" | sudo -S shutdown -h now
fi
