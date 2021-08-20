#! /bin/bash

#benchmark=fillseq
#benchmark=readrandom
#LD_PRELOAD=$SRC_ROOT/shim/libshim/libshim.so taskset -c 0,7 ./db_bench.mlfs --db=/mlfs --num=500000 --value_size=1024


#./run.sh ./db_bench.mlfs --db=/mlfs --num=400000 --value_size=1024 --benchmarks=fillseq,readseq
./run.sh ./db_bench.mlfs --db=/mlfs --num=400000 --value_size=1024 --benchmarks=fillseq,readrandom
#./run.sh ./db_bench.mlfs --db=/mlfs --num=400000 --value_size=1024 --benchmarks=fillseq,readhot
#./run.sh ./db_bench.mlfs --db=/mlfs --num=400000 --value_size=1024 --benchmarks=fillseq
#./run.sh ./db_bench.mlfs --db=/mlfs --num=400000 --value_size=1024 --benchmarks=fillrandom
#./run.sh ./db_bench.mlfs --db=/mlfs --num=400000 --value_size=1024 --benchmarks=fillsync

#./run.sh ltrace -f ./db_bench.mlfs --db=/mlfs --num=400000 --value_size=1024 --benchmarks=fillsync 2> debug.ltrace

#LD_PRELOAD=$SRC_ROOT/shim/libshim/libshim.so taskset -c 0,7 ./db_bench.mlfs --db=/mlfs --num=30000 --value_size=4096
#LD_PRELOAD=$SRC_ROOT/shim/libshim/libshim.so ${@}
