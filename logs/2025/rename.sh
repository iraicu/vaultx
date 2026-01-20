!/bin/bash

LOG_DIR="/data-fast/varvara/vaultx/logs"

# List of files to rename (excluding those with 'k36' in their name)
files=(
    "_k28_m1024_pidstat.log"
    "_k29_m2048_pidstat.log"
    "_k30_m512_pidstat.log"
    "k28_m1024_pidstat.log"
    "k29_m2048_pidstat.log"
    "_k28_m512_pidstat.log"
    "_k29_m512_pidstat.log"
    "_k31_m512_pidstat.log"
    "k28_m512_pidstat.log"
    "k29_m512_pidstat.log"
    "_k29_m1024_pidstat.log"
    "_k30_m1024_pidstat.log"
    "k29_m1024_pidstat.log"
)

for file in "${files[@]}"; do
    old_path="${LOG_DIR}/${file}"
    new_name="rpi5_${file}"
    new_path="${LOG_DIR}/${new_name}"
    echo "Renaming: ${old_path} -> ${new_path}"
    mv "${old_path}" "${new_path}"
done

echo "Renaming complete."

