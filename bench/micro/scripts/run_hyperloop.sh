#!/bin/bash
source scripts/consts.sh
HYPERLOOP_SERV_DIR="${PROJ_DIR}/libfs/lib/hyperloop/build/apps/server"
CONFIG_FILE="${PROJ_DIR}/mlfs_config.sh"

# Replace trace file path in mlfs_config.sh
replaceHyperloopTracePath()
{
    trace_file_path=$1

    echo -n "Configuration replaced from:"
    grep "HYPERLOOP_OPS_FILE_PATH" $CONFIG_FILE

    sed -i "/export HYPERLOOP_OPS_FILE_PATH*/c\export HYPERLOOP_OPS_FILE_PATH='${trace_file_path}'" ${PROJ_DIR}/mlfs_config.sh
    $SSH_HOST_2 "sed -i \"/export HYPERLOOP_OPS_FILE_PATH*/c\export HYPERLOOP_OPS_FILE_PATH='${trace_file_path}'\" ${PROJ_DIR}/mlfs_config.sh"
    $SSH_HOST_3 "sed -i \"/export HYPERLOOP_OPS_FILE_PATH*/c\export HYPERLOOP_OPS_FILE_PATH='${trace_file_path}'\" ${PROJ_DIR}/mlfs_config.sh"

    echo -n "to:"
    grep "HYPERLOOP_OPS_FILE_PATH" $CONFIG_FILE
}

runHyperloopServers()
{
    $SSH_HOST_3 "(cd ${HYPERLOOP_SERV_DIR}; sudo numactl -N0 -m0 ./server --port 50003 -m -i 3 2&> /dev/pts/2 &)"
    $SSH_HOST_3 "(cd ${HYPERLOOP_SERV_DIR}; sudo numactl -N0 -m0 ./server --port 50002 2&> /dev/pts/0 &)"

    $SSH_HOST_2 "(cd ${HYPERLOOP_SERV_DIR}; sudo numactl -N0 -m0 ./server --connect 192.168.13.115:50003 --port 50001 -m -i 3 2&> /dev/pts/2 &)"
    $SSH_HOST_2 "(cd ${HYPERLOOP_SERV_DIR}; sudo numactl -N0 -m0 ./server --connect 192.168.13.115:50002 --port 50000 2&> /dev/pts/1 &)"
}

killHyperloopServers()
{
    $SSH_HOST_3 pkill -9 server
    $SSH_HOST_2 pkill -9 server
}

# Test.
# runHyperloopServers
# killHyperloopServers
