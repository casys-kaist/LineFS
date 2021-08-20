#!/bin/sh

umount /mnt/ramdisk
umount /mnt/scratch
rmmod nova
insmod nova.ko measure_timing=0

sleep 1

mount -t NOVA -o init /dev/pmem0m /mnt/ramdisk
mount -t NOVA -o init /dev/pmem1m /mnt/scratch

