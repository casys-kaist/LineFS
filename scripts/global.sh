#!/bin/bash
PROJ_DIR="/home/yulistic/assise-nic"     # Host's project root directory path.
NIC_PROJ_DIR="/home/yulistic/assise-nic" # NIC's project root directory path.

SYS=$(gcc -dumpmachine)
if [ $SYS = "aarch64-linux-gnu" ]; then
	PINNING="" # There is only one socket in the SmartNIC.
else
	PINNING="numactl -N1 -m1"
fi

HOST_1="libra06"
HOST_2="libra08"
HOST_3="libra09"

NIC_1="libra06-nic-rdma"
NIC_2="libra08-nic-rdma"
NIC_3="libra09-nic-rdma"
