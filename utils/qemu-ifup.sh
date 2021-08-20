#!/bin/sh

## Inspired by: http://www.linuxquestions.org/questions/linux-networking-3/kvm-qemu-and-nat-on-the-host-machine-mini-tutorial-697980/

UTILS_DIR=$(dirname $0)
. ${UTILS_DIR}/qemu-if.conf

# set -x

echo ${0}:
echo "Setting up the network bridge for $1"

brctl addbr ${BRIDGE}
brctl addif ${BRIDGE} ${1}
ifconfig ${BRIDGE} ${HOST} netmask ${MASK}
ip link set ${1} up
ip link set ${BRIDGE} up

if iptables -t nat -L POSTROUTING -n | grep ^MASQUERADE | awk '{print $4}' | cut -d/ -f1 | grep ${NETWORK} > /dev/null; then
    echo "IP masquerading already set up"
else
    echo "Setting up IP masquerading"
    iptables -t nat -A POSTROUTING -s ${NETWORK}/${SHORT_MASK} ! -d ${NETWORK}/${SHORT_MASK} -j MASQUERADE
fi

echo "Setting up IP forwarding"
sysctl net.ipv4.ip_forward=1

exit 0
