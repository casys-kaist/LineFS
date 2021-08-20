#! /bin/bash

PATH=$PATH:.
SRC_ROOT=../../
export LD_LIBRARY_PATH=$SRC_ROOT/libfs/lib/nvml/src/nondebug/:$SRC_ROOT/libfs/build:$SRC_ROOT/shim/glibc-build/rt/ 
LD_PRELOAD=$SRC_ROOT/shim/libshim/libshim.so fio.mlfs nvm_mlfs.fio 

#LD_PRELOAD=$SRC_ROOT/shim/libshim/libshim.so fio.mlfs nvm_mlfs.fio 
#LD_PRELOAD=$SRC_ROOT/shim/libshim/libshim.so fio.mlfs --debug=file,io nvm_mlfs.fio 
#LD_PRELOAD=$SRC_ROOT/shim/libshim/libshim.so strace -ff fio.mlfs nvm_mlfs.fio 2> f.strace
#LD_PRELOAD=$SRC_ROOT/shim/libshim/libshim.so ltrace -f fio.mlfs nvm_mlfs.fio 2> f.strace
