#!/bin/bash

mkdir -p data

HOSTNAME=$(hostname)

case $HOSTNAME in 
    "epycbox")
        max_ram=524288
        thread_num=128
        disks=("/ssd-raid0" "/data-l" "/data-fast2")
        ;;
    "orangepi5plus")
        max_ram=8192
        thread_num=16
        disks=("/data-fast" "data-a")
        ;;
    "raspberrypi5")
        max_ram=4096
        thread_num=4
        disks=("/data-fast" "data-a")
        ;;
    *)
        echo "This script is not intended to be run on this machine."
        exit 1
        ;;
esac


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

    mount_path="$disk/varvara"

    if [ ! -d "$mount_path" ]; then
        echo "Creating directory $mount_path"
        mkdir -p "$mount_path"
    fi

    data_file="data/vaultx-$HOSTNAME-$disk-out-of-memory.csv"

    echo "APPROACH,K,sizeof(MemoRecord),num_threads,MEMORY_SIZE(MB),file_size(GB),BATCH_SIZE,total_throughput(MH/s),total_throughput(MB/s),elapsed_time_hash_total,elapsed_time_io_total,elapsed_time_io2_total,elapsed_time - elapsed_time_hash_total - elapsed_time_io_total - elapsed_time_io2_total,elapsed_time" > "$data_file"
done