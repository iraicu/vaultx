#!/bin/bash

SECONDS=0
HOSTNAME=$(hostname)

case $HOSTNAME in
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
    disks=("/ssd-raid0" "/data-fast2" "/data-l")
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
    echo "Unknown hostname: $HOSTNAME"
    exit 1
    ;;
esac

run_tests() {
    local nonce_size=$1
    local k_start=$2
    local k_end=$3
    local file_path=$4
    local join_path=$5
    local output_file=$6
    local lookup_data_file=$7
    local file_size_log_file="${file_path}/file_sizes.csv"

    make clean
    make $make_name NONCE_SIZE=$nonce_size RECORD_SIZE=16

    # Create file size CSV with headers if it doesn't exist
    if [ ! -f "$file_size_log_file" ]; then
        echo "K_value,file_size_GB" > "$file_size_log_file"
    fi

    for k in $(seq $k_start $k_end); do
        echo "=== Running vaultx with K=$k ==="
        k_start_time=$(date +%s)

        ./scripts/drop-all-caches.sh
        ./vaultx -a for -t "$threads" -K "$k" -m "$memory" -b 1024 -f "$file_path/memo.t" -g "$file_path/memo.x" -j "$join_path/memo.xx" -x true >>"$output_file"

        if [ -f "$file_path/memo.x" ]; then
            size_output=$(du -BG "$file_path/memo.x" | cut -f1)
            file_size_gb="${size_output%G}"  # Remove 'G' suffix
        else
            file_size_gb=0
        fi

        echo "$k,$file_size_gb" >> "$file_size_log_file"
        echo "File size of memo.x after K=$k: ${file_size_gb} GB"

        for search_size in 3 4 8 16 32; do
            echo "Running vaultx with K=$k, search size $search_size ..."
            ./scripts/drop-all-caches.sh
            ./vaultx -a for -t $threads -K $k -m $memory -j "$join_path/memo.xx" -p $search_size -x true >>"$lookup_data_file"
        done

        rm -f memo.t memo.x memo.xx

        k_end_time=$(date +%s)
        echo "=== Finished vaultx with K=$k in $((k_end_time - k_start_time)) seconds ==="
    done
}

nvme_disk=""

for disk in "${disks[@]}"; do
    case $disk in
    "/stor/substor1" | "/data-fast2" | "/data-fast")
        nvme_disk="$disk"
        break
        ;;
    esac
done

for disk in "${disks[@]}"; do
    echo "=== Starting benchmarks on $disk ==="
    disk_start_time=$(date +%s)

    case $disk in
    "/stor/substor1" | "/data-fast2" | "/data-fast")
        disk_name="nvme"
        ;;
    "/ssd-raid0")
        disk_name="sata"
        ;;
    "/data-a" | "/data-l")
        disk_name="hdd"
        ;;
    *)
        echo "Unknown disk: $disk"
        exit 1
        ;;
    esac

    mount_path="$disk/varvara"
    data_file="data/vaultx-$HOSTNAME-$disk_name-extra-run.csv"
    cached_gen_data_file="data/vaultx-$HOSTNAME-caching.csv"
    lookup_data_file="data/vaultx-$HOSTNAME-$disk_name-lookup.csv"

    echo "APPROACH,K,NONCE_SIZE(B),NUM_THREADS,MEMORY_SIZE(MB),FILE_SIZE(GB),BATCH_SIZE,THROUGHPUT(MH/S),THROUGHPUT(MB/S),HASH_TIME,IO_TIME,SHUFFLE_TIME,OTHER_TIME,TOTAL_TIME,STORAGE_EFFICIENCY" >"$data_file"
    echo "APPROACH,K,NONCE_SIZE(B),NUM_THREADS,MEMORY_SIZE(MB),FILE_SIZE(GB),BATCH_SIZE,THROUGHPUT(MH/S),THROUGHPUT(MB/S),HASH_TIME,IO_TIME,SHUFFLE_TIME,OTHER_TIME,TOTAL_TIME,STORAGE_EFFICIENCY" >"$cached_gen_data_file"
    echo "FILENAME,NUM_THREADS,FILE_SIZE(GB),NUM_BUCKETS_SEARCH,NUM_RECORDS_IN_BUCKET,NUM_LOOKUPS,SEARCH_SIZE,FOUND_RECORDS,NOT_FOUND_RECORDS,TOTAL_TIME,TIME_PER_LOOKUP" >"$lookup_data_file"

    if [ "$disk_name" == "hdd" ]; then
        if [ -z "$nvme_disk" ]; then
            echo "Skipping tests for $disk disk as no NVMe disk is available."
            continue
        fi

        echo "Using HDD ($disk) for file generation and NVMe ($nvme_disk) for caching"
        run_tests 4 25 $((max_k < 31 ? max_k : 31)) "$mount_path" "$nvme_disk/varvara" "$cached_gen_data_file" "$lookup_data_file"

        if [ $max_k -ge 32 ]; then
            run_tests 5 32 $max_k "$mount_path" "$nvme_disk/varvara" "$cached_gen_data_file" "$lookup_data_file"
        fi

        echo "Data collection on $disk disk complete. Results saved to $cached_gen_data_file and $lookup_data_file."
    else
        # Regular run — same mount path used for -f and -j
        run_tests 4 25 $((max_k < 31 ? max_k : 31)) "$mount_path" "$mount_path" "$data_file" "$lookup_data_file"

        if [ $max_k -ge 32 ]; then
            run_tests 5 32 $max_k "$mount_path" "$mount_path" "$data_file" "$lookup_data_file"
        fi

        echo "Data collection on $disk disk complete. Results saved to $data_file and $lookup_data_file."
    fi

    disk_end_time=$(date +%s)
    echo "=== Benchmarks on $disk took $((disk_end_time - disk_start_time)) seconds ==="
done

total_time=$SECONDS
echo "=== Total time for all benchmarks: $((total_time / 60)) minutes and $((total_time % 60)) seconds ==="
