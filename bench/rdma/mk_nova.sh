#! /bin/bash

user=$(whoami)

mkdir -p ./pmem
sudo mount -t NOVA -o init,sync /dev/pmem0 ./pmem
sudo chown $user ./pmem
