#!/usr/bin/env bash
set -euo pipefail

############################################
# CLI parsing
############################################

PROFILE_CSV=""
EXPERIMENTS_CSV=""
DRYRUN=false
OUTPUT_FILE=""
DRYRUN_COUNT=0

usage() {
  cat <<EOF
Usage:
  $0 --profile profiles.csv --experiments experiments.csv [--dryrun] [--output FILE]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile) PROFILE_CSV="${2:-}"; shift 2 ;;
    --experiments) EXPERIMENTS_CSV="${2:-}"; shift 2 ;;
    --dryrun) DRYRUN=true; shift ;;
    --output) OUTPUT_FILE="${2:-}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown argument: $1"; usage; exit 2 ;;
  esac
done

[[ -z "$PROFILE_CSV" ]] && { echo "ERROR: --profile is required"; exit 2; }
[[ -z "$EXPERIMENTS_CSV" ]] && { echo "ERROR: --experiments is required"; exit 2; }
[[ ! -f "$PROFILE_CSV" ]] && { echo "ERROR: not found: $PROFILE_CSV"; exit 2; }
[[ ! -f "$EXPERIMENTS_CSV" ]] && { echo "ERROR: not found: $EXPERIMENTS_CSV"; exit 2; }

############################################
# Helpers
############################################

emit() {
  echo "$1"
  if [[ -n "$OUTPUT_FILE" ]]; then
    echo "$1" >> "$OUTPUT_FILE"
  fi
}

# Track if search header has been written for this output file
SEARCH_HEADER_WRITTEN=false

EXPECTED_EXPERIMENT_COLS=18

trim() {
  local s="$1"
  s="${s#"${s%%[![:space:]]*}"}"
  s="${s%"${s##*[![:space:]]}"}"
  printf '%s' "$s"
}

strip_quotes() {
  local s
  s="$(trim "$1")"
  [[ "$s" == \"*\" && "$s" == *\" ]] && s="${s:1:${#s}-2}"
  printf '%s' "$s"
}

exp_context() {
  echo "READ: hostname=${hostname},exp_name=${exp_name},enabled=${enabled},exp_type=${exp_type},storage_targets=${storage_targets},temp_target=${temp_target},n=${n_val},P=${p_flag},A_list=${a_list},B_list=${b_list},k_list=${k_list},t_list=${t_list},i_list=${i_list},s_string=${s_string},S_count=${S_count},D_difficulty=${D_difficulty},r_list=${r_list},O_list=${O_list}"
}

exp_error() {
  local msg="$1"
  echo "ERROR: ${msg}"
  exp_context
  exit 1
}

############################################
# Load host profile
############################################

HOST="$(hostname -s)"
PROFILE_FOUND=false

while IFS=, read -r hostname system_name cores memory_gb vaultx_path NVME SSD HDD NFS_NVME NFS_HDD CEPH_HDD || [[ -n "${hostname:-}" ]]; do
  [[ "$hostname" == "hostname" ]] && continue
  hostname="$(strip_quotes "$hostname")"

  if [[ "$hostname" == "$HOST" ]]; then
    PROFILE_FOUND=true
    vaultx_path="$(strip_quotes "$vaultx_path")"
    NVME="$(strip_quotes "$NVME")"
    SSD="$(strip_quotes "$SSD")"
    HDD="$(strip_quotes "$HDD")"
    NFS_NVME="$(strip_quotes "$NFS_NVME")"
    NFS_HDD="$(strip_quotes "$NFS_HDD")"
    CEPH_HDD="$(strip_quotes "$CEPH_HDD")"
    break
  fi
done < "$PROFILE_CSV"

$PROFILE_FOUND || { echo "ERROR: No profile for host '$HOST'"; exit 1; }
[[ -x "$vaultx_path" ]] || { echo "ERROR: vaultx not executable: $vaultx_path"; exit 1; }

############################################
# Storage resolver
############################################

storage_path_for() {
  case "$1" in
    NVME) echo "$NVME" ;;
    SSD) echo "$SSD" ;;
    HDD) echo "$HDD" ;;
    NFS_NVME) echo "$NFS_NVME" ;;
    NFS_HDD) echo "$NFS_HDD" ;;
    CEPH_HDD) echo "$CEPH_HDD" ;;
    *) echo "" ;;
  esac
}

############################################
# Execute experiments
############################################

# CSV header:
# hostname,exp_name,enabled,exp_type,storage_targets,temp_target,n,P,A_list,B_list,k_list,t_list,i_list,s_string,S_count,D_difficulty,r_list,O_list
#
# For search experiments:
#   - s_string: single search string (for -s), mutually exclusive with S_count
#   - S_count: batch lookup count (for -S), mutually exclusive with s_string
#   - D_difficulty: difficulty/prefix length (for -D), required for search
#   - r_list: hash threads (for -r), can be a range like "1 2 4 8"
#   - O_list: keep files open (for -O), "true" or "false" or "true false"

while IFS= read -r line || [[ -n "${line:-}" ]]; do
  line="$(trim "$line")"
  [[ -z "$line" ]] && continue
  col_commas="${line//[^,]/}"
  col_count=$(( ${#col_commas} + 1 ))
  if [[ "$col_count" -ne "$EXPECTED_EXPERIMENT_COLS" ]]; then
    echo "ERROR: experiments.csv expected ${EXPECTED_EXPERIMENT_COLS} columns but read ${col_count}"
    echo "READ_LINE: ${line}"
    exit 1
  fi

  IFS=, read -r hostname exp_name enabled exp_type storage_targets temp_target n_val p_flag a_list b_list k_list t_list i_list s_string S_count D_difficulty r_list O_list <<< "$line"
  [[ "$hostname" == "hostname" ]] && continue
  hostname="$(strip_quotes "$hostname")"
  [[ "$hostname" != "$HOST" ]] && continue

  exp_name="$(strip_quotes "$exp_name")"
  enabled="$(strip_quotes "$enabled")"
  exp_type="$(strip_quotes "$exp_type")"
  storage_targets="$(strip_quotes "$storage_targets")"
  temp_target="$(strip_quotes "$temp_target")"
  n_val="$(strip_quotes "$n_val")"
  p_flag="$(strip_quotes "$p_flag")"
  a_list="$(strip_quotes "$a_list")"
  b_list="$(strip_quotes "$b_list")"
  k_list="$(strip_quotes "$k_list")"
  t_list="$(strip_quotes "$t_list")"
  i_list="$(strip_quotes "$i_list")"
  s_string="$(strip_quotes "$s_string")"
  S_count="$(strip_quotes "$S_count")"
  D_difficulty="$(strip_quotes "$D_difficulty")"
  r_list="$(strip_quotes "$r_list")"
  O_list="$(strip_quotes "$O_list")"

  [[ "$enabled" == "true" ]] || continue

  for tgt in $storage_targets; do
    tgt="$(strip_quotes "$tgt")"
    target_path="$(storage_path_for "$tgt")"
    [[ -n "$target_path" ]] || exp_error "Unknown storage '$tgt'"

    if [[ "$exp_type" == "merge" ]]; then
      [[ -n "$temp_target" ]] || exp_error "merge exp '$exp_name' missing temp_target"
      temp_path="$(storage_path_for "$temp_target")"
      [[ -n "$temp_path" ]] || exp_error "Unknown temp_target '$temp_target'"
      [[ "$n_val" =~ ^[0-9]+$ ]] || exp_error "merge exp '$exp_name' invalid n"
      if [[ -n "$a_list" ]]; then
        for a in $a_list; do
          [[ "$a" == "pipelined" || "$a" == "serial" ]] || exp_error "merge exp '$exp_name' invalid A value '$a' (must be 'pipelined' or 'serial')"
        done
      fi
      if [[ -n "$b_list" ]]; then
        for b in $b_list; do
          [[ "$b" =~ ^[0-9]+$ ]] || exp_error "merge exp '$exp_name' invalid B value '$b' (must be a number)"
        done
      fi
      if [[ -n "$p_flag" ]]; then
        [[ "$p_flag" == "true" || "$p_flag" == "both" || "$p_flag" == "gen" || "$p_flag" == "merge" ]] || exp_error "merge exp '$exp_name' invalid P value '$p_flag' (must be 'true', 'both', 'gen', or 'merge')"
      fi
    fi

    ############################################
    # Validate search experiments
    ############################################
    if [[ "$exp_type" == "search" ]]; then
      if [[ -n "$s_string" && -n "$S_count" ]]; then
        exp_error "search exp '$exp_name' cannot have both s_string and S_count"
      fi
      if [[ -z "$s_string" && -z "$S_count" ]]; then
        exp_error "search exp '$exp_name' must have either s_string (-s) or S_count (-S)"
      fi
      if [[ -z "$D_difficulty" ]]; then
        exp_error "search exp '$exp_name' missing D_difficulty (-D)"
      fi
      [[ "$D_difficulty" =~ ^[0-9]+$ ]] || exp_error "search exp '$exp_name' invalid D_difficulty '$D_difficulty' (must be a number)"
   
      if [[ -n "$S_count" ]]; then
        [[ "$S_count" =~ ^[0-9]+$ ]] || exp_error "search exp '$exp_name' invalid S_count '$S_count' (must be a number)"
      fi
    
      if [[ -n "$r_list" ]]; then
        for r in $r_list; do
          [[ "$r" =~ ^[0-9]+$ ]] || exp_error "search exp '$exp_name' invalid r_list value '$r' (must be a number)"
        done
      fi

      if [[ -n "$O_list" ]]; then
        for o in $O_list; do
          [[ "$o" == "true" || "$o" == "false" ]] || exp_error "search exp '$exp_name' invalid O_list value '$o' (must be 'true' or 'false')"
        done
      fi
    fi

    ############################################
    # Execute search experiments
    ############################################
    if [[ "$exp_type" == "search" ]]; then

      if [[ -n "$r_list" ]]; then
        r_values=($r_list)
      else
        r_values=("")
      fi
      if [[ -n "$O_list" ]]; then
        O_values=($O_list)
      else
        O_values=("false")
      fi

      for t in $t_list; do
        for r in "${r_values[@]}"; do
          for o in "${O_values[@]}"; do
            for i in $i_list; do

              
              CMD=( "$vaultx_path" -t "$t" -b true -f "$target_path" -D "$D_difficulty" )

              # Determine search type: single (-s) or batch (-S)
              if [[ -n "$s_string" ]]; then
                search_mode="single"
                CMD+=( -s "$s_string" )
              else
                search_mode="batch"
                CMD+=( -S "$S_count" )
              fi

              
              [[ -n "$r" ]] && CMD+=( -r "$r" )

              # Add -O (keep files open)
              [[ "$o" == "true" ]] && CMD+=( -O true )

              # Build prefix for output
              r_val="${r:-$t}"
              PREFIX="host=${HOST},pid=$$,type=${exp_type},exp=${exp_name},storage=${tgt},search_mode=${search_mode},t=${t},r=${r_val},O=${o},D=${D_difficulty},i=${i}"

              if $DRYRUN; then
                DRYRUN_COUNT=$((DRYRUN_COUNT + 1))
                emit "[DRYRUN ${DRYRUN_COUNT}] ${CMD[*]}"
              else
                # Run search and capture output from search-b.csv
                # First, remove any existing search-b.csv to get fresh results
                rm -f ./search-b.csv

                # Run the search command (suppresses stdout since -b true writes to CSV)
                "${CMD[@]}" > /dev/null 2>&1

                # Read and emit the search results with headers
                if [[ -f ./search-b.csv ]]; then
                  # Emit header on first search result for this output file
                  if [[ "$SEARCH_HEADER_WRITTEN" == "false" ]]; then
                    # Read the header line and add prefix columns
                    header_line=$(head -n 1 ./search-b.csv)
                    emit "host,pid,exp_type,exp_name,storage,search_mode,io_threads,hash_threads,keep_open,difficulty,iteration,${header_line}"
                    SEARCH_HEADER_WRITTEN=true
                  fi
                  # Emit data line (skip header, should be only 1 data line)
                  data_line=$(tail -n +2 ./search-b.csv | head -n 1)
                  if [[ -n "$data_line" ]]; then
                    emit "${HOST},$$,${exp_type},${exp_name},${tgt},${search_mode},${t},${r_val},${o},${D_difficulty},${i},${data_line}"
                  fi
                  rm -f ./search-b.csv
                else
                  echo "WARNING: search-b.csv not found after search command"
                fi
              fi

            done
          done
        done
      done

    ############################################
    # Execute single/merge experiments
    ############################################
    else
      for k in $k_list; do
        for t in $t_list; do
          for i in $i_list; do
            if [[ "$exp_type" == "merge" ]]; then
              if [[ -n "$a_list" ]]; then
                a_values=($a_list)
              else
                a_values=("")
              fi
              if [[ -n "$b_list" ]]; then
                b_values=($b_list)
              else
                b_values=("")
              fi
            else
              a_values=("")
              b_values=("")
            fi

            for a in "${a_values[@]}"; do
              for b in "${b_values[@]}"; do

                CMD=( "$vaultx_path" -k "$k" -t "$t" -i "$i" -b true )

                if [[ "$exp_type" == "single" ]]; then
                  CMD+=( -f "$target_path" )
                elif [[ "$exp_type" == "merge" ]]; then
                  CMD+=( -T "$target_path" -F "$temp_path" -n "$n_val" )
                  if [[ "$p_flag" == "true" || "$p_flag" == "both" ]]; then
                    CMD+=( -P )
                  elif [[ "$p_flag" == "gen" || "$p_flag" == "merge" ]]; then
                    CMD+=( -P "$p_flag" )
                  fi
                  [[ -n "$a" ]] && CMD+=( -A "$a" )
                  [[ -n "$b" ]] && CMD+=( -B "$b" )
                else
                  exp_error "Unknown exp_type '$exp_type'"
                fi

                if [[ "$exp_type" == "merge" ]]; then
                  PREFIX="host=${HOST},pid=$$,type=${exp_type},exp=${exp_name},storage=${tgt},temp=${temp_target},n=${n_val},k=${k},t=${t},i=${i},A=${a},B=${b}"
                else
                  PREFIX="host=${HOST},pid=$$,type=${exp_type},exp=${exp_name},storage=${tgt},k=${k},t=${t},i=${i},A=,B="
                fi

                if $DRYRUN; then
                  DRYRUN_COUNT=$((DRYRUN_COUNT + 1))
                  emit "[DRYRUN ${DRYRUN_COUNT}] ${CMD[*]}"
                else
                  "${CMD[@]}" | while IFS= read -r line; do
                    emit "${PREFIX},${line}"
                  done
                fi

              done
            done

          done
        done
      done
    fi
  done

done < "$EXPERIMENTS_CSV"
