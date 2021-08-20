#!/bin/bash
LIBFS_NUM=$1
ID=$2
FILE_SIZE="$((12/LIBFS_NUM))G"
PINNING="numactl -N1 -m1"

echo "ID:$ID FILE_SIZE:$FILE_SIZE"

# normal run.
CMD="sudo FILE_ID=$ID nice -n -20 $PINNING ./run.sh ./iobench -d /mlfs/$ID -s -w sw $FILE_SIZE 16K 1"

# gdb run.
# CMD="sudo FILE_ID=$ID nice -n -20 $PINNING gdb --tty=/dev/pts/5 -ex run --args ./iobench -d /mlfs/$ID -s -w sw $FILE_SIZE 16K 1"
# CMD="sudo FILE_ID=$ID nice -n -20 $PINNING gdb -ex run --args ./iobench -d /mlfs/$ID -s -w sw $FILE_SIZE 4K 1"
# CMD="sudo FILE_ID=$ID nice -n -20 $PINNING gdb --tty=/tmp/gdb_iobench_out -ex run --args ./iobench -d /mlfs/$ID -s -w sw $FILE_SIZE 4K 1"

# infinite run.
# CMD="sudo FILE_ID=$ID nice -n -20 $PINNING ./run.sh ./iobench.infinite -d /mlfs/$ID -s -w sw $FILE_SIZE 16K 1"

# ntimes run.
# CMD="sudo FILE_ID=$ID nice -n -20 $PINNING ./run.sh ./iobench.ntimes -d /mlfs/$ID -s -w sw $FILE_SIZE 16K 1"

echo $CMD
$CMD