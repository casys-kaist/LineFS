#!/bin/sh

echo "Unmount existing partition..."
umount /mnt/ramdisk
rmmod nova
insmod nova.ko measure_timing=0

echo "Unmount done."
sleep 1

echo "Mounting..."
mount -t NOVA /dev/pmem0 /mnt/ramdisk

