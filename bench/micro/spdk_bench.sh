#!/bin/bash

MODES="sw rw"
BSIZES="4K 64K 1M 16M"
TOTAL_SIZES="512M 2G"

for mode in $MODES; do
    for block in $BSIZES; do
	for size in $TOTAL_SIZES; do
	    output_file="results/spdk_"$mode"_"$size"_"$block".txt"
	    echo Running $output_file
	    sudo ./write_read.normal $mode spdk aa $size $block | tee $output_file
	    sleep 1
	done
    done
done
