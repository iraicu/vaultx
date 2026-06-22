#!/usr/bin/env bash
# merge_benchmark.sh
#
# Benchmark the vaultx merge-only workflow (-P merge) across combinations of:
#   -B (batch memory in MB)  and  -n (number of subplots to merge)
#
# Three experiment modes (determined automatically by array lengths):
#   vary_B  : multiple B_VALUES, one N_VALUES entry  → varying batch size
#   vary_N  : one B_VALUES entry,  multiple N_VALUES → varying subplot count
#   vary_BN : multiple in both arrays               → full B × N grid
#
# Source subplots are NEVER deleted.
# The merged output in DEST_DIR is copied to CEPH_DIR (if not already there),
# then deleted from DEST_DIR to free space before the next run.
#
# CSV columns:
#   B_mb, N, K, A, F, T, compute_threads_t, read_batch_R,
#   file_size_mb, per_file_batch_mb, total_data_gb,
#   total_batches, buckets_per_batch, expected_peak_ram_mb,
#   read_time_s, write_time_s, compute_time_s,
#   total_merge_time_s, total_merge_time_min,
#   avg_throughput_mbs, avg_batch_throughput_mbs,
#   peak_memory_mb, wall_time_s, wall_time_min
#
# Usage: ./merge_benchmark.sh
set -euo pipefail

# Raise the file-descriptor limit — merge opens all N subplots simultaneously.
ulimit -n 1048576 2>/dev/null || ulimit -n 65536 2>/dev/null || true


SOURCE_DIR=/nfs_nvme/sfatunmbi/subplots

DEST_DIR=/data-l/sfatunmbi/merged

CEPH_DIR=/ceph/sfatunmbi/mergedplots

CSV_DIR=/home/sfatunmbi/vaultx/experiments/merge_benchmark

K=32
B_VALUES=(256 512 1024 2048)
N_VALUES=(8 16 32)

COMPUTE_THREADS=1
READ_BATCH_SIZE=1024
MERGE_APPROACH=pipelined

SUDO_PASS=sfatunmbi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/vaultx"

[[ -x "$BIN" ]] || { echo "Error: vaultx not found at $BIN" >&2; exit 1; }
[[ -d "$SOURCE_DIR" ]] || { echo "Error: SOURCE_DIR '$SOURCE_DIR' does not exist." >&2; exit 1; }
[[ ${#B_VALUES[@]} -ge 1 ]] || { echo "Error: B_VALUES must have at least one entry." >&2; exit 1; }
[[ ${#N_VALUES[@]} -ge 1 ]] || { echo "Error: N_VALUES must have at least one entry." >&2; exit 1; }


drop_caches() {
  echo "$SUDO_PASS" | sudo -S sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null \
    || { echo "  Warning: sudo cache drop failed, falling back to sync." >&2; sync; }
}

safe_mkdir() {
  mkdir -p "$@" 2>/dev/null || echo "$SUDO_PASS" | sudo -S mkdir -p "$@"
}

safe_rm() {
  rm -f "$@" 2>/dev/null || echo "$SUDO_PASS" | sudo -S rm -f "$@" 2>/dev/null || true
}

safe_cp() {
  cp "$@" 2>/dev/null || echo "$SUDO_PASS" | sudo -S cp "$@"
}

n_b=${#B_VALUES[@]}
n_n=${#N_VALUES[@]}

if (( n_b > 1 && n_n == 1 )); then
  exp_mode="vary_B"
  csv_label="varyB_N${N_VALUES[0]}_K${K}"
elif (( n_b == 1 && n_n > 1 )); then
  exp_mode="vary_N"
  csv_label="B${B_VALUES[0]}_varyN_K${K}"
else
  exp_mode="vary_BN"
  csv_label="varyB_varyN_K${K}"
fi


safe_mkdir "$DEST_DIR" "$CEPH_DIR" "$CSV_DIR"

CSV="${CSV_DIR}/merge_benchmark_${csv_label}.csv"

printf '%s\n' \
  "B_mb,N,K,A,F,T,compute_threads_t,read_batch_R,file_size_mb,per_file_batch_mb,total_data_gb,total_batches,buckets_per_batch,expected_peak_ram_mb,read_time_s,write_time_s,compute_time_s,total_merge_time_s,total_merge_time_min,avg_throughput_mbs,avg_batch_throughput_mbs,peak_memory_mb,wall_time_s,wall_time_min" \
  > "$CSV"

echo ""
echo "=== merge_benchmark configuration ==="
echo "  Experiment mode  : $exp_mode"
echo "  K                : $K"
echo "  B values (MB)    : ${B_VALUES[*]}"
echo "  N values         : ${N_VALUES[*]}"
echo "  Source dir       : $SOURCE_DIR"
echo "  Dest dir         : $DEST_DIR"
echo "  Ceph dir         : $CEPH_DIR"
echo "  Compute threads  : $COMPUTE_THREADS (-t)"
echo "  Read batch size  : $READ_BATCH_SIZE (-R)"
echo "  Merge approach   : $MERGE_APPROACH (-A)"
echo "  CSV              : $CSV"
echo ""


for n in "${N_VALUES[@]}"; do
  for b in "${B_VALUES[@]}"; do

    echo "============================================================"
    echo " N=$n  B=${b} MB  K=$K"
    echo "============================================================"

    # ── Pre-run: verify enough subplots exist ─────────────────────────────

    subplot_count=$(find "$SOURCE_DIR" -maxdepth 1 -name "k${K}-*.plot" 2>/dev/null | wc -l)
    if (( subplot_count < n )); then
      echo "  Error: need $n k${K} subplots in $SOURCE_DIR but only found $subplot_count — skipping." >&2
      continue
    fi


    while IFS= read -r stale; do
      [[ -z "$stale" ]] && continue
      echo "  Removing stale: $stale"
      safe_rm "$stale"
    done < <(find "$DEST_DIR" -maxdepth 1 -name "merge_${K}_${n}*.plot" 2>/dev/null)


    log=$(mktemp --suffix=".merge_bench.log")

    echo "  CMD: $BIN -P merge -k $K -n $n -F $SOURCE_DIR -T $DEST_DIR \\"
    echo "             -B $b -t $COMPUTE_THREADS -R $READ_BATCH_SIZE -A $MERGE_APPROACH"
    echo ""

    wall_start_ms=$(date +%s%3N)

    set +e
    "$BIN" -P merge \
      -k "$K" -n "$n" \
      -F "$SOURCE_DIR" -T "$DEST_DIR" \
      -B "$b" -t "$COMPUTE_THREADS" \
      -R "$READ_BATCH_SIZE" -A "$MERGE_APPROACH" \
      2>&1 | tee "$log"
    vaultx_exit=${PIPESTATUS[0]}
    set -e

    wall_end_ms=$(date +%s%3N)
    wall_time_s=$(awk "BEGIN {printf \"%.3f\", (${wall_end_ms} - ${wall_start_ms}) / 1000.0}")
    wall_time_min=$(awk "BEGIN {printf \"%.6f\", ${wall_time_s} / 60.0}")

    if [[ $vaultx_exit -ne 0 ]]; then
      echo "  Error: vaultx exited with code $vaultx_exit — skipping CSV row." >&2
      rm -f "$log"
      drop_caches
      continue
    fi

    # ── Parse summary block ───────────────────────────────────────────────
    # Printed by merge.c at the very end of a successful merge:
    #   Read Time: X.XXs
    #   Write Time: X.XXs
    #   Merge Time: X.XXs          ← this is total process wall time in merge.c
    #   Avg Throughput: X.XX MB/s
    #   Peak Memory Usage: XXX MB

    read_time_s=$(grep "^Read Time:" "$log" 2>/dev/null | head -1 \
      | awk '{v=$3; gsub(/s$/,"",v); printf "%.4f", v+0}')

    write_time_s=$(grep "^Write Time:" "$log" 2>/dev/null | head -1 \
      | awk '{v=$3; gsub(/s$/,"",v); printf "%.4f", v+0}')

    total_merge_time_s=$(grep "^Merge Time:" "$log" 2>/dev/null | head -1 \
      | awk '{v=$3; gsub(/s$/,"",v); printf "%.4f", v+0}')

    avg_throughput_mbs=$(grep "^Avg Throughput:" "$log" 2>/dev/null | head -1 \
      | awk '{printf "%.4f", $3+0}')

    peak_memory_mb=$(grep "^Peak Memory Usage:" "$log" 2>/dev/null | head -1 \
      | awk '{printf "%.2f", $4+0}')

    # ── Parse config block ────────────────────────────────────────────────
    # Printed by merge.c before the merge starts:
    #   File size         : X.XX MB (X.XX GB)
    #   Total data        : X.XX GB
    #   Total batches     : N
    #   Buckets / batch   : N
    #   Per-file / batch  : X.XX MB
    #   Expected peak RAM : X MB (X.XX GB)  [2 x B]

    file_size_mb=$(grep "^File size" "$log" 2>/dev/null | head -1 \
      | awk '{printf "%.4f", $4+0}')

    total_data_gb=$(grep "^Total data" "$log" 2>/dev/null | head -1 \
      | awk '{printf "%.4f", $4+0}')

    total_batches=$(grep "^Total batches" "$log" 2>/dev/null | head -1 \
      | awk '{printf "%d", $NF+0}')

    buckets_per_batch=$(grep "^Buckets / batch" "$log" 2>/dev/null | head -1 \
      | awk '{printf "%d", $NF+0}')

    per_file_batch_mb=$(grep "^Per-file / batch" "$log" 2>/dev/null | head -1 \
      | awk '{printf "%.4f", $5+0}')

    expected_peak_ram_mb=$(grep "^Expected peak RAM" "$log" 2>/dev/null | head -1 \
      | awk '{printf "%.2f", $5+0}')

    # ── Average per-batch throughput ──────────────────────────────────────
    # Each batch line (pipelined approach) looks like:
    #   [XX.XX%] | Read: Xs | Write: Xs | Batch Time: Xs | Total Time: Xs | ETA: Xs | Throughput: XX.XX MB/s

    avg_batch_throughput_mbs=$(grep "Throughput:" "$log" 2>/dev/null \
      | grep -v "^Avg Throughput:" \
      | sed 's/.*Throughput: \([0-9.]*\) MB.*/\1/' \
      | awk 'BEGIN{sum=0;count=0} /^[0-9]/{sum+=$1; count++} END{if(count>0) printf "%.4f",sum/count; else print "NA"}')

    # ── Defaults for any missing fields ──────────────────────────────────

    read_time_s="${read_time_s:-NA}"
    write_time_s="${write_time_s:-NA}"
    total_merge_time_s="${total_merge_time_s:-NA}"
    avg_throughput_mbs="${avg_throughput_mbs:-NA}"
    peak_memory_mb="${peak_memory_mb:-NA}"
    file_size_mb="${file_size_mb:-NA}"
    total_data_gb="${total_data_gb:-NA}"
    total_batches="${total_batches:-NA}"
    buckets_per_batch="${buckets_per_batch:-NA}"
    per_file_batch_mb="${per_file_batch_mb:-NA}"
    expected_peak_ram_mb="${expected_peak_ram_mb:-NA}"
    avg_batch_throughput_mbs="${avg_batch_throughput_mbs:-NA}"

    # ── Derived fields ────────────────────────────────────────────────────

    if [[ "$total_merge_time_s" != "NA" ]]; then
      total_merge_time_min=$(awk "BEGIN {printf \"%.6f\", ${total_merge_time_s}/60.0}")
    else
      total_merge_time_min="NA"
    fi

    if [[ "$total_merge_time_s" != "NA" && "$read_time_s" != "NA" && "$write_time_s" != "NA" ]]; then
      compute_time_s=$(awk "BEGIN {
        ct = ${total_merge_time_s} - ${read_time_s} - ${write_time_s}
        if (ct < 0) ct = 0
        printf \"%.4f\", ct
      }")
    else
      compute_time_s="NA"
    fi

    rm -f "$log"

    # ── Handle merged output → Ceph ───────────────────────────────────────
    # Copy to Ceph if not already there, then always delete the local copy.
    # Source subplots in SOURCE_DIR are never touched.

    merged_file=""
    if [[ -f "$DEST_DIR/merge_${K}_${n}.plot" ]]; then
      merged_file="$DEST_DIR/merge_${K}_${n}.plot"
    else
      merged_file=$(find "$DEST_DIR" -maxdepth 1 -name "merge_${K}_${n}_*.plot" \
        -printf "%T@ %p\n" 2>/dev/null | sort -n | tail -1 | awk '{print $2}') || true
    fi

    if [[ -n "$merged_file" && -f "$merged_file" ]]; then
      merged_name=$(basename "$merged_file")
      if [[ -f "$CEPH_DIR/$merged_name" ]]; then
        echo "  '$merged_name' already in Ceph — deleting local copy."
      else
        echo "  Copying '$merged_name' → $CEPH_DIR/ ..."
        safe_cp "$merged_file" "$CEPH_DIR/"
        echo "  Copy done."
      fi
      safe_rm "$merged_file"
    else
      echo "  Warning: merged output not found in $DEST_DIR" >&2
    fi

    # ── Drop system page cache ────────────────────────────────────────────

    echo "  Dropping caches ..."
    drop_caches

    # ── Append row to CSV ─────────────────────────────────────────────────

    printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
      "$b" "$n" "$K" \
      "$MERGE_APPROACH" "$SOURCE_DIR" "$DEST_DIR" \
      "$COMPUTE_THREADS" "$READ_BATCH_SIZE" \
      "$file_size_mb" "$per_file_batch_mb" "$total_data_gb" \
      "$total_batches" "$buckets_per_batch" "$expected_peak_ram_mb" \
      "$read_time_s" "$write_time_s" "$compute_time_s" \
      "$total_merge_time_s" "$total_merge_time_min" \
      "$avg_throughput_mbs" "$avg_batch_throughput_mbs" \
      "$peak_memory_mb" "$wall_time_s" "$wall_time_min" \
      >> "$CSV"

    echo "  Row written → $CSV"
    echo ""
  done
done

echo ""
echo "All experiments complete."
echo "Results: $CSV"
