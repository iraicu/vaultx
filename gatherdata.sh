#!/usr/bin/env bash
# gatherdata.sh — Orchestrate data collection across lab machines from login1
#
# Usage:
#   gatherdata.sh -run <scriptname>   Start <scriptname> in screen 'plotting' on all hosts
#   gatherdata.sh -gather true        Copy results from all hosts into ~/Results/
#   gatherdata.sh -check true         Check if screen 'plotting' is running on all hosts

# ---------------------------------------------------------------------------
# Configuration — edit these as needed
# ---------------------------------------------------------------------------

HOSTNAMES=(epycbox athena s8 nvmebox gpubox opi5 rpi5 thunderx1 thunderx2 fpganode2 torus)

# Hosts that require password authentication
NEEDS_PASSWORD=(s8 opi5 rpi5)
SSH_PASSWORD="sfatunmbi"

# Remote layout (same on every machine)
REMOTE_USER="sfatunmbi"
REMOTE_BASE="/home/${REMOTE_USER}/vaultx"

# Where gathered results land on login1
RESULTS_BASE="${HOME}/Results"

# Screen session name
SCREEN_NAME="plotting"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

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
            -T \
            "$host" "$@"
    else
        ssh \
            -o StrictHostKeyChecking=no \
            -o ConnectTimeout=10 \
            -o BatchMode=yes \
            -T \
            "$host" "$@"
    fi
}

_rsync() {
    local host="$1" src="$2" dst="$3"
    local ssh_e="ssh -o StrictHostKeyChecking=no -o ConnectTimeout=10"
    if _needs_password "$host"; then
        sshpass -p "$SSH_PASSWORD" rsync -az --stats -e "$ssh_e" "${host}:${src}" "$dst"
    else
        rsync -az --stats -e "$ssh_e" "${host}:${src}" "$dst"
    fi
}

pad() { printf "[%-12s] " "$1"; }

# ---------------------------------------------------------------------------
# -run <scriptname>
# Start the named script in a detached screen session on every host.
# If 'plotting' already exists on a host, report and leave it alone.
# ---------------------------------------------------------------------------
cmd_run() {
    local script_name="$1"
    [[ -z "$script_name" ]] && { echo "Usage: $(basename "$0") -run <scriptname>"; exit 1; }

    local script_path="${REMOTE_BASE}/${script_name}"

    echo "Launching '${script_name}' in screen '${SCREEN_NAME}' on ${#HOSTNAMES[@]} machines ..."
    echo

    for host in "${HOSTNAMES[@]}"; do
        pad "$host"

        result=$(_ssh "$host" "
if screen -list 2>/dev/null | grep -q '\\.${SCREEN_NAME}[[:space:]]'; then
    echo EXISTS
else
    cd '${REMOTE_BASE}' && \
        screen -dmS '${SCREEN_NAME}' bash '${script_path}' 2>/dev/null && \
        echo STARTED || echo FAILED
fi" 2>&1)
        rc=$?

        if   [[ $rc -ne 0 ]];             then echo "FAIL   — cannot connect (SSH exit ${rc})"
        elif [[ "$result" == "EXISTS" ]];  then echo "SKIP   — screen '${SCREEN_NAME}' already running, left untouched"
        elif [[ "$result" == "STARTED" ]]; then echo "OK     — '${script_name}' started in screen '${SCREEN_NAME}'"
        elif [[ "$result" == "FAILED" ]];  then echo "FAIL   — screen created but script could not start (check path/permissions)"
        else                                    echo "FAIL   — unexpected output: ${result}"
        fi
    done
}

# ---------------------------------------------------------------------------
# -gather true
# rsync vaultx/newexperiments/<host>/ from each machine into ~/Results/<host>/
# ---------------------------------------------------------------------------
cmd_gather() {
    mkdir -p "$RESULTS_BASE"
    echo "Gathering results into ${RESULTS_BASE}/ ..."
    echo

    for host in "${HOSTNAMES[@]}"; do
        pad "$host"
        local local_dest="${RESULTS_BASE}/${host}"
        mkdir -p "$local_dest"

        output=$(_rsync "$host" "${REMOTE_BASE}/newexperiments/${host}/" "$local_dest" 2>&1)
        rc=$?

        if [[ $rc -eq 0 ]]; then
            transferred=$(echo "$output" | grep "Number of regular files transferred:" | awk '{print $NF}')
            echo "OK     — ${transferred:-0} file(s) synced → ${local_dest}"
        else
            last_err=$(echo "$output" | tail -1)
            echo "FAIL   — rsync exit ${rc}: ${last_err}"
        fi
    done
}

# ---------------------------------------------------------------------------
# -check true
# Report whether screen 'plotting' is still alive on each host.
# ---------------------------------------------------------------------------
cmd_check() {
    echo "Checking screen '${SCREEN_NAME}' on ${#HOSTNAMES[@]} machines ..."
    echo

    for host in "${HOSTNAMES[@]}"; do
        pad "$host"

        output=$(_ssh "$host" "screen -list 2>/dev/null" 2>&1)
        rc=$?

        if [[ $rc -ne 0 ]]; then
            echo "FAIL   — cannot connect (SSH exit ${rc})"
            continue
        fi

        match=$(echo "$output" | grep "\.${SCREEN_NAME}[[:space:]]")
        if [[ -n "$match" ]]; then
            state=$(echo "$match" | sed 's/.*(\([^)]*\)).*/\1/')
            echo "RUNNING — screen '${SCREEN_NAME}' is ${state:-active}"
        else
            echo "DONE   — no screen '${SCREEN_NAME}' found (experiment finished or not yet started)"
        fi
    done
}

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

usage() {
    cat <<'EOF'
Usage:
  gatherdata.sh -run <scriptname>   Start <scriptname> in screen 'plotting' on all hosts
  gatherdata.sh -gather true        Copy results from all hosts into ~/Results/
  gatherdata.sh -check true         Check if screen 'plotting' is running on all hosts
EOF
    exit 1
}

# Warn early if sshpass is missing but required for at least one active host
for h in "${NEEDS_PASSWORD[@]}"; do
    if [[ " ${HOSTNAMES[*]} " == *" ${h} "* ]]; then
        if ! command -v sshpass &>/dev/null; then
            echo "Warning: 'sshpass' is not installed but required for: ${NEEDS_PASSWORD[*]}"
            echo "         Install it with: sudo apt install sshpass"
            echo
        fi
        break
    fi
done

[[ $# -lt 1 ]] && usage

case "$1" in
    -run)    cmd_run    "${2:-}" ;;
    -gather) cmd_gather ;;
    -check)  cmd_check  ;;
    *)       usage ;;
esac
