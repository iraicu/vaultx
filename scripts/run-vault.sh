#!/bin/bash

set -e 
set -o pipefail

data_file="data/vaultx-$(hostname).csv"
mkdir -p data

case $(hostname) in 
    "eightsocket")
        max_k=35
        memory=524288
        threads=256
        make_name="vaultx_x86_c"
        ;;
    "epycbox")
        max_k=34
        memory=262144
        threads=128
        make_name="vaultx_x86_c"
        ;;
    "orangepi5plus")
        max_k=31
        memory=32768
        threads=8
        make_name="vaultx_arm_c"
        ;;
    "raspberrypi5")
        max_k=28
        memory=4096
        threads=4
        make_name="vaultx_arm_c"
        ;;
    *)
        echo "Unknown hostname: $(hostname)"
        exit 1
        ;;
esac

echo "APPROACH,K,NONCE_SIZE(B),NUM_THREADS,MEMORY_SIZE(MB),FILE_SIZE(GB),BATCH_SIZE,THROUGHPUT(MH/S),THROUGHPUT(MB/S),HASH_TIME,IO_TIME,IO2_TIME,REST_TIME,TOTAL_TIME,STORAGE_EFFICIENCY" > "$data_file"

run_tests() {
    local nonce_size=$1
    local k_start=$2
    local k_end=$3

    make clean
    make $make_name NONCE_SIZE=$nonce_size RECORD_SIZE=16

    for k in $(seq $k_start $k_end)
    do
        for i in $(seq 1 3)
        do
            echo "Running vaultx with K=$k, run $i ..."
            ./scripts/drop-all-caches.sh
            ./vaultx -a for -t $threads -K $k -m $memory -b 1024 -f memo.t -g memo.x -j memo.xx -x true >> "$data_file"
            rm -f memo.t memo.x memo.xx
        done
        
    done
}

# Run tests for NONCE_SIZE=4
run_tests 4 25 31

# Run tests for NONCE_SIZE=5
run_tests 5 32 35

echo "Data collection complete. Results saved to $data_file."
