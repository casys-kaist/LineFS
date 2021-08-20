#! /bin/bash
PATH=$PATH:.
SRC_ROOT=../../../
source $SRC_ROOT/scripts/global.sh
source $SRC_ROOT/mlfs_config.sh
export LD_LIBRARY_PATH=$SRC_ROOT/libfs/lib/nvml/src/nondebug/:$SRC_ROOT/libfs/build:/usr/local/lib/:/usr/lib/x86_64-linux-gnu/:/lib/x86_64-linux-gnu/:/usr/lib/
$PINNING ${@}
