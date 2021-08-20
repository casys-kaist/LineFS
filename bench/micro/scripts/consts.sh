#!/bin/bash
source ../../scripts/global.sh

DATE=$(date +"%y%m%d-%H%M%S")
PARSEC_DIR="${PROJ_DIR}/bench/parsec"

# Set tty for output. You can find the number X (/dev/pts/X) by the command: `tty`
HOST_1_TTY=1
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