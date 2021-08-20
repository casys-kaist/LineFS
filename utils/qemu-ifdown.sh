#!/bin/sh

UTILS_DIR=$(dirname $0)
. ${UTILS_DIR}/qemu-if.conf

# set -x

echo "Tearing down network bridge for $1"
ip link set ${1} down
brctl delif ${BRIDGE} ${1}

#ifconfig ${BRIDGE} 0.0.0.0
#ip link set ${BRIDGE} down
#brctl delbr ${BRIDGE}

exit 0
