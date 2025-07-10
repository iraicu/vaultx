#!/bin/bash

disks=('/ssd-raid0' '/data-l')

make vaultx_x86_c NONCE_SIZE=4 RECORD_SIZE=16

hostname=$(hostname)

for disk in "${disks[@]}"; do
    echo "Processing disk: $disk"
    mkdir -p "$disk/varvara/plot/"

    case $disk in
        /ssd-raid0)
            disk_type="satassd"
            ;;
        /data-l)
            disk_type="hdd"
            ;;
        *)
            echo "Unknown disk type."
            ;;
    esac

    data_file="data/$hostname-$disk_type-write-batch-size-check.txt"

    echo "Starting write batch size check for disk: $disk"
    echo "Data will be saved to: $data_file"

    echo "APPROACH,K,sizeof(MemoRecord),num_threads,MEMORY_SIZE(MB),file_size(GB),BATCH_SIZE,total_throughput(MH/s),total_throughput(MB/s),elapsed_time_hash_total,elapsed_time_hash2_total,elapsed_time_io_total,elapsed_time_shuffle_total,other_time,elapsed_time,storage_efficiency" > "$data_file"

    # Run the vaultx command with the specified parameters
    for ((W=1024; W <= 16777216; W*=2)) ; do
        echo "Running with WRITE_BATCH_SIZE=$W"
        for i in {1..3}; do
            echo "Iteration $i for WRITE_BATCH_SIZE=$W"
            ./scripts/drop-all-caches.sh
            ./vaultx -v true -a for -f "$disk/varvara/plot/" -g "$disk/varvara/plot/" -j "$disk/varvara/plot/" -t 64 -K 29 -m 512 -W "$W" -R 1 -x true >> "$data_file"
        done
    done
    
    echo "Write batch size check completed for disk: $disk"
done

echo "All disks processed"
