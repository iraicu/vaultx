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
        disks=("/ssd-raid0" "data-l")
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
        total_records=$((1 << k))
        table1_Mbytes=$((total_records * 4 / 1024 / 1024))
        table2_Mbytes=$((total_records * 8 / 1024 / 1024))
        total_Mbytes=$((table1_Mbytes + table2_Mbytes))

        echo "K=$k will need ~${total_Mbytes}MB to store all hashes."

        for memory_mb in 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288
        do
            # if [[ $k -ne 25 && $memory_mb -eq 256 ]]; then 
            #     continue
            # fi

            if [ $memory_mb -gt $total_Mbytes ]; then
                echo "Memory size $memory_mb MB fits all records for K=$k. Skipping..."
                continue
            fi

            echo "Running vaultx with K=$k, NONCE_SIZE=$nonce_size, MEMORY_SIZE=${memory_mb}MB..."

            ./scripts/drop-all-caches.sh
            ./vaultx -a for -t $threads -K $k -m $memory_mb -b 1024 -f "$mount_path/memo.t" -j "$mount_path/memo.xx" -x true >> "$data_file"
            rm -f $mount_path/memo.t $mount_path/memo.xx
        done
    done
}

for disk in "${disks[@]}"; do 
    echo "Running benchmarks on $disk disk"

    case $disk in
        "/stor/substor1")
            disk_name="ssd"
            ;;
        "/data-fast2")
            disk_name="nvme"
            ;;
        "/ssd-raid0")
            disk_name="ssd"
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

    mount_path="$disk/varvara"

    if [ ! -d "$mount_path" ]; then
        echo "Creating directory $mount_path"
        mkdir -p "$mount_path"
    fi

    data_file="data/vaultx-$(hostname)-$disk_name-out-of-memory.csv"

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


