#! /bin/bash

user=$(whoami)

sudo mkfs.ext4 /dev/pmem0
mkdir -p ./pmem
sudo mount -t ext4 -o dax /dev/pmem0 ./pmem
sudo chown $user ./pmem

