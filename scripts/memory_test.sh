#!/bin/bash

# Simple VaultX K32 Memory Test Script
# Tests K=32 with match_factor=0.83706 across different memory values
# Usage: ./memory_test.sh

# Memory values to test (MB)
# MEMORY_VALUES=(320 640 1280 2560 5120 10240 20480 40960)
MEMORY_VALUES=(1280 2560 5120 10240 20480)

# Fixed parameters
K=32
MATCH_FACTOR=0.83706

echo "Testing VaultX K=$K with match_factor=$MATCH_FACTOR"
echo "Memory values: ${MEMORY_VALUES[*]} MB"
echo ""

# Create results file
RESULTS_FILE="./data/memory_test_results.csv"
echo "memory_mb,total_time,throughput_mh,io_throughput,storage_efficiency" > "$RESULTS_FILE"

make clean
make vaultx_x86_c NONCE_SIZE=4 RECORD_SIZE=16

for memory in "${MEMORY_VALUES[@]}"; do
    echo "=== Testing Memory: $memory MB ==="
    
    # Clean plots directory
    rm -rf ./plots/*
    rm -rf /stor/substor0/varvara/*
    
    # Run VaultX
    ./vaultx -a for -K $K -m $memory -W $memory -M $MATCH_FACTOR -f ./plots/ -g /stor/substor0/varvara/ -j ./plots/ -t 128 -x true > output.txt 2>&1
    
    if [ $? -eq 0 ]; then
        # Extract metrics from last CSV line
        csv_line=$(grep -E '.*,.*%.*,.*%' output.txt | tail -1)
        if [ -n "$csv_line" ]; then
            IFS=',' read -ra VALUES <<< "$csv_line"
            total_time="${VALUES[17]}"
            throughput="${VALUES[8]}"
            io_throughput="${VALUES[11]}"
            storage_eff="${VALUES[19]//\%/}"
            
            echo "  Time: ${total_time}s, Throughput: ${throughput} MH/s, Storage: ${storage_eff}%"
            echo "$memory,$total_time,$throughput,$io_throughput,$storage_eff" >> "$RESULTS_FILE"
        else
            echo "  FAILED - No CSV output"
            echo "$memory,FAILED,FAILED,FAILED,FAILED" >> "$RESULTS_FILE"
        fi
    else
        echo "  FAILED - VaultX error"
        echo "$memory,FAILED,FAILED,FAILED,FAILED" >> "$RESULTS_FILE"
    fi
    
    echo ""
done

echo "Results saved to: $RESULTS_FILE"
echo "Done!"
