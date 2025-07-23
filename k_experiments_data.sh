#!/bin/bash

LOG_ROOT=$1
OUTPUT_CSV=$2

if [[ -z "$LOG_ROOT" || -z "$OUTPUT_CSV" ]]; then
  echo "Usage: $0 <logs_folder> <output_csv>"
  exit 1
fi

echo "Source,Destination,K,GenTime(s),MergeTotalTime(s),ReadTime(s),WriteTime(s),MergeTime(s)" > "$OUTPUT_CSV"

for folder in "$LOG_ROOT"/*_to_*; do
  [[ -d "$folder" ]] || continue

  base=$(basename "$folder")
  SOURCE=$(echo "$base" | cut -d'_' -f1)
  DEST=$(echo "$base" | cut -d'_' -f3)

  for genfile in "$folder"/gen_K*.log; do
    [[ -f "$genfile" ]] || continue
    K=$(basename "$genfile" | sed -E 's/gen_K([0-9]+)\.log/\1/')

    GENTIME=$(grep -oP '\[\K[0-9]+(\.[0-9]+)?(?=s\] Completed generating)' "$genfile")

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

    # Round times to nearest integer if not empty
    if [[ -n "$GENTIME" ]]; then
      GENTIME=$(printf "%.0f" "$GENTIME")
    fi
    if [[ -n "$MERGETIME_TOTAL" ]]; then
      MERGETIME_TOTAL=$(printf "%.0f" "$MERGETIME_TOTAL")
    fi
    if [[ -n "$READTIME" ]]; then
      READTIME=$(printf "%.0f" "$READTIME")
    fi
    if [[ -n "$WRITETIME" ]]; then
      WRITETIME=$(printf "%.0f" "$WRITETIME")
    fi
    if [[ -n "$MERGETIME" ]]; then
      MERGETIME=$(printf "%.0f" "$MERGETIME")
    fi

    echo "$SOURCE,$DEST,$K,$GENTIME,$MERGETIME_TOTAL,$READTIME,$WRITETIME,$MERGETIME" >> "$OUTPUT_CSV"
  done
done

echo "Extraction complete. Output saved to $OUTPUT_CSV"
