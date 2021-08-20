#!/bin/bash
SRC_ROOT_PATH="/src/proj/rootdir/"
TARGET_ROOT_PATH="/tar/proj/rootdir/"  # The last "/" should not be skipped.

# SRC_ROOT_PATH="/home/yulistic/assise-nic/"
# TARGET_ROOT_PATH="/home/yulistic/assise-nic-arm/"  # The last "/" should not be skipped.
# TARGET_ROOT_PATH="/home/yulistic/playground/LineFS/"  # The last "/" should not be skipped.

if [ $# -ge 1 ] && [ $1 = "all" ] ; then
    echo "rsync all."
    rsync -av $SRC_ROOT_PATH ${TARGET_ROOT_PATH} &

elif [ $# -ge 1 ] && [ $1 = "rdma" ] ; then
    echo "sync libfs/lib/rdma only."
    rsync -av ${SRC_ROOT_PATH}libfs/lib/rdma/ ${TARGET_ROOT_PATH}libfs/lib/rdma/ &

else
    echo "exclude some directories."
    rsync -av \
        --exclude='libfs/lib' \
        --exclude='libfs/tests' \
        --exclude='kernfs/lib' \
        --exclude='bench' \
        --exclude='sleep.sh' \
        $SRC_ROOT_PATH ${TARGET_ROOT_PATH} &
        # --exclude='kernfs/distributed/rpc_interface.h' \
        # --exclude='libfs/src/distributed/rpc_interface.h' \
        # --exclude='kernfs/storage/storage.c' \
        # --exclude='libfs/src/storage/storage.c' \
        # --exclude='kernfs/Makefile' \
        # --exclude='libfs/Makefile' \
        # --exclude='run_kernfs.sh' \
        # --exclude='mkfs_run_kernfs.sh' \
        # --exclude='gdb_kernfs.sh' \
        # --exclude='mlfs_config.sh' \
        # --exclude='mlfs_config.gdb' \
fi