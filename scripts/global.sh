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

# Set tty for output. You can find the number X (/dev/pts/X) with the command: `tty`
HOST_1_TTY=0
HOST_2_TTY=0
HOST_3_TTY=0

NIC_1_TTY=0
NIC_2_TTY=0
NIC_3_TTY=0

SSH_HOST_1="ssh $HOST_1"
SSH_HOST_2="ssh $HOST_2"
SSH_HOST_3="ssh $HOST_3"

SSH_NIC_1="ssh $NIC_1"
SSH_NIC_2="ssh $NIC_2"
SSH_NIC_3="ssh $NIC_3"

buildLineFS() {
	echo "Building LineFS."
	(
		cd "$PROJ_DIR" || exit

		echo "Building Kernel Worker and LibFS in host ($HOST_1)."
		make kernfs-linefs &>/dev/null && make libfs-linefs &>/dev/null || exit 1

		echo "Copying source code to ARM directory."
		sudo $PROJ_DIR/scripts/push_src.sh &>/dev/null # Copy source codes to ARM directory.
		sleep 1
	)
	echo "Building NICFS in NIC ($NIC_1)."
	# $SSH_NIC_1 "(cd ${NIC_PROJ_DIR}; make kernfs &> /dev/pts/${NIC_1_TTY})"
	$SSH_NIC_1 "(echo Building NICFS. &> /dev/pts/${NIC_1_TTY}; cd ${NIC_PROJ_DIR}; make kernfs-linefs &> /dev/null; echo Done. &> /dev/pts/${NIC_1_TTY})"
	echo Building LineFS done.
}

buildAssise() {
	echo "Building Assise."
	(
		cd "$PROJ_DIR" || exit
		make kernfs-assise &>/dev/null && make libfs-assise &>/dev/null || exit 1
	)
	echo "Building Assise done."
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

dumpConfigs() {
	config_dump_path="$1"
	cp $PROJ_DIR/kernfs/Makefile "$config_dump_path/Makefile.kernfs"
	cp $PROJ_DIR/libfs/Makefile "$config_dump_path/Makefile.libfs"
	cp $PROJ_DIR/mlfs_config.sh $config_dump_path/mlfs_config
}
