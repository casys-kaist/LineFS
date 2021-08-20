#! /bin/bash

user=$(whoami)

mkdir -p ./pmem
sudo mount -t pmfs -o init /dev/pmem0 ./pmem
sudo chown $user ./pmem
