#!/bin/bash

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

make clean
make vaultx NONCE_SIZE=4 RECORD_SIZE=16
./vaultx -a for -t $threads -K 31 -m $memory -b 1024 -f "/data-fast/memo.t" -g "/data-fast/memo.x" -j "/data-fast/memo.xx" -x true

for k in $(seq 1 $max_k)
do
    for 
done