#! /bin/bash

user=$(whoami)

SRC_ROOT=.

sudo mkfs.ext4 /dev/pmem0
sudo mount -t ext4 -o dax /dev/pmem0 $SRC_ROOT/pmem
sudo chown $user $SRC_ROOT/pmem
