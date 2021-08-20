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

start_kernfs() {
    nic=$1

    # Start kernfs.
    if [ ! -z "$nic" ] && [ "$nic" = "nic" ]; then
        echo "Start NICFSes and host kernel workers."
    else
        echo "Start KernFSes."
    fi

    printCmd "$SSH_HOST_3 \"(cd ${PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${HOST_3_TTY} &)\""
    $SSH_HOST_3 "(cd ${PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${HOST_3_TTY} &)" &

    printCmd "$SSH_HOST_2 \"(cd ${PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${HOST_2_TTY} &)\""
    $SSH_HOST_2 "(cd ${PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${HOST_2_TTY} &)" &

    printCmd "$SSH_HOST_1 \"(cd ${PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${HOST_1_TTY} &)\""
    $SSH_HOST_1 "(cd ${PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${HOST_1_TTY} &)" &

    if [ ! -z "$nic" ] && [ "$nic" = "nic" ]; then
        printCmd "$SSH_NIC_3 \"(cd ${NIC_PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${NIC_3_TTY} &)\""
        $SSH_NIC_3 "(cd ${NIC_PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${NIC_3_TTY} &)" &

        printCmd "$SSH_NIC_2 \"(cd ${NIC_PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${NIC_2_TTY} &)\""
        $SSH_NIC_2 "(cd ${NIC_PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${NIC_2_TTY} &)" &

        printCmd "$SSH_NIC_1 \"(cd ${NIC_PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${NIC_1_TTY} &)\""
        $SSH_NIC_1 "(cd ${NIC_PROJ_DIR}; scripts/mkfs_run_kernfs.sh 2&> /dev/pts/${NIC_1_TTY} &)" &
    fi
}

reset_kernfs() {
    nic=$1

    kill_kernfs "$nic"
    # sleep 3

    start_kernfs "$nic"

    if [ "$nic" = "nic" ]; then
        echo "Waiting for all the NICFSes and kernel workers to be ready. (Adjust sleep time at reset_kernfs() in reset_kernfs.sh)"
        # sleepAndCountdown 55 # 12G file size.
        sleepAndCountdown 70 # 19G file size.
        # sleepAndCountdown 140 # 24G file size.
        # sleepAndCountdown 150 # 48G file size.
    else
        echo "Waiting for all the KernFSes to be ready. (Adjust sleep time at reset_kernfs() in reset_kernfs.sh)"
        # sleepAndCountdown 50 # 12G file size.
        sleepAndCountdown 65 # 19G file size.
        # sleepAndCountdown 80 # 24G file size.
        # sleepAndCountdown 100 # 48G file size.
    fi

    # (cd ${PROJ_DIR}/libfs/tests; ./run.sh ./mkdir_test 2&> /dev/pts/6 &)
    # sleep 5
}

print_usage() {
    echo "Reset KernFSes of Assise (hostonly) or NICFSes and kernel workers of NICFS (nic).
Usage: $0 [ -t hostonly|nic ] [ -k ]
    -k : Kill rather than reset.
    -t : System type. <hostonly|nic> (hostonly is default)"
}

KILL=0

while getopts "kt:?h" opt; do
    case $opt in
    k)
        KILL=1
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

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then # script is executed directly.
    if [ $KILL = 1 ]; then
        kill_kernfs "$SYSTEM"
    else
        reset_kernfs "$SYSTEM"
    fi
fi
