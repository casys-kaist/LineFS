#!/bin/bash

# Run it in the result directory of run_bench_histo.sh
# Names of sub-directories should be matched with "hostonly" and "nic-offload"

DIR="$1"

parse_latency() {
    dir=$1
    bench=$2
    filename=$3
    echo -n "$bench "
    (
        cd "$dir" || exit
        val=$(grep "$bench" "$filename" | cut -d ':' -f 2 | cut -d ';' -f 1 | sed 's/^[[:space:]]*//' | cut -d ' ' -f 1)
        if [ -z "$val" ];then
            echo ""
        else
            echo "$val"
        fi
    )
}

parse_tput() {
    dir=$1
    bench=$2
    filename=$3
    echo -n "$bench "
    (
        cd "$dir" || exit
        val=$(grep "$bench" "$filename" | cut -d ':' -f 2 | cut -d ';' -f 2 | sed 's/^[[:space:]]*//' | cut -d ' ' -f 1)
        if [ -z "$val" ];then
            echo ""
        else
            echo "$val"
        fi
    )
}

echo "Latency(micros/op)"
parse_latency "$DIR" readseq fillseq,readseq_1
parse_latency "$DIR" readrandom fillseq,readrandom_1
parse_latency "$DIR" readhot fillseq,readhot_1
parse_latency "$DIR" fillseq fillseq_1
parse_latency "$DIR" fillrandom fillrandom_1
parse_latency "$DIR" fillsync fillsync_1

echo " "
echo "Throughput(MB/s)"
parse_tput "$DIR" readseq fillseq,readseq_1
parse_tput "$DIR" fillseq fillseq_1
parse_tput "$DIR" fillrandom fillrandom_1
parse_tput "$DIR" fillsync fillsync_1
