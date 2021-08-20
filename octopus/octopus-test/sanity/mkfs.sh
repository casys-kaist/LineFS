#! /usr/bin/sudo /bin/bash

LD_LIBRARY_PATH=../lib/nvml/src/nondebug/:../lib/libspdk/libspdk/ ../bin/mkfs.mlfs 4 $@
#LD_LIBRARY_PATH=../lib/nvml/src/nondebug/:../lib/libspdk/libspdk/ ../bin/mkfs.mlfs 5 $@
