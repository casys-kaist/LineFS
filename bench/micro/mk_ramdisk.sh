#! /bin/bash

# Ramblock device module parameters.
# rd_nr : Maximum number of brd devices
# rd_size : Size of each RAM disk in kbytes.
# max_part : Maximum number of partitions per RAM disk

size=$(expr 10 \* 1024) # 10MB
user=$(whoami)

sudo modprobe brd rd_size=$size max_part=1 rd_nr=1

sudo mkfs.ext4 /dev/ram0

mkdir -p ./ramdisk
#umount ./ramdisk
sudo mount /dev/ram0 ./ramdisk

sudo chown $user ./ramdisk
