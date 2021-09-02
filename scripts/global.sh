#!/bin/bash
PROJ_DIR="/path/to/host/proj_root"     # Host's project root directory path.
NIC_PROJ_DIR="/path/to/nic/proj_root" # NIC's project root directory path.

SYS=$(gcc -dumpmachine)
if [ $SYS = "aarch64-linux-gnu" ]; then
	PINNING="" # There is only one socket in the SmartNIC.
else
	PINNING="numactl -N0 -m0"
fi

HOST_1="libra06"
HOST_2="libra08"
HOST_3="libra09"

NIC_1="libra06-nic-rdma"
NIC_2="libra08-nic-rdma"
NIC_3="libra09-nic-rdma"

buildLineFS() {
	echo "Building LineFS."
	(
		cd "$PROJ_DIR" || exit
		make kernfs-linefs && make libfs-linefs || exit 1
	)
}

buildAssise() {
	echo "Building Assise."
	(
		cd "$PROJ_DIR" || exit
		make kernfs-assise && make libfs-assise || exit 1
	)
}

setAsyncReplicationOn() {
	echo "Enable Async (background) replication. ASYNC_REPLICATION=1 in mlfs_config.sh"
	(
		cd "$PROJ_DIR" || exit
		sed -i 's/export ASYNC_REPLICATION=0/export ASYNC_REPLICATION=1/g' mlfs_config.sh
	)
}

setAsyncReplicationOff() {
	echo "Disable Async (background) replication. ASYNC_REPLICATION=0 in mlfs_config.sh"
	(
		cd "$PROJ_DIR" || exit
		sed -i 's/export ASYNC_REPLICATION=1/export ASYNC_REPLICATION=0/g' mlfs_config.sh
	)
}
