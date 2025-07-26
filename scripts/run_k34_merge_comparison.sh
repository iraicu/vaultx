#!/bin/bash

# Simple K=34 memory sweep script
# Tests VaultX with different memory values from 320MB to 163840MB

set -e

# Memory values to test (in MB)
MEMORY_VALUES=(320 640 1280 2560 5120 10240 20480 40960 81920 163840)

make clean
make vaultx_x86_c NONCE_SIZE=4 RECORD_SIZE=16

# Create directories if they don't exist
mkdir -p ./data
mkdir -p ./plots
mkdir -p ./graphs

echo "Starting K=34 memory sweep tests..."
echo "approach,K,record_size,num_threads,memory_mb,file_size_gb,batch_size,write_batch_size,throughput_mhs,throughput_mbs,hash_time,hash2_time,io_time,io2_time,other_time,total_time,match_percentage,storage_efficiency" > ./data/comparison-to-merge.csv

for MEMORY in "${MEMORY_VALUES[@]}"; do
    echo "Testing with ${MEMORY}MB memory..."
    
    # Clean previous run
    rm -rf ./plots/*.plot ./plots/*.tmp 

    ./scripts/drop-all-caches.sh
    
    # Run VaultX
    ./scripts/vaultx_system_monitor_pidstat.py --plot-file ./graphs/monitor-plot-k34-$MEMORY.svg ./vaultx -a for -K 28 -m $MEMORY -W $MEMORY -t 64 -f ./plots/ -g ./plots/ -j ./plots/ -M 1 -x true -n true >> ./data/comparison-to-merge.csv
    
    # Clean previous run
    rm -rf ./plots/*.plot ./plots/*.tmp
done

echo "Memory sweep complete! Results saved to ./data/comparison-to-merge.csv"
