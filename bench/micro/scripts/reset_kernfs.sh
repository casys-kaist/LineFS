#! /usr/bin/sudo /bin/bash

source scripts/consts.sh
source scripts/utils.sh

# Kill kernfs
kill_kernfs() {
    nic=$1

    if [ ! -z "$nic" ] && [ "$nic" = "nic" ]; then
        echo "Kill NICFSes and host kernel workers."
    else
        echo "Kill KernFSes."
    fi

    # Kill kernfs.
    id_nic=0
    if [ ! -z "$nic" ] && [ $nic = "nic" ]; then
        $SSH_NIC_3 "pkill -9 -x kernfs" &
        pid_nic[${id_nic}]=$!
        ((id_nic = $id_nic + 1))

        $SSH_NIC_2 "pkill -9 -x kernfs" &
        pid_nic[${id_nic}]=$!
        ((id_nic = $id_nic + 1))

        $SSH_NIC_1 "pkill -9 -x kernfs" &
        pid_nic[${id_nic}]=$!
        ((id_nic = $id_nic + 1))
    fi

    id_host=0
    $SSH_HOST_3 "pkill -9 -x kernfs" &
    pid_host[${id_host}]=$!
    ((id_host = $id_host + 1))

    $SSH_HOST_2 "pkill -9 -x kernfs" &
    pid_host[${id_host}]=$!
    ((id_host = $id_host + 1))

    $SSH_HOST_1 "pkill -9 -x kernfs" &
    pid_host[${id_host}]=$!
    ((id_host = $id_host + 1))

    # wait for all pids.
    host_cnt=$id_host
    host_done=0
    nic_cnt=$id_nic
    nic_done=0
    # echo "Waiting all the processes killed."
    for pid in ${pid_host[*]}; do
        # echo "Waiting host: $pid"
        wait $pid
        ((host_done = $host_done + 1))
        echo -ne "\tHost:$host_done/$host_cnt  \tNIC: $nic_done/$nic_cnt\033[0K\r"
    done
    for pid in ${pid_nic[*]}; do
        # echo "Waiting NIC: $pid"
        wait $pid
        ((nic_done = $nic_done + 1))
        echo -ne "\tHost: $host_done/$host_cnt  \tNIC: $nic_done/$nic_cnt\033[0K\r"
    done
    echo -e "\nDone."
}

# Wait for a signal.
waitKernFSReadySignal() {
    sender=$1

    echo "Waiting for $sender's KernelWorker (LineFS) / SharedFS (Assise) is ready."
    while [ ! -f "$KERNFS_SIGNAL_DIR/$sender" ] || [ "$(cat "$KERNFS_SIGNAL_DIR/$sender")" = 0 ]; do
        sleep 1
    done
}

waitFormatDoneSignal() {
    sender=$1

    echo "Waiting for $sender's formatting finishes."
    while [ ! -f "$FORMAT_SIGNAL_DIR/$sender" ] || [ "$(cat "$FORMAT_SIGNAL_DIR/$sender")" = 0 ]; do
        sleep 1
    done
}

waitNICFSReadySignal() {
    sender=$1

    echo "Waiting for $sender's NICFS is ready."
    while [ ! -f "$KERNFS_SIGNAL_DIR_ARM/$sender" ] || [ "$(cat "$KERNFS_SIGNAL_DIR_ARM/$sender")" = 0 ]; do
        sleep 1
    done
}

format_devices() {
    echo "Format devices."
    printCmd "$SSH_HOST_3 \"(cd ${PROJ_DIR}; scripts/mkfs.sh 2&> /dev/pts/${HOST_3_TTY}; cd bench/micro; scripts/reset_kernfs.sh -f)\" &"
    $SSH_HOST_3 "(cd ${PROJ_DIR}; scripts/mkfs.sh 2&> /dev/pts/${HOST_3_TTY}; cd bench/micro; scripts/reset_kernfs.sh -f)" &
    $SSH_HOST_2 "(cd ${PROJ_DIR}; scripts/mkfs.sh 2&> /dev/pts/${HOST_2_TTY}; cd bench/micro; scripts/reset_kernfs.sh -f)" &
    $SSH_HOST_1 "(cd ${PROJ_DIR}; scripts/mkfs.sh 2&> /dev/pts/${HOST_1_TTY}; cd bench/micro; scripts/reset_kernfs.sh -f)" &
    waitFormatDoneSignal "$HOST_3"
    waitFormatDoneSignal "$HOST_2"
    waitFormatDoneSignal "$HOST_1"
}

start_host_kernfs() {
    echo "Start host KernelWorker(LineFS)/SharedFS(Assise)."
    printCmd "$SSH_HOST_3 \"(cd ${PROJ_DIR}; scripts/run_kernfs.sh 2&> /dev/pts/${HOST_3_TTY} &)\" &"

    $SSH_HOST_3 "(cd ${PROJ_DIR}; scripts/run_kernfs.sh 2&> /dev/pts/${HOST_3_TTY} &)" &
    waitKernFSReadySignal "$HOST_3"

    $SSH_HOST_2 "(cd ${PROJ_DIR}; scripts/run_kernfs.sh 2&> /dev/pts/${HOST_2_TTY} &)" &
    waitKernFSReadySignal "$HOST_2"

    $SSH_HOST_1 "(cd ${PROJ_DIR}; scripts/run_kernfs.sh 2&> /dev/pts/${HOST_1_TTY} &)" &
    waitKernFSReadySignal "$HOST_1"
}

start_nicfs() {
    echo "Start NICFSes."
    printCmd "$SSH_NIC_3 \"(cd ${NIC_PROJ_DIR}; scripts/run_kernfs.sh 2&> /dev/pts/${NIC_3_TTY} &)\""

    $SSH_NIC_3 "(cd ${NIC_PROJ_DIR}; scripts/run_kernfs.sh 2&> /dev/pts/${NIC_3_TTY} &)" &
    waitNICFSReadySignal "$NIC_3"

    $SSH_NIC_2 "(cd ${NIC_PROJ_DIR}; scripts/run_kernfs.sh 2&> /dev/pts/${NIC_2_TTY} &)" &
    waitNICFSReadySignal "$NIC_2"

    $SSH_NIC_1 "(cd ${NIC_PROJ_DIR}; scripts/run_kernfs.sh 2&> /dev/pts/${NIC_1_TTY} &)" &
    waitNICFSReadySignal "$NIC_1"
}

signalFormatDone() {
    mkdir -p "$FORMAT_SIGNAL_DIR"
    printCmd "echo 1 >$FORMAT_SIGNAL_DIR/$(hostname)"
    echo "1" >"$FORMAT_SIGNAL_DIR/$(hostname)"
}

resetSignals() {
    rm -rf "$KERNFS_SIGNAL_DIR"
    rm -rf "$FORMAT_SIGNAL_DIR"
    rm -rf "$KERNFS_SIGNAL_DIR_ARM"
    mkdir -p "$KERNFS_SIGNAL_DIR"
    mkdir -p "$FORMAT_SIGNAL_DIR"
    mkdir -p "$KERNFS_SIGNAL_DIR_ARM"
}

reset_kernfs() {
    nic=$1

    kill_kernfs "$nic"
    resetSignals

    format_devices
    start_host_kernfs

    if [ -n "$nic" ] && [ "$nic" = "nic" ]; then
        start_nicfs
    fi
}

print_usage() {
    echo "Reset KernFSes of Assise (hostonly) or NICFSes and kernel workers of NICFS (nic).
Usage: $0 [ -t hostonly|nic ] [ -k ]
    -k : Kill rather than reset.
    -t : System type. <hostonly|nic> (hostonly is default)
    -f : Signal formatting device finished."
}

if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then # script is sourced.
    unset -f print_usage
else # script is executed directly.
    KILL=0

    while getopts "fkt:?h" opt; do
        case $opt in
        k)
            KILL=1
            ;;
        f)
            signalFormatDone
            exit
            ;;
        t)
            SYSTEM=$OPTARG
            ;;
        h | ?)
            print_usage
            exit 2
            ;;
        esac
    done

    if [ "$SYSTEM" != "hostonly" ] && [ "$SYSTEM" != "nic" ]; then
        print_usage
        exit 2
    fi

    if [ $KILL = 1 ]; then
        kill_kernfs "$SYSTEM"
    else
        reset_kernfs "$SYSTEM"
    fi
fi