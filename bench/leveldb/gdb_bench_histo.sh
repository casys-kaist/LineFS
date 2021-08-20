#!/bin/bash
OUTPUT="leveldb_results_today"
DIR="/mlfs"
mkdir -p $OUTPUT
for WL in fillseq,readseq fillseq,readrandom fillseq,readhot fillseq fillrandom fillsync
do
    for ROUND in {1..1}
    do
	if [ "$WL" == "fillsync" ]; then
	    sudo gdb -ex run --args ./db_bench.mlfs --db=$DIR --num=1000000000 --histogram=1 --value_size=1024 --benchmarks=${WL} 2>&1 | tee ./${OUTPUT}/${WL}_${ROUND}
	else
	    sudo gdb -ex run --args ./db_bench.mlfs --db=$DIR --num=1000000 --histogram=1 --value_size=1024 --benchmarks=${WL} 2>&1 | tee ./${OUTPUT}/${WL}_${ROUND}
	fi
    done
done
