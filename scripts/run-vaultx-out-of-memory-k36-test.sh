#!/bin/bash

mkdir -p data logs

HOSTNAME=$(hostname)
max_k=36

case $HOSTNAME in
    "epycbox")
        # max_k=34
        max_ram=131072
        thread_num=64
        disks=("/ssd-raid0" "/data-l" "/data-fast2")
        make_name="vaultx_x86_c"
        ;;
    "orangepi5plus")
        # max_k=31
        max_ram=32768
        thread_num=8
        disks=("/data-fast" "/data-a")
        make_name="vaultx_arm_c"
        ;;
    "raspberrypi5")
        # max_k=28
        max_ram=4096
        thread_num=4
        disks=("/data-fast" "/data-a")
        make_name="vaultx_arm_c"
        ;;
    *)
        echo "This script is not intended to be run on this machine."
        exit 1
        ;;
esac


run_tests() {
    local nonce_size=$1
    local k=$2
    local mount_path=$3

    local mem_min=512
    local mem_max=$((2**k*$nonce_size/1024/1024))
    if  [ $mem_max -gt $max_ram ]; then
        mem_max=$max_ram
    fi

    local memory=$mem_min
    #while [ $memory -le $mem_max ]; do
    for memory in 1280 20480; do 
        #for i in $(seq 1 3)
        #do
            echo "Running vaultx with K=$k, memory=$memory MB, run $i ..."
            ./scripts/drop-all-caches.sh

            ./vaultx -a for -t $thread_num -K $k -m $memory -b 1024 -f "$mount_path/" -g "$mount_path/" -j "$mount_path/" -x true -v true >> "$data_file" & 
            vaultx_pid=$!

            pidstat_log="logs/disk${disk_name}_k${k}_m${memory}_pidstat.log"
            echo "----Run $i for K=$k, memory=$memory MB----" >> "$pidstat_log"
            pidstat -h -r -u -d -p $vaultx_pid 1 >> "$pidstat_log" &

            wait $vaultx_pid
            if [ $? -ne 0 ]; then
                echo "vaultx failed with K=$k, memory=$memory MB, run $i"
            fi

            rm -r "$mount_path/*"
        #done
        #memory=$((memory * 2))
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

    data_file="data/vaultx-$HOSTNAME-$disk_name-out-of-memory.csv"

    echo "APPROACH,K,sizeof(MemoRecord),num_threads,MEMORY_SIZE(MB),file_size(GB),BATCH_SIZE,total_throughput(MH/s),total_throughput(MB/s),elapsed_time_hash_total,elapsed_time_hash2_total,elapsed_time_io_total,elapsed_time_shuffle_total,other_time,elapsed_time,storage_efficiency" > "$data_file"

    # if [ $max_k -le 32 ]; then
    #     k=$max_k
    # else
    #     k=32
    # fi

    #make clean
    #make $make_name NONCE_SIZE=4 RECORD_SIZE=16

    # Run tests for NONCE_SIZE=4
    #for K in $(seq 28 32); do
    #    echo "Running tests for K=$K with $thread_num threads on $disk_name disk"
    #    run_tests 4 $K $mount_path
    #done

    make clean
    make $make_name NONCE_SIZE=5 RECORD_SIZE=16

    run_tests 5 36 $mount_path

    # Run tests for NONCE_SIZE=5
    #for K in $(seq 33 $max_k); do
    #    echo "Running tests for K=$K with $thread_num threads on $disk_name disk"
    #    run_tests 5 $K $mount_path
    #done
done
