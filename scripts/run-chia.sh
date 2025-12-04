#!/bin/bash

HOSTNAME=$(hostname)

case $HOSTNAME in
    "eightsocket")
        memory=524288
        threads=256
        make_name="vaultx_x86_c"
        disks=("/stor/substor1")
        ;;
    "epycbox")
        memory=262144
        threads=128
        make_name="vaultx_x86_c"
        disks=("/ssd-raid0" "/data-l" "/data-fast2")
        ;;
    "orangepi5plus")
        memory=32768
        threads=8
        make_name="vaultx_arm_c"
        disks=("/data-fast" "/data-a")
        ;;
    "raspberrypi5")
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

k=32

ramdisk=/mnt/ram
sudo mkdir -p $ramdisk
sudo mount -t tmpfs -o size=340G tmpfs $ramdisk

output="chia-$HOSTNAME.csv"
echo "PLOTTER,K,THREADS,TOTAL_TIME_SEC" > "$output"

run_plotter() {
    local plotter_name="$1"
    shift
    local cmd=("$@")

    echo "=== Running $plotter_name ==="

    start_time=$(date +%s)

    # Run the command and capture the elapsed time
    { /usr/bin/time -f "%e" "${cmd[@]}" ; } 2> time_output.txt

    end_time=$(date +%s)
    elapsed_time=$(cat time_output.txt | tail -n 1 | xargs)

    echo "$plotter_name,$max_k,$threads,$elapsed_time" >> "$output"
}

# Run bladebit ramplot across all disks
for disk in "${disks[@]}"; do
    ./drop-all-caches.sh
    run_plotter "bladebit-ram" chia plotters bladebit ramplot -d "$disk/varvara/plot" -n 1 -r $threads
    
    ./drop-all-caches.sh
    run_plotter "madmax" chia plotters madmax -r $threads -t "$ramdisk/varvara/plot" -d "$disk/varvara/plot/" -n 1

    ./drop-all-caches.sh
    buffer_size=$((memory / 8))
    run_plotter "chiapos" chia plotters chiapos -r $threads -t "$ramdisk/varvara/plot" -d "$disk/varvara/plot" -n 1 -b $buffer_size
done


