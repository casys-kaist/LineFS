#! /bin/bash

mkdir -p ./ramfs

sudo mount -t tmpfs -o size=3G tmpfs ./ramfs
