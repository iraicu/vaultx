#!/bin/bash

# Arrays for number of files (n) and batch memory size (B)
# Corresponds to k=33 to k=40
n_values=(2 4 8 16 32 64 128 256)
b_values=(128 256 512 1024 2048 4096 8192 16384)

# Ensure we are in the root directory if the script is run from scripts/
# Logic: checking if vaultx binary is present in current directory, if not assume we might be in scripts/ and cd ..
if [ ! -f "./vaultx" ]; then
    if [ -f "../vaultx" ]; then
         cd ..
    fi
fi

# Loop from indices 0 to 7 to cover k=33 to k=40
for i in {0..7}; do
    k=$((33 + i))
    n=${n_values[$i]}
    b=${b_values[$i]}
    
    # Determine drive name
    if [ "$k" -eq 40 ]; then
        drive_name="data-q"
    else
        drive_name="data-r"
    fi

    echo "=================================================="
    echo "Starting merge for k=$k (n=$n, B=$b, drive=$drive_name)"
    echo "=================================================="

    # Clear cache
    echo "Clearing cache..."
    sudo sync && echo 3 | sudo tee /proc/sys/vm/drop_caches
    sleep 2 # Give it a moment

    # Construct and execute command
    log_file="log_k32_n${n}_t1_m${b}_merge_hdd_hdd.txt"
    
    echo "Executing command..."
    # Using eval to handle the complexity of the command string with bash -c and pipes
    cmd="stdbuf -o0 -e0 bash -c 'time ./vaultx -P merge -k 32 -n $n -F /data-l/iraicu/tmp/ -T /$drive_name/iraicu/vaults/ -t 1 -R 1024  -A pipelined -B $b' 2>&1 | tee ./logs/$log_file"
    
    echo "$cmd"
    eval "$cmd"
    
    echo "Finished merge for k=$k"
    echo ""
done
