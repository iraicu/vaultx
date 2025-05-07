#!/bin/bash

set -e
set -o pipefail

mkdir -p data

case $(hostname) in
    "eightsocket")
        max_k=35
        memory=524288
        threads=256
        make_name="vaultx_x86_c"
        disks=("/stor/substor1")
        ;;
    "epycbox")
        max_k=34
        memory=262144
        threads=128
        make_name="vaultx_x86_c"
        disks=("/ssd-raid0" "/data-l" "/data-fast2")
        ;;
    "orangepi5plus")
        max_k=31
        memory=32768
        threads=8
        make_name="vaultx_arm_c"
        disks=("/data-fast" "/data-a")
        ;;
    "raspberrypi5")
        max_k=28
        memory=4096
        threads=4
        make_name="vaultx_arm_c"
        disks=("/data-fast" "/data-a")
        ;;
    *)
        echo "Unknown hostname: $(hostname)"
        exit 1
        ;;
esac

run_tests() {
    local nonce_size=$1
    local k_start=$2
    local k_end=$3
    local mount_path=$4

    make clean
    make $make_name NONCE_SIZE=$nonce_size RECORD_SIZE=16

    for k in $(seq $k_start $k_end)
    do
        for i in $(seq 1 3)
        do
            echo "Running vaultx with K=$k, run $i ..."
            ./scripts/drop-all-caches.sh
            ./vaultx -a for -t $threads -K $k -m $memory -b 1024 -f "$mount_path/memo.t" -g "$mount_path/memo.x" -j "$mount_path/memo.xx" -x true >> "$data_file"
            rm -f memo.t memo.x memo.xx
        done
    done
}

for disk in "${disks[@]}"; do 
    echo "Running benchmarks on $disk disk"

    case $disk in
        "/stor/substor1")
            disk_name="nvme"
            ;;
        "/data-fast2")
            disk_name="nvme"
            ;;
        "/ssd-raid0")
            disk_name="sata"
            ;;
        "/data-fast")
            disk_name="nvme"
            ;;
        "/data-a")
            disk_name="hdd"
            ;;
        "data-l")
            disk_name="hdd"
            ;;
        *)
            echo "Unknown disk: $disk"
            exit 1
            ;;
    esac

    mount_path="$disk/varvara"

    if [ ! -d "$mount_path" ]; then
        echo "Creating directory $mount_path"
        mkdir -p "$mount_path"
    fi

    data_file="data/vaultx-$(hostname)-$disk_name.csv"

    echo "APPROACH,K,NONCE_SIZE(B),NUM_THREADS,MEMORY_SIZE(MB),FILE_SIZE(GB),BATCH_SIZE,THROUGHPUT(MH/S),THROUGHPUT(MB/S),HASH_TIME,IO_TIME,SHUFFLE_TIME,OTHER_TIME,TOTAL_TIME,STORAGE_EFFICIENCY" > "$data_file"

    # Run tests for NONCE_SIZE=4
    if [ $max_k -le 31 ]; then
        run_tests 4 25 $max_k $mount_path
    else
        run_tests 4 25 31 $mount_path
    fi

    # Run tests for NONCE_SIZE=5
    if [ $max_k -ge 32 ]; then
        run_tests 5 32 $max_k $mount_path
    fi 
    
    echo "Data collection on $disk disk complete. Results saved to $data_file."

done


