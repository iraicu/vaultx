#!/bin/bash

# Script to run vaultx for K25-K34 with varying -M argument
# The -M values are read from the match_percentage column in data/match_factor.csv
# This script tests different matching factors based on observed match rates

set -e  # Exit on any error

# Configuration
INPUT_CSV="data/match_factor.csv"
OUTPUT_FILE="data/variable_match_factor.csv"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Create data directory if it doesn't exist
mkdir -p "$PROJECT_DIR/data"

# Check if input CSV exists
if [ ! -f "$PROJECT_DIR/$INPUT_CSV" ]; then
    echo "Error: Input CSV file $INPUT_CSV not found!"
    exit 1
fi

# Initialize output CSV file with headers
echo "approach,K,record_size,num_threads,memory_mb,file_size_gb,batch_size,write_batch_size,throughput_mhs,throughput_mbs,hash_time,hash2_time,io_time,io2_time,other_time,total_time,match_percentage,storage_efficiency,match_factor_used" > "$OUTPUT_FILE"

echo "Starting variable match factor experiment for K25-K34..."
echo "Reading match percentages from: $INPUT_CSV"
echo "Results will be saved to: $OUTPUT_FILE"

# Function to compile vaultx with specific NONCE_SIZE
compile_vaultx() {
    local nonce_size=$1
    echo "Compiling vaultx with NONCE_SIZE=$nonce_size..."
    cd "$PROJECT_DIR"
    make clean > /dev/null 2>&1
    make vaultx_x86_c NONCE_SIZE=$nonce_size RECORD_SIZE=16 > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "Error: Failed to compile vaultx with NONCE_SIZE=$nonce_size"
        exit 1
    fi
    echo "Successfully compiled vaultx with NONCE_SIZE=$nonce_size"
}

# Function to calculate memory needed for in-memory generation
calculate_memory() {
    local k=$1
    
    # Use specific values for K33-K34, power-of-2 scaling for K25-K32
    case $k in
        33) echo 40960 ;;   # 40GB
        34) echo 81920 ;;   # 80GB
        *)
            local base_memory=128
            local memory_mb=$((base_memory << (k - 25)))  # Multiply by 2 for each K increment
            echo $memory_mb
            ;;
    esac
}

# Function to extract match percentage from CSV for a given K value
get_match_percentage() {
    local k=$1
    local match_percentage=$(awk -F',' -v k="$k" '
        NR > 1 && $2 == k { 
            gsub(/%/, "", $17); 
            print $17; 
            exit 
        }' "$PROJECT_DIR/$INPUT_CSV")
    
    if [ -z "$match_percentage" ]; then
        echo "Error: Could not find match percentage for K=$k in $INPUT_CSV"
        return 1
    fi
    
    echo "$match_percentage"
}

# Function to run vaultx for a specific K value with custom match factor
run_vaultx() {
    local k=$1
    local memory_mb=$2
    local nonce_size=$3
    local match_percentage=$4
    
    # Convert percentage to decimal (22.28 -> 0.2228)
    local match_factor=$(echo "scale=4; $match_percentage / 100" | bc)
    
    echo "Running vaultx for K=$k with memory=${memory_mb}MB, match_factor=${match_factor} (${match_percentage}%) (NONCE_SIZE=$nonce_size)..."
    
    # Remove temporary files if they exist
    rm -f "$PROJECT_DIR"/plots/*.tmp "$PROJECT_DIR"/plots/*.plot

    ./scripts/drop-all-caches.sh
    
    # Run vaultx with benchmark mode and custom match factor
    cd "$PROJECT_DIR"
    ./vaultx \
        -K $k \
        -m $memory_mb \
        -M $match_factor \
        -f "./plots/" \
        -j "./plots/" \
        -g "./plots/" \
        -t 128 \
        -x true \
        -a for \
        -W 16384 | sed "s/$/,$match_factor/" >> "$OUTPUT_FILE"
    
    local exit_code=${PIPESTATUS[0]}
    
    # Clean up temporary files
    rm -f "$PROJECT_DIR"/plots/*.tmp "$PROJECT_DIR"/plots/*.plot

    if [ $exit_code -ne 0 ]; then
        echo "Error: vaultx failed for K=$k with exit code $exit_code"
        return 1
    fi
    
    echo "Completed K=$k successfully with match factor $match_factor (${match_percentage}%)"
    return 0
}

# Main execution loop
current_nonce_size=0

for k in {25..34}; do
    echo "----------------------------------------"
    echo "Processing K=$k"
    
    # Get match percentage from CSV
    match_percentage=$(get_match_percentage $k)
    if [ $? -ne 0 ]; then
        echo "Skipping K=$k due to missing data"
        continue
    fi
    
    echo "Found match percentage for K=$k: $match_percentage%"
    
    # Determine required NONCE_SIZE
    if [ $k -le 32 ]; then
        required_nonce_size=4
    else
        required_nonce_size=5
    fi
    
    # Recompile if NONCE_SIZE changed
    if [ $current_nonce_size -ne $required_nonce_size ]; then
        compile_vaultx $required_nonce_size
        current_nonce_size=$required_nonce_size
    fi
    
    # Calculate memory requirement
    memory_mb=$(calculate_memory $k)
    
    echo "K=$k requires ${memory_mb}MB memory (NONCE_SIZE=$required_nonce_size)"
    
    # Check if memory requirement is reasonable
    if [ $memory_mb -gt 81920 ]; then
        echo "Warning: K=$k requires ${memory_mb}MB (>80GB), this may cause issues"
    fi
    
    # Run the experiment with the match percentage as the -M argument
    if ! run_vaultx $k $memory_mb $required_nonce_size $match_percentage; then
        echo "Failed to complete K=$k, continuing with next value..."
        continue
    fi
    
    # Brief pause between runs
    sleep 2
done

echo "----------------------------------------"
echo "Variable match factor experiment completed!"
echo "Results saved to: $OUTPUT_FILE"
echo ""
echo "Summary of match factors used:"
for k in {25..34}; do
    match_percentage=$(get_match_percentage $k 2>/dev/null || echo "N/A")
    memory_mb=$(calculate_memory $k)
    if [ $k -le 32 ]; then
        nonce_size=4
    else
        nonce_size=5
    fi
    echo "K$k: ${memory_mb}MB (NONCE_SIZE=$nonce_size) - Match Factor: ${match_percentage}%"
done

# Show first few lines of results
echo ""
echo "First few lines of results:"
head -n 5 "$OUTPUT_FILE"

# Show file size
file_size=$(du -h "$OUTPUT_FILE" | cut -f1)
echo ""
echo "Results file size: $file_size"
