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

echo "approach,K,nonce_size,num_threads,MEMORY_SIZE_MB,file_size_gb,BATCH_SIZE,total_throughput,total_throughput*NONCE_SIZE,elapsed_time_hash_total,elapsed_time_io_total,elapsed_time_io2_total,elapsed_time-elapsed_time_hash_total-elapsed_time_io_total-elapsed_time_io2_total,elapsed_time" > "$data_file"

for k in $(seq 1 $max_k); do
    if [ "$k -lt 32"]; then
        nonce_size=4
    else
        nonce_size=5
    fi

    echo "Building vaultx with NONCE_SIZE=$nonce_size for K=$k ..."
    make clean
    make $make_name RECORD_SIZE=16 NONCE_SIZE=$nonce_size

    for i in $(seq 1 5); do 
        echo "Running vaultx with K=$k, run $i ..."
        ./vaultx -a for -t $threads -K $k -m $memory -b 1024 -f memo.t -g memo.x -j memo.xx -x true >> "$data_file"

    done
done

echo "Data collection complete. Results saved to $data_file."
