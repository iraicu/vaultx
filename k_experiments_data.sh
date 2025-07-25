#!/bin/bash

LOG_ROOT=$1
OUTPUT_CSV=$2

if [[ -z "$LOG_ROOT" || -z "$OUTPUT_CSV" ]]; then
  echo "Usage: $0 <logs_folder> <output_csv>"
  exit 1
fi

echo "Source,Destination,K,GenTime(s),GenThroughput(MH/s),IOThroughput(MB/s),MergeTotalTime(s),ReadTime(s),WriteTime(s),MergeTime(s),SmallSearchTime(s),MergedSearchTime(s)" > "$OUTPUT_CSV"

for folder in "$LOG_ROOT"/*_to_*; do
  [[ -d "$folder" ]] || continue

  base=$(basename "$folder")
  SOURCE=$(echo "$base" | cut -d'_' -f1)
  DEST=$(echo "$base" | cut -d'_' -f3)

  for genfile in "$folder"/gen_K*.log; do
    [[ -f "$genfile" ]] || continue
    K=$(basename "$genfile" | sed -E 's/gen_K([0-9]+)\.log/\1/')

    GENTIME=$(grep -oP '\[\K[0-9]+(\.[0-9]+)?(?=s\] Completed generating)' "$genfile")
    GENTHROUGHPUT=$(grep -oP '(?<=Hashgen Throughput: )[0-9]+(\.[0-9]+)?(?= MH/s)' "$genfile")
    IOTHRUPUT=$(grep -oP '(?<=IO Throughput: )[0-9]+(\.[0-9]+)?(?= MB/s)' "$genfile")

    MERGETIME_TOTAL=""
    READTIME=""
    WRITETIME=""
    MERGETIME=""

    mergefile="$folder/merge_K${K}.log"
    if [[ -f "$mergefile" ]]; then
      MERGETIME_TOTAL=$(grep -oP '(?<=Merge complete: )[0-9]+(\.[0-9]+)?(?=s)' "$mergefile")
      READTIME=$(grep -oP '(?<=Read Time: )[0-9]+(\.[0-9]+)?(?=s)' "$mergefile")
      WRITETIME=$(grep -oP '(?<=Write Time: )[0-9]+(\.[0-9]+)?(?=s)' "$mergefile")
      MERGETIME=$(grep -oP '(?<=Merge Time: )[0-9]+(\.[0-9]+)?(?=s)' "$mergefile")
    fi

    SMALLSEARCH="0"
    MERGEDSEARCH="0"
    if [[ "$SOURCE" == "$DEST" ]]; then
      searchfile="$folder/search_k${K}.log"
      if [[ -f "$searchfile" ]]; then
        SMALLSEARCH=$(grep "Small Plots Lookup Time:" "$searchfile" | sed -E 's/.*Small Plots Lookup Time: ([0-9.]+).*/\1/')
        MERGEDSEARCH=$(grep "Merged Plot Lookup Time:" "$searchfile" | sed -E 's/.*Merged Plot Lookup Time: ([0-9.]+).*/\1/')
      fi
    fi

    # Round times except search and throughput (which keep 2 decimal places)
    format_round() {
      local val=$1
      if [[ -z "$val" ]]; then
        echo "0"
      else
        printf "%.0f" "$val"
      fi
    }

    format_two_dec() {
      local val=$1
      if [[ -z "$val" ]]; then
        echo "0.00"
      else
        printf "%.2f" "$val"
      fi
    }

    GENTIME=$(format_round "$GENTIME")
    MERGETIME_TOTAL=$(format_round "$MERGETIME_TOTAL")
    READTIME=$(format_round "$READTIME")
    WRITETIME=$(format_round "$WRITETIME")
    MERGETIME=$(format_round "$MERGETIME")

    GENTHROUGHPUT=$(format_two_dec "$GENTHROUGHPUT")
    IOTHRUPUT=$(format_two_dec "$IOTHRUPUT")
    SMALLSEARCH=$(format_two_dec "$SMALLSEARCH")
    MERGEDSEARCH=$(format_two_dec "$MERGEDSEARCH")

    echo "$SOURCE,$DEST,$K,$GENTIME,$GENTHROUGHPUT,$IOTHRUPUT,$MERGETIME_TOTAL,$READTIME,$WRITETIME,$MERGETIME,$SMALLSEARCH,$MERGEDSEARCH" >> "$OUTPUT_CSV"
  done
done

echo "Extraction complete. Output saved to $OUTPUT_CSV"
