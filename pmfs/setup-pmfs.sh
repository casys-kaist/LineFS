#!/bin/sh

umount /mnt/ramdisk
umount /mnt/scratch
rmmod pmfs
insmod pmfs.ko measure_timing=0

sleep 1

mount -t pmfs -o init /dev/pmem0m /mnt/ramdisk
mount -t pmfs -o init /dev/pmem1m /mnt/scratch

