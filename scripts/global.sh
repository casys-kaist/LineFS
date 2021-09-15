#!/bin/bash
## Set paths in X86 host.
PROJ_DIR="/path/to/proj_root/in/x86/host" ### Project root directory path in x86 host.
SIGNAL_DIR="$PROJ_DIR/scripts/signals"
KERNFS_SIGNAL_DIR="$SIGNAL_DIR/kernfs"
FORMAT_SIGNAL_DIR="$SIGNAL_DIR/mkfs"
NIC_SRC_DIR="/path/to/arm/source/code/in/x86/host" ### Path of the host directory that includes source code for NIC.
SIGNAL_DIR_ARM="$NIC_SRC_DIR/scripts/signals"
KERNFS_SIGNAL_DIR_ARM="$SIGNAL_DIR_ARM/nicfs"

## Set paths in ARM.
NIC_PROJ_DIR="/path/to/proj_root/in/nic" ### Project root directory path in SmartNIC.
NIC_SIGNAL_DIR="$NIC_PROJ_DIR/scripts/signals"
NICFS_SIGNAL_DIR="$NIC_SIGNAL_DIR/kernfs"

SYS=$(gcc -dumpmachine)
if [ $SYS = "aarch64-linux-gnu" ]; then
	PINNING="" # There is only one socket in the SmartNIC.
else
	PINNING="numactl -N0 -m0"
fi

# Hostnames of X86 hosts.
# You can get this values by running `hostname` command on each X86 host.
HOST_1="libra06"
HOST_2="libra08"
HOST_3="libra09"

# Hostname (or IP address) of host machines. You should be able to ssh to each machine with these names.
HOST_1_INF="libra06"
HOST_2_INF="libra08"
HOST_3_INF="libra09"

# Hostnames of NICs
# You can get this values by running `hostname` command on each NIC.
NIC_1="libra06-nic"
NIC_2="libra08-nic"
NIC_3="libra09-nic"

# Name (or IP address) of RDMA interface of NICs. You should be able to ssh to each NIC with these names.
NIC_1_INF="libra06-nic-rdma"
NIC_2_INF="libra08-nic-rdma"
NIC_3_INF="libra09-nic-rdma"

# Set tty for output. You can find the number X (/dev/pts/X) with the command: `tty`
HOST_1_TTY=33
HOST_2_TTY=0
HOST_3_TTY=0

NIC_1_TTY=0
NIC_2_TTY=0
NIC_3_TTY=0

SSH_HOST_1="ssh $HOST_1_INF"
SSH_HOST_2="ssh $HOST_2_INF"
SSH_HOST_3="ssh $HOST_3_INF"

SSH_NIC_1="ssh $NIC_1_INF"
SSH_NIC_2="ssh $NIC_2_INF"
SSH_NIC_3="ssh $NIC_3_INF"

buildLineFS() {
	echo "Building LineFS."
	(
		cd "$PROJ_DIR" || { echo "Project root directory not found."; exit 1; }

		echo "Building Kernel Worker and LibFS in host ($HOST_1)."
		make kernfs-linefs &>/dev/null && make libfs-linefs &>/dev/null || { echo "Building LineFS failed."; exit 1; }

		echo "Copying source code to ARM directory."
		sudo $PROJ_DIR/scripts/push_src.sh &>/dev/null # Copy source codes to ARM directory.
		rm -rf $NIC_SRC_DIR/kernfs/buildarm # Workaround: delete buildarm directory manually. 'make clean' over ssh to NIC1 does not work.
	) || exit 1
	echo "Building NICFS in NIC ($NIC_1)."
	# $SSH_NIC_1 "(ho Building NICFS. &> /dev/pts/${NIC_1_TTY}; cd ${NIC_PROJ_DIR}; make kernfs-linefs &> /dev/null; echo Done. &> /dev/pts/${NIC_1_TTY})"
	$SSH_NIC_1 "(echo Building NICFS. &> /dev/pts/${NIC_1_TTY}; cd ${NIC_PROJ_DIR}; make kernfs-linefs &> /dev/pts/${NIC_1_TTY}; echo Done. &> /dev/pts/${NIC_1_TTY})" || { echo "Building NICFS failed."; exit 1; }
	echo Building LineFS done.
}

buildAssise() {
	echo "Building Assise."
	(
		cd "$PROJ_DIR" || { echo "Project root directory not found."; exit 1; }
		make kernfs-assise &>/dev/null && make libfs-assise &>/dev/null || { echo "Building Assise failed."; exit 1; }
	) || exit 1
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

setMemcpyBatching() {
	val=$1
	echo "Set Memcpy Batching config to $val."
	(
		cd "$PROJ_DIR" || exit
		if [ "$val" = 0 ]; then
			sed -i 's/.*WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST/# WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST/g' kernfs/Makefile
			sed -i 's/.*WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST/# WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST/g' libfs/Makefile
		else
			sed -i 's/.*WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST/WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST/g' kernfs/Makefile
			sed -i 's/.*WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST/WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST/g' libfs/Makefile
		fi
	)
}

dumpConfigs() {
	config_dump_path="$1"
	mkdir -p "$config_dump_path"
	cp $PROJ_DIR/kernfs/Makefile "$config_dump_path/Makefile.kernfs"
	cp $PROJ_DIR/libfs/Makefile "$config_dump_path/Makefile.libfs"
	cp $PROJ_DIR/mlfs_config.sh "$config_dump_path/mlfs_config"
}
