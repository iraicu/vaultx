#!/bin/bash

# =============================================================================
# BLADEBIT RAMPLOT — SINGLE RUN
# Plots once using all RAM (ramplot subcommand).
# Measures power and peak memory; moves plot to Ceph; clears caches before/after.
# =============================================================================

# --- FARMER KEY & CONTRACT ADDRESS ---
FARMER_KEY="96854d93efe790622d1a872ae3055eee65b2908bc3a55c203e1b81021179d797dfa219303a173808d76f15ce38efc1fa"
CONTRACT="xch1ta2vz9qddvlhn6sahade07nfe9vf86jz8v0k3p4n62208rl377esytyjx3"

# --- THREAD COUNT ---
N_THREADS=256

# --- OUTPUT DIRECTORY (change this to redirect where the .plot file is written) ---
PLOT_OUT_DIR="/nvme-raid0/sfatunmbi/plots"

# --- DESTINATION FOR FINISHED PLOTS ---
CEPH_DEST="/ceph/sfatunmbi/bladebit"

# --- LOG OUTPUT DIRECTORY ---
LOG_DIR="$HOME/vaultx/newexperiments/eightsocket/bladebit"

# --- CSV OUTPUT FILE ---
CSV_FILE="$LOG_DIR/bladebit_ramplot.csv"

# --- SUDO PASSWORD ---
SUDO_PASS="sfatunmbi"

# --- PATH TO BLADEBIT BINARY (run this script from the bladebit directory) ---
BLADEBIT_BIN="./bladebit"

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
# SETUP
# =============================================================================
mkdir -p "$LOG_DIR" "$CEPH_DEST" "$PLOT_OUT_DIR"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="$LOG_DIR/ramplot_${TIMESTAMP}.txt"

# Write CSV header if file does not yet exist
if [ ! -f "$CSV_FILE" ]; then
    echo "threads,plot_out_dir,total_plot_time(s),total_plot_time(min),min_power(W),max_power(W),avg_power(W),total_energy(Wh),total_energy(kWh),peak_memory(MB)" \
        > "$CSV_FILE"
fi

echo "=============================================="
echo " BLADEBIT RAMPLOT — SINGLE RUN"
echo " Threads    : $N_THREADS"
echo " Output dir : $PLOT_OUT_DIR"
echo " Ceph dest  : $CEPH_DEST"
echo " Log file   : $LOG_FILE"
echo " CSV output : $CSV_FILE"
echo "=============================================="

# --- Write run header to log ---
{
    echo "=============================="
    echo "BLADEBIT RAMPLOT RUN"
    echo "Date/Time   : $(date)"
    echo "Threads     : $N_THREADS"
    echo "Output dir  : $PLOT_OUT_DIR"
    echo "=============================="
    echo ""
} > "$LOG_FILE"

# =============================================================================
# PRE-RUN CACHE CLEAR
# =============================================================================
echo "  [pre] Dropping caches before run..."
sync
echo "$SUDO_PASS" | sudo -S sh -c 'echo 3 > /proc/sys/vm/drop_caches'
echo "  [pre] Cache cleared."

# =============================================================================
# RUN BLADEBIT RAMPLOT
# =============================================================================
POWER_TMP=$(mktemp /tmp/power_readings.XXXXXX)
POWER_SENTINEL=$(mktemp /tmp/power_sentinel.XXXXXX)
power_monitor "$POWER_TMP" "$POWER_SENTINEL" &
POWER_BG_PID=$!

MEM_TMP=$(mktemp /tmp/peak_mem.XXXXXX)
MEM_SENTINEL=$(mktemp /tmp/mem_sentinel.XXXXXX)

EXP_START=$(date +%s)

echo "  [run] Starting bladebit ramplot (threads=$N_THREADS)..."
"$BLADEBIT_BIN" \
    -t "$N_THREADS" \
    -f "$FARMER_KEY" \
    -c "$CONTRACT" \
    ramplot \
    "$PLOT_OUT_DIR" \
    >> "$LOG_FILE" 2>&1 &
PLOT_PID=$!

peak_memory_monitor "$PLOT_PID" "$MEM_TMP" "$MEM_SENTINEL" &
MEM_BG_PID=$!

# Stream log to terminal while waiting
tail -f "$LOG_FILE" &
TAIL_PID=$!

wait "$PLOT_PID"
PLOT_EXIT_CODE=$?

EXP_END=$(date +%s)
EXP_DURATION=$(( EXP_END - EXP_START ))

# Stop monitors and tail
rm -f "$POWER_SENTINEL" "$MEM_SENTINEL"
wait "$POWER_BG_PID" 2>/dev/null
wait "$MEM_BG_PID" 2>/dev/null
kill "$TAIL_PID" 2>/dev/null
wait "$TAIL_PID" 2>/dev/null

# =============================================================================
# POWER STATS
# =============================================================================
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

# =============================================================================
# PEAK MEMORY
# =============================================================================
PEAK_MEM_KB=$(cat "$MEM_TMP" 2>/dev/null)
if [[ "$PEAK_MEM_KB" =~ ^[0-9]+$ ]] && [ "$PEAK_MEM_KB" -gt 0 ]; then
    PEAK_MEM_MB=$(awk "BEGIN { printf \"%.2f\", $PEAK_MEM_KB / 1024 }")
else
    PEAK_MEM_MB="N/A"
fi

rm -f "$POWER_TMP" "$MEM_TMP"

# =============================================================================
# SUMMARY
# =============================================================================
{
    echo ""
    echo "=============================="
    echo "RUN SUMMARY"
    echo "=============================="
    echo "Duration            : ${EXP_DURATION}s ($(( EXP_DURATION / 60 ))m $(( EXP_DURATION % 60 ))s)"
    echo "Power readings      : $READING_COUNT"
    echo "Min power           : ${MIN_W} W"
    echo "Max power           : ${MAX_W} W"
    echo "Avg power           : ${AVG_W} W"
    echo "Total energy        : ${TOTAL_WH} Wh  /  ${TOTAL_KWH} kWh"
    echo "Peak memory (RSS)   : ${PEAK_MEM_MB} MB"
    echo "Plot exit code      : $PLOT_EXIT_CODE"
    echo "=============================="
} | tee -a "$LOG_FILE"

# =============================================================================
# APPEND CSV ROW
# =============================================================================
DURATION_MIN=$(awk "BEGIN { printf \"%.4f\", $EXP_DURATION / 60 }")
echo "${N_THREADS},${PLOT_OUT_DIR},${EXP_DURATION},${DURATION_MIN},${MIN_W},${MAX_W},${AVG_W},${TOTAL_WH},${TOTAL_KWH},${PEAK_MEM_MB}" \
    >> "$CSV_FILE"
echo "  [csv] Row appended to $CSV_FILE"

# =============================================================================
# MOVE PLOTS TO CEPH
# =============================================================================
echo "  [post] Moving .plot files to $CEPH_DEST..."
for plot_file in "$PLOT_OUT_DIR"/*.plot; do
    [ -f "$plot_file" ] || continue
    plot_name=$(basename "$plot_file")
    dest_path="$CEPH_DEST/$plot_name"
    if [ -f "$dest_path" ]; then
        echo "  [post] $plot_name already exists at destination — deleting local copy."
        rm -f "$plot_file"
    else
        echo "  [post] Moving $plot_name -> $CEPH_DEST"
        mv "$plot_file" "$CEPH_DEST/"
    fi
done

# =============================================================================
# POST-RUN CACHE CLEAR
# =============================================================================
echo "  [post] Dropping caches after run..."
sync
echo "$SUDO_PASS" | sudo -S sh -c 'echo 3 > /proc/sys/vm/drop_caches'
echo "  [post] Cache cleared."

echo ""
echo "=============================================="
echo " RUN COMPLETE"
echo " Log : $LOG_FILE"
echo " CSV : $CSV_FILE"
echo "=============================================="
