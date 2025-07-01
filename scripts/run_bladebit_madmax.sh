#!/bin/bash

rm -r /ssd-raid0/varvara/plot/*
./drop-all-caches.sh
chia plotters bladebit ramplot -d /ssd-raid0/varvara/plot -n 1 -r 64 --compress 0 > "/data-l/varvara/vaultx/data/bladebit-sata-output.txt"

rm -r /mnt/ram/varvara/plot/*
rm -r /ssd-raid0/varvara/plot/*
./drop-all-caches.sh
chia plotters madmax -r 64 -t /mnt/ram/varvara/plot -d /ssd-raid0/varvara/plot -n 1 > "/data-l/varvara/vaultx/data/madmax-sata-output.txt"

rm -r /mnt/ram/varvara/plot/*
rm -r /data-fast2/varvara/plot/*
./drop-all-caches.sh
chia plotters madmax -r 64 -t /mnt/ram/varvara/plot -d /data-fast2/varvara/plot -n 1 > "/data-l/varvara/vaultx/data/madmax-nvme-output.txt"

rm -r /mnt/ram/varvara/plot/*
rm -r /data-fast2/varvara/plot/*
./drop-all-caches.sh
chia plotters chiapos -t /mnt/ram/varvara/plot -b 262144 -d /data-fast2/varvara/plot -r 64 > "/data-l/varvara/vaultx/data/chiapos-nvme-output.txt"

rm -r /mnt/ram/varvara/plot/*
rm -r /data-fast2/varvara/plot/*
rm -r /ssd-raid0/varvara/plot/*
./drop-all-caches.sh
chia plotters chiapos -t /mnt/ram/varvara/plot -b 262144 -d /ssd-raid0/varvara/plot -r 64 > "/data-l/varvara/vaultx/data/chiapos-sata-output.txt"

rm -r /mnt/ram/varvara/plot/*
rm -r /data-l/varvara/plot/*
./drop-all-caches.sh
chia plotters chiapos -t /mnt/ram/varvara/plot -b 262144 -d /data-l/varvara/plot -r 64 > "/data-l/varvara/vaultx/data/chiapos-hdd-output.txt"
