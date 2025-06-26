#!/bin/bash

mkdir -p data

HOSTNAME=$(hostname)

case $HOSTNAME in 
    "epycbox")
        max_k=34
        max_ram=262144
        thread_num=128
        disks=("/ssd-raid0" "/data-l" "/data-fast2")
        ;;
    "orangepi5plus")
        max_k=31
        max_ram=32768
        thread_num=8
        disks=("/data-fast" "data-a")
        ;;
    "raspberrypi5")
        max_k=28
        max_ram=4096
        thread_num=4
        disks=("/data-fast" "data-a")
        ;;
    *)
        echo "This script is not intended to be run on this machine."
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

    local mem_min=256
    local exponent=$((k - 28))
    local mem_max=$((256 << exponent)) # 256 * 2^(k-28)
    if  [ $mem_max -gt $max_ram ]; then
        mem_max=$max_ram
    fi

    local memory=$mem_min
    while [ $memory -le $mem_max ]; do
        for i in $(seq 1 3)
        do
            echo "Running vaultx with K=$k, run $i ..."
            ./scripts/drop-all-caches.sh
            ./vaultx -a for -t $threads -K $k -m $memory -b 1024 -f "$mount_path/" -g "$mount_path/" -j "$mount_path/" -x true -v true >> "$data_file"
            rm -f plots/*.plot
        done
        memory=$((memory * 2))
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
        "/data-l")
            disk_name="hdd"
            ;;
        *)
            echo "Unknown disk: $disk"
            exit 1
            ;;
    esac

    mount_path="$disk/varvara/vaultx/plots"

    if [ ! -d "$mount_path" ]; then
        echo "Creating directory $mount_path"
        mkdir -p "$mount_path"
    fi

    data_file="data/vaultx-$HOSTNAME-$disk-out-of-memory.csv"

    echo "APPROACH,K,sizeof(MemoRecord),num_threads,MEMORY_SIZE(MB),file_size(GB),BATCH_SIZE,total_throughput(MH/s),total_throughput(MB/s),elapsed_time_hash_total,elapsed_time_hash2_total,elapsed_time_io_total,elapsed_time_shuffle_total,other_time,elapsed_time,storage_efficiency" > "$data_file"

    # Run tests for NONCE_SIZE=4
    if [ $max_k -le 32 ]; then
        run_tests 4 25 $max_k $mount_path
    else
        run_tests 4 25 32 $mount_path
    fi

    # Run tests for NONCE_SIZE=5
    if [ $max_k -ge 33 ]; then
        run_tests 5 33 $max_k $mount_path
    fi 
done