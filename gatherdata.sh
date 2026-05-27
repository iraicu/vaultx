#!/usr/bin/env bash
# gatherdata.sh — Distributed data collection and experiment orchestration
# across lab machines, run from login1.
#
# Commands:
#   -run <scriptname>                          Start script in screen on all HOSTNAMES
#   -gather true                               Rsync results from all HOSTNAMES into ~/Results/
#   -check true                                Check screen/vaultx status on all HOSTNAMES
#   -setup                                     Init state + push per-machine .drives.local
#   -orchestrate                               Run scheduling loop (5-min ticks, runs forever)
#   -status                                    Show orchestrator job status table
#   -mark-done <script> <drive> <machine...>   Mark specific jobs as done

# ===========================================================================
# SECTION 1 — Basic config (used by -run / -gather / -check)
# ===========================================================================

# Hosts for ad-hoc commands (-run, -gather, -check).
HOSTNAMES=(athena s8 nvmebox gpubox rpi5 thunderx1 thunderx2 fpganode2 torus)

NEEDS_PASSWORD=(s8 rpi5)
SSH_PASSWORD="sfatunmbi"

REMOTE_USER="sfatunmbi"
REMOTE_BASE="/home/${REMOTE_USER}/vaultx"
REMOTE_SCRIPTS="${REMOTE_BASE}/scripts"
RESULTS_BASE="${HOME}/Results"
SCREEN_NAME="plotting"

# SSH alias → real hostname (as reported by hostname(1) on the machine).
declare -A REAL_HOSTNAME=(
    [s8]="eightsocket"
    [torus]="torusnode01"
)

# ===========================================================================
# SECTION 2 — Orchestrator config (used by -setup / -orchestrate / -status)
# ===========================================================================

# ---------------------------------------------------------------------------
# MACHINES UNDER ORCHESTRATOR CONTROL
# Add or remove hostnames here to include/exclude machines.
# Examples:
#   - comment out 'opi5' when it is offline
#   - uncomment 'epycbox' once its current screen job finishes
# ---------------------------------------------------------------------------
ORCH_MACHINES=(
    athena
    s8
    nvmebox
    gpubox
    rpi5
    thunderx1
    thunderx2
    fpganode2
    torus
    # epycbox   # busy with another screen job — add back when free
    # opi5      # offline — add back when reachable
)

# Experiments to run, in this order per machine.
ORCH_SCRIPTS=(
    run_k27_k32_benchmark.sh
    vaultxplot_varying_memory.sh
    vaultxplot_varying_threads.sh
)

# ---------------------------------------------------------------------------
# PER-MACHINE LOCAL DRIVES (semicolon-separated paths)
# These are pushed to .drives.local on each machine by -setup.
# Edit here and re-run -setup to update machines.
# ---------------------------------------------------------------------------
declare -A MACHINE_LOCAL_DRIVES
MACHINE_LOCAL_DRIVES[epycbox]="/data-k/sfatunmbi;/ssd-raid0/sfatunmbi;/sfatunmbi"
MACHINE_LOCAL_DRIVES[athena]="/HDD/sfatunmbi;/sfatunmbi;/NVME_RAID-0/sfatunmbi"
MACHINE_LOCAL_DRIVES[s8]="/data-i/sfatunmbi;/ssd-raid0/sfatunmbi;/nvme-raid0/sfatunmbi"
MACHINE_LOCAL_DRIVES[nvmebox]="/data-b/sfatunmbi;/data-fast/sfatunmbi;/sfatunmbi"
MACHINE_LOCAL_DRIVES[gpubox]="/data-fast/sfatunmbi"
MACHINE_LOCAL_DRIVES[rpi5]="/data-a/sfatunmbi;/data-fast/sfatunmbi"
MACHINE_LOCAL_DRIVES[thunderx1]="/sfatunmbi"
MACHINE_LOCAL_DRIVES[thunderx2]="/sfatunmbi"
MACHINE_LOCAL_DRIVES[fpganode2]="/ssd-raid0/sfatunmbi"
MACHINE_LOCAL_DRIVES[torus]="/data-c/sfatunmbi;/ssd-raid0/sfatunmbi"
MACHINE_LOCAL_DRIVES[opi5]=""   # no drives — leave empty

# ---------------------------------------------------------------------------
# SHARED NETWORK DRIVES (name → mount path)
# Only one machine may use each drive at a time (enforced via lock files).
# ---------------------------------------------------------------------------
declare -A NETWORK_DRIVE_PATHS
NETWORK_DRIVE_PATHS[nfs_hdd]="/nfs_hdd/sfatunmbi"
NETWORK_DRIVE_PATHS[nfs_nvme]="/nfs_nvme/sfatunmbi"
NETWORK_DRIVE_PATHS[ceph]="/ceph/sfatunmbi"

# Ordered iteration for network drives (associative arrays have no stable order).
NETWORK_DRIVE_KEYS=(nfs_hdd nfs_nvme ceph)

# Orchestrator state directory on login1.
STATE_DIR="${HOME}/vaultx_state"
JOBS_FILE="${STATE_DIR}/jobs.tsv"
LOCKS_DIR="${STATE_DIR}/locks"
RUNNING_DIR="${STATE_DIR}/running"

# ===========================================================================
# SECTION 3 — SSH / rsync helpers
# ===========================================================================

_needs_password() {
    local h
    for h in "${NEEDS_PASSWORD[@]}"; do [[ "$h" == "$1" ]] && return 0; done
    return 1
}

_ssh() {
    local host="$1"; shift
    if _needs_password "$host"; then
        sshpass -p "$SSH_PASSWORD" ssh \
            -o StrictHostKeyChecking=no \
            -o ConnectTimeout=10 \
            -T "$host" "$@"
    else
        ssh \
            -o StrictHostKeyChecking=no \
            -o ConnectTimeout=10 \
            -o BatchMode=yes \
            -T "$host" "$@"
    fi
}

_rsync() {
    local host="$1" src="$2" dst="$3"
    local ssh_e="ssh -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o ServerAliveInterval=30 -o ServerAliveCountMax=6"
    if _needs_password "$host"; then
        sshpass -p "$SSH_PASSWORD" rsync -az --stats -e "$ssh_e" "${host}:${src}" "$dst"
    else
        rsync -az --stats -e "$ssh_e" "${host}:${src}" "$dst"
    fi
}

pad() { printf "[%-12s] " "$1"; }

# ===========================================================================
# SECTION 4 — Orchestrator state management
# ===========================================================================

_get_job_status() {
    grep -m1 "^${1}"$'\t' "$JOBS_FILE" 2>/dev/null | cut -f2
}

_set_job_status() {
    local job_id="$1" status="$2"
    local tmp
    tmp=$(mktemp)
    grep -v "^${job_id}"$'\t' "$JOBS_FILE" 2>/dev/null > "$tmp" || true
    printf '%s\t%s\t%s\n' "$job_id" "$status" "$(date +%Y-%m-%dT%H:%M:%S)" >> "$tmp"
    mv "$tmp" "$JOBS_FILE"
}

_init_jobs() {
    : > "$JOBS_FILE"
    for machine in "${ORCH_MACHINES[@]}"; do
        for script in "${ORCH_SCRIPTS[@]}"; do
            printf '%s\tpending\t%s\n' "${machine}:${script}:local" "$(date +%Y-%m-%dT%H:%M:%S)" >> "$JOBS_FILE"
            for dk in "${NETWORK_DRIVE_KEYS[@]}"; do
                printf '%s\tpending\t%s\n' "${machine}:${script}:${dk}" "$(date +%Y-%m-%dT%H:%M:%S)" >> "$JOBS_FILE"
            done
        done
    done
}

_claim_lock() {
    local drive_key="$1" machine="$2"
    local holder
    holder=$(cat "${LOCKS_DIR}/${drive_key}" 2>/dev/null)
    if [[ -z "$holder" ]]; then
        echo "$machine" > "${LOCKS_DIR}/${drive_key}"
        return 0
    fi
    return 1
}

_release_lock() { : > "${LOCKS_DIR}/${1}"; }

_lock_holder() { cat "${LOCKS_DIR}/${1}" 2>/dev/null; }

# ===========================================================================
# SECTION 5 — Commands
# ===========================================================================

# ---------------------------------------------------------------------------
# -run <scriptname>  — start a script on all HOSTNAMES (ad-hoc, no drive override)
# ---------------------------------------------------------------------------
cmd_run() {
    local script_name="$1"
    [[ -z "$script_name" ]] && { echo "Usage: $(basename "$0") -run <scriptname>"; exit 1; }

    local script_path="${REMOTE_SCRIPTS}/${script_name}"
    echo "Launching '${script_name}' in screen '${SCREEN_NAME}' on ${#HOSTNAMES[@]} machines ..."
    echo

    for host in "${HOSTNAMES[@]}"; do
        pad "$host"
        result=$(_ssh "$host" "
screen -wipe >/dev/null 2>&1
if screen -list 2>/dev/null | grep -q '\\.${SCREEN_NAME}[[:space:]]'; then
    echo EXISTS
else
    STARTUP_LOG=\"/tmp/${SCREEN_NAME}_startup.log\"
    screen -dmS '${SCREEN_NAME}' bash -c \"bash '${script_path}' > \\\"\${STARTUP_LOG}\\\" 2>&1\" 2>/dev/null || { echo FAILED; exit 0; }
    sleep 5
    screen -wipe >/dev/null 2>&1
    if screen -list 2>/dev/null | grep -q '\\.${SCREEN_NAME}[[:space:]]'; then
        echo STARTED
    else
        echo DIED
        cat \"\${STARTUP_LOG}\" 2>/dev/null || echo '(no log output captured)'
    fi
fi" 2>&1)
        rc=$?

        if   [[ $rc -ne 0 ]];             then echo "FAIL   — cannot connect (SSH exit ${rc})"
        elif [[ "$result" == "EXISTS" ]];  then echo "SKIP   — screen '${SCREEN_NAME}' already running, left untouched"
        elif [[ "$result" == "STARTED" ]]; then echo "OK     — '${script_name}' started and still alive in screen '${SCREEN_NAME}'"
        elif [[ "$result" == DIED* ]];     then printf "FAIL   — script exited within 5s on %s. Output:\n%s\n" "$host" "${result#DIED$'\n'}"
        elif [[ "$result" == "FAILED" ]];  then echo "FAIL   — could not create screen session"
        else                                    echo "FAIL   — unexpected output: ${result}"
        fi
    done
}

# ---------------------------------------------------------------------------
# -gather true
# ---------------------------------------------------------------------------
cmd_gather() {
    mkdir -p "$RESULTS_BASE"
    echo "Gathering results into ${RESULTS_BASE}/ ..."
    echo

    for host in "${HOSTNAMES[@]}"; do
        pad "$host"
        local local_dest="${RESULTS_BASE}/${host}"
        mkdir -p "$local_dest"

        local real_host="${REAL_HOSTNAME[$host]:-$host}"

        pre=$(_ssh "$host" "
if ! command -v rsync >/dev/null 2>&1; then echo NO_RSYNC
elif [[ ! -d \"${REMOTE_BASE}/newexperiments/${real_host}\" ]]; then echo NO_DIR
else echo OK
fi" 2>&1)

        if   [[ "$pre" == NO_RSYNC ]]; then echo "FAIL   — rsync not installed on ${host}"; continue
        elif [[ "$pre" == NO_DIR ]];   then echo "SKIP   — results directory does not exist yet on ${host}"; continue
        fi

        output=$(_rsync "$host" "${REMOTE_BASE}/newexperiments/${real_host}/" "$local_dest" 2>&1)
        rc=$?
        if [[ $rc -eq 0 ]]; then
            transferred=$(echo "$output" | grep "Number of regular files transferred:" | awk '{print $NF}')
            echo "OK     — ${transferred:-0} file(s) synced → ${local_dest}"
        else
            echo "FAIL   — rsync exit ${rc}:"
            echo "$output" | grep -v "^$" | tail -5 | sed 's/^/             /'
        fi
    done
}

# ---------------------------------------------------------------------------
# -check true
# ---------------------------------------------------------------------------
cmd_check() {
    echo "Checking screen '${SCREEN_NAME}' on ${#HOSTNAMES[@]} machines ..."
    echo

    for host in "${HOSTNAMES[@]}"; do
        pad "$host"
        output=$(_ssh "$host" "
screen -wipe >/dev/null 2>&1
if ! screen -list 2>/dev/null | grep -q '\.${SCREEN_NAME}[[:space:]]'; then
    echo GONE
elif pgrep -x vaultx >/dev/null 2>&1; then
    echo RUNNING
else
    echo IDLE
fi" 2>&1)
        rc=$?

        if   [[ $rc -ne 0 ]];           then echo "FAIL    — cannot connect (SSH exit ${rc})"
        elif [[ "$output" == RUNNING ]]; then echo "RUNNING — vaultx is actively computing"
        elif [[ "$output" == IDLE ]];    then echo "IDLE    — screen alive but vaultx stopped (done or between k-values)"
        elif [[ "$output" == GONE ]];    then echo "DONE    — screen gone, experiment finished"
        else                                  echo "FAIL    — unexpected output: ${output}"
        fi
    done
}

# ---------------------------------------------------------------------------
# -setup  — init state directory + push .drives.local to each machine
# ---------------------------------------------------------------------------
cmd_setup() {
    mkdir -p "$STATE_DIR" "$LOCKS_DIR" "$RUNNING_DIR"

    # Create lock files (empty = free)
    for dk in "${NETWORK_DRIVE_KEYS[@]}"; do
        [[ ! -f "${LOCKS_DIR}/${dk}" ]] && : > "${LOCKS_DIR}/${dk}"
    done

    if [[ -f "$JOBS_FILE" ]]; then
        echo "State file already exists at ${JOBS_FILE} — not overwritten."
        echo "Delete it manually and re-run -setup to start completely fresh."
    else
        _init_jobs
        local count
        count=$(wc -l < "$JOBS_FILE")
        echo "Created ${JOBS_FILE} with ${count} jobs (all pending)."
        echo ""
        echo "Mark already-completed work before starting the orchestrator:"
        echo "  $(basename "$0") -mark-done <script> <drive_key> <machine> [machine2 ...]"
        echo "  drive_key is 'local' or one of: ${NETWORK_DRIVE_KEYS[*]}"
        echo ""
        echo "For your current state (run_k27_k32_benchmark.sh local already done):"
        echo "  $(basename "$0") -mark-done run_k27_k32_benchmark.sh local \\"
        echo "      athena s8 nvmebox gpubox rpi5 thunderx1 thunderx2 fpganode2 torus"
    fi

    echo ""
    echo "Pushing .drives.local to machines in ORCH_MACHINES ..."
    echo ""

    for machine in "${ORCH_MACHINES[@]}"; do
        pad "$machine"
        local drives="${MACHINE_LOCAL_DRIVES[$machine]:-}"
        if [[ -z "$drives" ]]; then
            echo "SKIP — no local drives configured in MACHINE_LOCAL_DRIVES"
            continue
        fi

        IFS=';' read -ra drive_arr <<< "$drives"
        local arr_lines=""
        for d in "${drive_arr[@]}"; do
            arr_lines+="  \"${d}\"\n"
        done

        printf 'FINAL_DRIVES=(\n%b)\nTEMP_DRIVES=("${FINAL_DRIVES[@]}")\n' "$arr_lines" | \
            _ssh "$machine" "cat > '${REMOTE_SCRIPTS}/.drives.local'" 2>&1
        if [[ $? -eq 0 ]]; then
            echo "OK — .drives.local written (${#drive_arr[@]} drive(s))"
        else
            echo "FAIL — could not write .drives.local"
        fi
    done
}

# ---------------------------------------------------------------------------
# -orchestrate  — scheduling loop (5-minute ticks)
# ---------------------------------------------------------------------------

_dispatch_job() {
    local machine="$1" script="$2" drive_key="$3"
    local drives script_path

    if [[ "$drive_key" == "local" ]]; then
        drives="${MACHINE_LOCAL_DRIVES[$machine]:-}"
        if [[ -z "$drives" ]]; then
            echo "    WARN [${machine}] no local drives configured — marking local jobs done"
            for s in "${ORCH_SCRIPTS[@]}"; do
                _set_job_status "${machine}:${s}:local" "done"
            done
            return 0
        fi
    else
        drives="${NETWORK_DRIVE_PATHS[$drive_key]}"
    fi

    script_path="${REMOTE_SCRIPTS}/${script}"

    local result
    result=$(_ssh "$machine" "
screen -wipe >/dev/null 2>&1
if screen -list 2>/dev/null | grep -q '\\.${SCREEN_NAME}[[:space:]]'; then
    echo BUSY
    exit 0
fi
STARTUP_LOG=\"/tmp/${SCREEN_NAME}_startup.log\"
screen -dmS '${SCREEN_NAME}' bash -c \
    \"export VAULTX_DRIVES='${drives}'; bash '${script_path}' > \\\"\${STARTUP_LOG}\\\" 2>&1\" \
    2>/dev/null || { echo FAILED; exit 0; }
sleep 5
screen -wipe >/dev/null 2>&1
if screen -list 2>/dev/null | grep -q '\\.${SCREEN_NAME}[[:space:]]'; then
    echo STARTED
else
    echo DIED
    cat \"\${STARTUP_LOG}\" 2>/dev/null
fi" 2>&1)
    local rc=$?

    if [[ $rc -ne 0 ]]; then
        echo "    FAIL [${machine}] cannot connect"
        return 1
    fi

    case "$result" in
        BUSY)
            echo "    SKIP [${machine}] screen occupied by a job outside the orchestrator — will retry next tick"
            return 1
            ;;
        STARTED)
            _set_job_status "${machine}:${script}:${drive_key}" "running"
            echo "${machine}:${script}:${drive_key}" > "${RUNNING_DIR}/${machine}"
            echo "    OK   [${machine}] ${script} on ${drive_key}"
            return 0
            ;;
        DIED*)
            echo "    FAIL [${machine}] ${script} died within 5s on ${drive_key}:"
            printf '%s\n' "$result" | grep -v '^DIED$' | head -10 | sed 's/^/         /'
            _set_job_status "${machine}:${script}:${drive_key}" "failed"
            [[ "$drive_key" != "local" ]] && _release_lock "$drive_key"
            return 1
            ;;
        FAILED)
            echo "    FAIL [${machine}] could not create screen session"
            return 1
            ;;
        *)
            echo "    FAIL [${machine}] unexpected output: ${result}"
            return 1
            ;;
    esac
}

orchestrate_tick() {
    local dispatched=0 still_running=0 completed=0 fully_done=0

    for machine in "${ORCH_MACHINES[@]}"; do
        local running_job=""
        [[ -f "${RUNNING_DIR}/${machine}" ]] && running_job=$(cat "${RUNNING_DIR}/${machine}" 2>/dev/null)

        # Check on a recorded running job
        if [[ -n "$running_job" ]]; then
            local alive
            alive=$(_ssh "$machine" "
screen -wipe >/dev/null 2>&1
screen -list 2>/dev/null | grep -q '\\.${SCREEN_NAME}[[:space:]]' && echo ALIVE || echo GONE" 2>&1)

            if [[ "$alive" == "ALIVE" ]]; then
                echo "  RUNNING  [${machine}] ${running_job}"
                (( still_running++ )) || true
                continue
            fi

            # Screen gone — job finished
            echo "  COMPLETE [${machine}] ${running_job}"
            _set_job_status "$running_job" "done"
            (( completed++ )) || true
            local done_drive="${running_job##*:}"
            [[ "$done_drive" != "local" ]] && _release_lock "$done_drive"
            rm -f "${RUNNING_DIR}/${machine}"
            running_job=""
        fi

        # Find next pending job — local jobs first (in script order), then network
        local next_script="" next_drive=""

        for script in "${ORCH_SCRIPTS[@]}"; do
            local st
            st=$(_get_job_status "${machine}:${script}:local")
            if [[ "$st" == "pending" ]]; then
                next_script="$script"
                next_drive="local"
                break
            fi
        done

        if [[ -z "$next_script" ]]; then
            for script in "${ORCH_SCRIPTS[@]}"; do
                for dk in "${NETWORK_DRIVE_KEYS[@]}"; do
                    local st
                    st=$(_get_job_status "${machine}:${script}:${dk}")
                    if [[ "$st" == "pending" ]] && _claim_lock "$dk" "$machine"; then
                        next_script="$script"
                        next_drive="$dk"
                        break 2
                    fi
                done
            done
        fi

        if [[ -n "$next_script" ]]; then
            echo "  DISPATCH [${machine}] ${next_script} on ${next_drive}"
            if ! _dispatch_job "$machine" "$next_script" "$next_drive"; then
                [[ "$next_drive" != "local" ]] && _release_lock "$next_drive"
            else
                (( dispatched++ )) || true
            fi
        else
            # Check if anything is still pending or running
            local has_work=false
            for script in "${ORCH_SCRIPTS[@]}"; do
                local st
                st=$(_get_job_status "${machine}:${script}:local")
                [[ "$st" == "pending" || "$st" == "running" ]] && { has_work=true; break; }
                for dk in "${NETWORK_DRIVE_KEYS[@]}"; do
                    st=$(_get_job_status "${machine}:${script}:${dk}")
                    [[ "$st" == "pending" || "$st" == "running" ]] && { has_work=true; break 2; }
                done
            done

            if $has_work; then
                echo "  WAITING  [${machine}] has pending jobs but all needed network drives are locked"
            else
                echo "  DONE     [${machine}] all jobs complete"
                (( fully_done++ )) || true
            fi
        fi
    done

    echo ""
    printf "  Tick summary: %d running | %d dispatched | %d just completed | %d machines fully done\n" \
        "$still_running" "$dispatched" "$completed" "$fully_done"
}

cmd_orchestrate() {
    [[ ! -f "$JOBS_FILE" ]] && {
        echo "Error: no state file. Run '$(basename "$0") -setup' first."
        exit 1
    }
    echo "Orchestrator started — ticking every 5 minutes."
    echo "Tip: run inside a screen on login1 so it survives disconnects:"
    echo "  screen -S orchestrator -dm $(realpath "$0") -orchestrate"
    echo ""
    while true; do
        echo "========================================================"
        echo " Tick: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "========================================================"
        orchestrate_tick
        echo ""
        echo " Next tick: $(date -d '+5 minutes' '+%H:%M:%S'). Ctrl+C to stop."
        sleep 300
    done
}

# ---------------------------------------------------------------------------
# -status  — print the full job table
# ---------------------------------------------------------------------------
cmd_status() {
    [[ ! -f "$JOBS_FILE" ]] && { echo "No state file. Run '$(basename "$0") -setup' first."; exit 1; }
    echo "Job Status — $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    echo "Network drive locks:"
    for dk in "${NETWORK_DRIVE_KEYS[@]}"; do
        local holder
        holder=$(_lock_holder "$dk")
        printf "  %-10s: %s\n" "$dk" "${holder:-free}"
    done
    echo ""
    printf "%-12s  %-42s  %-8s  %-9s  %-9s  %s\n" "Machine" "Script" "local" "nfs_hdd" "nfs_nvme" "ceph"
    printf "%-12s  %-42s  %-8s  %-9s  %-9s  %s\n" "-------" "------" "-----" "-------" "--------" "----"
    for machine in "${ORCH_MACHINES[@]}"; do
        for script in "${ORCH_SCRIPTS[@]}"; do
            local l h n c
            l=$(_get_job_status "${machine}:${script}:local")
            h=$(_get_job_status "${machine}:${script}:nfs_hdd")
            n=$(_get_job_status "${machine}:${script}:nfs_nvme")
            c=$(_get_job_status "${machine}:${script}:ceph")
            printf "%-12s  %-42s  %-8s  %-9s  %-9s  %s\n" \
                "$machine" "$script" "${l:-?}" "${h:-?}" "${n:-?}" "${c:-?}"
        done
    done
}

# ---------------------------------------------------------------------------
# -add-machine <machine>  — add a newly-enabled machine to the state file
# Run this after uncommenting a machine in ORCH_MACHINES, then restart the
# orchestrator so it picks up the updated array.
# ---------------------------------------------------------------------------
cmd_add_machine() {
    local machine="${1:-}"
    [[ -z "$machine" ]] && { echo "Usage: $(basename "$0") -add-machine <machine>"; exit 1; }
    [[ ! -f "$JOBS_FILE" ]] && { echo "Error: run -setup first."; exit 1; }

    echo "Adding jobs for '${machine}' ..."
    local added=0
    for script in "${ORCH_SCRIPTS[@]}"; do
        for dk in "local" "${NETWORK_DRIVE_KEYS[@]}"; do
            local job_id="${machine}:${script}:${dk}"
            local existing
            existing=$(_get_job_status "$job_id")
            if [[ -z "$existing" ]]; then
                printf '%s\tpending\t%s\n' "$job_id" "$(date +%Y-%m-%dT%H:%M:%S)" >> "$JOBS_FILE"
                (( added++ )) || true
            else
                echo "  SKIP — ${job_id} already in state (${existing})"
            fi
        done
    done
    echo "  Added ${added} jobs for ${machine}."

    echo ""
    echo "Pushing .drives.local to ${machine} ..."
    local drives="${MACHINE_LOCAL_DRIVES[$machine]:-}"
    if [[ -z "$drives" ]]; then
        echo "  SKIP — no local drives configured in MACHINE_LOCAL_DRIVES for ${machine}"
    else
        IFS=';' read -ra drive_arr <<< "$drives"
        local arr_lines=""
        for d in "${drive_arr[@]}"; do arr_lines+="  \"${d}\"\n"; done
        printf 'FINAL_DRIVES=(\n%b)\nTEMP_DRIVES=("${FINAL_DRIVES[@]}")\n' "$arr_lines" | \
            _ssh "$machine" "cat > '${REMOTE_SCRIPTS}/.drives.local'" 2>&1 \
            && echo "  OK — .drives.local written" \
            || echo "  FAIL — could not write .drives.local"
    fi

    echo ""
    echo "Now restart the orchestrator to include ${machine}:"
    echo "  screen -S orchestrator -X quit"
    echo "  screen -S orchestrator -dm $(realpath "$0") -orchestrate"
}

# ---------------------------------------------------------------------------
# -mark-done <script> <drive_key> <machine> [machine2 ...]
# ---------------------------------------------------------------------------
cmd_mark_done() {
    local script="${1:-}" drive_key="${2:-}"
    [[ -z "$script" || -z "$drive_key" ]] && {
        echo "Usage: $(basename "$0") -mark-done <script> <drive_key> <machine> [machine2 ...]"
        echo "  drive_key: 'local' or one of: ${NETWORK_DRIVE_KEYS[*]}"
        exit 1
    }
    shift 2
    [[ $# -eq 0 ]] && { echo "Error: specify at least one machine."; exit 1; }
    [[ ! -f "$JOBS_FILE" ]] && { echo "Error: run -setup first."; exit 1; }
    for machine in "$@"; do
        _set_job_status "${machine}:${script}:${drive_key}" "done"
        echo "  Marked done: ${machine}:${script}:${drive_key}"
    done
}

# ===========================================================================
# Entry point
# ===========================================================================

usage() {
    cat <<'EOF'
Usage:
  gatherdata.sh -run <scriptname>                          Start script on all HOSTNAMES
  gatherdata.sh -gather true                               Gather results from all HOSTNAMES
  gatherdata.sh -check true                                Check screen/vaultx on all HOSTNAMES
  gatherdata.sh -setup                                     Init state + push .drives.local
  gatherdata.sh -orchestrate                               Run scheduling loop (5-min ticks)
  gatherdata.sh -status                                    Show job status table
  gatherdata.sh -add-machine <machine>                     Add a machine mid-run + push drives
  gatherdata.sh -mark-done <script> <drive> <machine...>  Mark jobs as done
EOF
    exit 1
}

# Warn if sshpass is missing
for h in "${NEEDS_PASSWORD[@]}"; do
    if [[ " ${HOSTNAMES[*]} ${ORCH_MACHINES[*]} " == *" ${h} "* ]]; then
        if ! command -v sshpass &>/dev/null; then
            echo "Warning: 'sshpass' not installed but required for: ${NEEDS_PASSWORD[*]}"
            echo "         Install: sudo apt install sshpass"
            echo
        fi
        break
    fi
done

[[ $# -lt 1 ]] && usage

case "$1" in
    -run)         cmd_run       "${2:-}" ;;
    -gather)      cmd_gather ;;
    -check)       cmd_check ;;
    -setup)       cmd_setup ;;
    -orchestrate) cmd_orchestrate ;;
    -status)      cmd_status ;;
    -add-machine) cmd_add_machine "${2:-}" ;;
    -mark-done)   cmd_mark_done  "${@:2}" ;;
    *)            usage ;;
esac
