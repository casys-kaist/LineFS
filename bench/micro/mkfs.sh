#! /usr/bin/sudo /bin/bash

PROJECT_ROOT=../../

LD_LIBRARY_PATH=$PROJECT_ROOT/libfs/lib/nvml/src/nondebug/:$PROJECT_ROOT/libfs/lib/libspdk/libspdk/ $PROJECT_ROOT/libfs/bin/mkfs.mlfs 4 $@
LD_LIBRARY_PATH=$PROJECT_ROOT/libfs/lib/nvml/src/nondebug/:$PROJECT_ROOT/libfs/lib/libspdk/libspdk/ $PROJECT_ROOT/libfs/bin/mkfs.mlfs 5 $@
