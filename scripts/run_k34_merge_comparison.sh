#!/bin/bash

# Simple K=34 memory sweep script
# Tests VaultX with different memory values from 320MB to 163840MB

set -e

# Memory values to test (in MB)
MEMORY_VALUES=(320 640 1280 2560 5120 10240 20480 40960 81920 163840)

# Drive configurations to test
DRIVES=(
    "/data-l/varvara/vaultx/plots/"
    "/data-fast2/varvara/vaultx/plots/"
    "MIXED"
)

DRIVE_NAMES=(
    "hdd"
    "ssd-nvme"
    "ssd-raid0-hdd"
)

# Mixed drive configuration (for third option)
MIXED_F_G="/ssd-raid0/varvara/vaultx/plots/"
MIXED_J="/data-l/varvara/vaultx/plots/"

make clean
make vaultx_x86_c NONCE_SIZE=5 RECORD_SIZE=16

# Create directories if they don't exist
mkdir -p ./data
mkdir -p ./plots
mkdir -p ./graphs

# Create drive directories
for DRIVE in "${DRIVES[@]}"; do
    if [ "$DRIVE" != "MIXED" ]; then
        mkdir -p "$DRIVE"
    fi
done
# Create mixed drive directories
mkdir -p "$MIXED_F_G"
mkdir -p "$MIXED_J"

echo "Starting K=34 memory sweep tests on multiple drives..."
echo "approach,drive,K,record_size,num_threads,memory_mb,file_size_gb,batch_size,write_batch_size,throughput_mhs,throughput_mbs,hash_time,hash2_time,io_time,io2_time,other_time,total_time,match_percentage,storage_efficiency" > ./data/comparison-to-merge.csv

# Test each drive configuration
for i in "${!DRIVES[@]}"; do
    DRIVE_PATH="${DRIVES[$i]}"
    DRIVE_NAME="${DRIVE_NAMES[$i]}"
    
    echo "Testing drive: $DRIVE_NAME at $DRIVE_PATH"
    
    for MEMORY in "${MEMORY_VALUES[@]}"; do
        echo "Testing with ${MEMORY}MB memory on $DRIVE_NAME..."
        
        # Clean previous run from all drives
        for CLEAN_DRIVE in "${DRIVES[@]}"; do
            if [ "$CLEAN_DRIVE" != "MIXED" ]; then
                rm -rf "${CLEAN_DRIVE}"*.plot "${CLEAN_DRIVE}"*.tmp "${CLEAN_DRIVE}"*.tmp2 2>/dev/null || true
            fi
        done
        # Clean mixed drive directories
        rm -rf "${MIXED_F_G}"*.plot "${MIXED_F_G}"*.tmp "${MIXED_F_G}"*.tmp2 2>/dev/null || true
        rm -rf "${MIXED_J}"*.plot "${MIXED_J}"*.tmp "${MIXED_J}"*.tmp2 2>/dev/null || true
        rm -rf ./plots/*.plot ./plots/*.tmp 2>/dev/null || true

        ./scripts/drop-all-caches.sh
        
        # Determine drive configuration and run VaultX
        if [ "$DRIVE_PATH" == "MIXED" ]; then
            # Mixed drive configuration: -f -g on SSD RAID0, -j on HDD
            ./scripts/vaultx_system_monitor_pidstat.py \
                --plot-file "./graphs/${DRIVE_NAME}-monitor-plot-k34-${MEMORY}.svg" \
                --csv-output "./data/${DRIVE_NAME}-monitor-k34-${MEMORY}.csv" \
                -- ./vaultx -a for -K 34 -m $MEMORY -W $MEMORY -t 64 \
                -f "$MIXED_F_G" \
                -g "$MIXED_F_G" \
                -j "$MIXED_J" \
                -M 1 -x true -n true | sed "s/^/for,${DRIVE_NAME},/" >> ./data/comparison-to-merge.csv
            
        else
            # Single drive configuration: all flags point to same drive
            ./scripts/vaultx_system_monitor_pidstat.py \
                --plot-file "./graphs/${DRIVE_NAME}-monitor-plot-k34-${MEMORY}.svg" \
                --csv-output "./data/${DRIVE_NAME}-monitor-k34-${MEMORY}.csv" \
                -- ./vaultx -a for -K 34 -m $MEMORY -W $MEMORY -t 64 \
                -f "$DRIVE_PATH" \
                -g "$DRIVE_PATH" \
                -j "$DRIVE_PATH" \
                -M 1 -x true -n true | sed "s/^/for,${DRIVE_NAME},/" >> ./data/comparison-to-merge.csv
        fi
        
        # Clean up after each test
        for CLEAN_DRIVE in "${DRIVES[@]}"; do
            if [ "$CLEAN_DRIVE" != "MIXED" ]; then
                rm -rf "${CLEAN_DRIVE}"*.plot "${CLEAN_DRIVE}"*.tmp "${CLEAN_DRIVE}"*.tmp2 2>/dev/null || true
            fi
        done
        # Clean mixed drive directories
        rm -rf "${MIXED_F_G}"*.plot "${MIXED_F_G}"*.tmp "${MIXED_F_G}"*.tmp2 2>/dev/null || true
        rm -rf "${MIXED_J}"*.plot "${MIXED_J}"*.tmp "${MIXED_J}"*.tmp2 2>/dev/null || true
        rm -rf ./plots/*.plot ./plots/*.tmp 2>/dev/null || true
    done
    
    echo "Completed testing on $DRIVE_NAME"
    echo ""
done

echo "Memory sweep complete across all drives! Results saved to ./data/comparison-to-merge.csv"
echo "Individual monitoring data saved to ./data/<drive>-monitor-k34-<memory>.csv"
echo "Monitoring plots saved to ./graphs/<drive>-monitor-plot-k34-<memory>.svg"
