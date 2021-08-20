#!/bin/bash

MODES="sw rw"
BSIZES="4K 64K 1M 16M"
TOTAL_SIZES="512M 2G"

mkdir -p results
mkdir -p ext4

for mode in $MODES; do
    for block in $BSIZES; do
	for size in $TOTAL_SIZES; do
	    output_file="results/ext4_"$mode"_"$size"_"$block".txt"
	    sudo umount ./ext4 2> /dev/null
	    sudo mkfs.ext4 -F /dev/nvme1n1 2> /dev/null
	    sudo mount -t ext4 /dev/nvme1n1 ext4/ 2> /dev/null
	    echo Running $output_file
	    sudo ./write_read.normal $mode ext4 ext4/aa $size $block | tee $output_file
	    sleep 1
	done
    done
done
