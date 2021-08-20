#!/bin/bash
PINNING="numactl -N1 -m1"

sudo nice -n -20 $PINNING ./run.sh ./iobench_lat -s sw 1G 4K 1

# gdb
# sudo nice -n -20 $PINNING gdb --tty=/dev/pts/12 -ex run --args iobench_lat -s sw 1G 16K 1
