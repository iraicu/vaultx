#!/bin/bash

mkdir -p data logs

HOSTNAME=$(hostname)
max_k=36

temp_path="/ssd-raid0/varvara/vaultx/plots"
dest_path="/data-l/varvara/vaultx/plots"

mkdir -p "$temp_path" "$dest_path"

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
        max_ram=8192
        thread_num=8
        disks=("/data-a")
	#disks=("/data-fast" "/data-a")
        make_name="vaultx_arm_c"
        ;;
    "raspberrypi5")
        # max_k=28
        max_ram=2048
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

    for memory in 1280 20480; do 
            echo "Running vaultx with K=$k, memory=$memory MB..."
            ./scripts/drop-all-caches.sh

            data_file="data/vaultx-${HOSTNAME}-caching-k${k}-m${memory}.csv"
            pidstat_log="logs/epycbox_caching_k${k}_m${memory}_pidstat.log"

    	    echo "APPROACH,K,sizeof(MemoRecord),num_threads,MEMORY_SIZE(MB),file_size(GB),BATCH_SIZE,total_throughput(MH/s),total_throughput(MB/s),elapsed_time_hash_total,elapsed_time_hash2_total,elapsed_time_io_total,elapsed_time_shuffle_total,other_time,elapsed_time,storage_efficiency" > "$data_file"
	    echo "---- Run with K=$k, memory=$memory Mb ----" > "$pidstat_log"

            ./vaultx -a for -t $thread_num -K $k -m $memory -b 1024 -f "$temp_path/" -g "$temp_path/" -j "$dest_path/" -x true -v true >> "$data_file" & 
            vaultx_pid=$!

            pidstat -h -r -u -d -p $vaultx_pid 1 >> "$pidstat_log" &

            wait $vaultx_pid
            if [ $? -ne 0 ]; then
                echo "vaultx failed with K=$k, memory=$memory MB"
            fi

	    rm -r /ssd-raid0/varvara/vaultx/plots/*
            rm -r /data-l/varvara/vaultx/plots/*.plot 
        #done
        #memory=$((memory * 2))
    done
}

    make clean
    make $make_name NONCE_SIZE=5 RECORD_SIZE=16

    run_tests 5 $max_k

done
