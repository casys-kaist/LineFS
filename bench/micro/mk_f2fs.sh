#! /bin/bash

user=$(whoami)

sudo mkfs.f2fs /dev/nvme0n1
mkdir -p ./ssd
sudo mount /dev/nvme0n1 ./ssd/
sudo chown $user ./ssd
