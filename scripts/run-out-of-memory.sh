#!/bin/bash

# The script explores performance, storage efficiency, and memory usage of out-of-memory VaultX protocol

case $(hostname) in
    "eightsocket")
        echo "Running on eightsocket"
        memory_max=524288
        thread_num=128
        disk_folder="/stor/substor1/varvara/vaultx/"
        ;;
    "orangepi5plus")
        echo "Running on opi5"
        memory_max=16384
        thread_num=8
        # has 2 folders: /data-l/varvara/vaultx and /data-fast...
        # needs to run on both
        ;;
    "raspberrypi5")
        echo "Running on rpi5"
        memory_max=4096
        thread_num=4
        # same 2 folders 
        ;;
    *)
        echo "Running on unknown host"
        exit 1
        ;;
esac

# TODO: Add disk type detection logic

make vaultx_x86_c NONCE_SIZE=4 RECORD_SIZE=16

output_file="data/vaultx-$hostname-$disk_type-outofmemory.csv"

echo "approach,K,sizeof(MemoRecord),num_threads,MEMORY_SIZE(MB),file_size(GB),BATCH_SIZE,total_throughput(MH/S),total_throughput(MB/S),elapsed_time_hash_total,elapsed_time_io_total,elapsed_time_io2_total,elapsed_time - elapsed_time_hash_total - elapsed_time_io_total - elapsed_time_io2_total,elapsed_time" > "$output_file"

for k in {25..35};
do
    echo "Testing k=$k"
    plot_size=$((2**k))
    for m in {};
    do 
        ./drop-all-caches.sh
        ./vaultx -a for -t $thread_num -f $disk_folder -g $disk_folder -j $disk_folder -K $k -m ... -x true >> $output_file
        rm -r ../*.plot
    done
done