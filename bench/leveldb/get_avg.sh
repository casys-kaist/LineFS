#!/bin/bash

# Run it in the result directory of run_bench_histo.sh
# Names of sub-directories should be matched with "hostonly" and "nic-offload"

HOSTONLY="hostonly"
NIC_OFFLOAD="nic-offload"

parse_latency() {
    dir=$1
    bench=$2
    filename=$3
    echo -n "$bench $dir "
    (cd $dir; grep $bench $filename | cut -d ':' -f 2 | cut -d ';' -f 1 | sed 's/^[[:space:]]*//' | cut -d ' ' -f 1)
}

parse_tput() {
    dir=$1
    bench=$2
    filename=$3
    echo -n "$bench $dir "
    (cd $dir; grep $bench $filename | cut -d ':' -f 2 | cut -d ';' -f 2 | sed 's/^[[:space:]]*//' | cut -d ' ' -f 1)
}

echo "Latency(micros/op)"
parse_latency $HOSTONLY readseq fillseq,readseq_1
parse_latency $NIC_OFFLOAD readseq fillseq,readseq_1
parse_latency $HOSTONLY readrandom fillseq,readrandom_1
parse_latency $NIC_OFFLOAD readrandom fillseq,readrandom_1
parse_latency $HOSTONLY readhot fillseq,readhot_1
parse_latency $NIC_OFFLOAD readhot fillseq,readhot_1
parse_latency $HOSTONLY fillseq fillseq_1
parse_latency $NIC_OFFLOAD fillseq fillseq_1
parse_latency $HOSTONLY fillrandom fillrandom_1
parse_latency $NIC_OFFLOAD fillrandom fillrandom_1
parse_latency $HOSTONLY fillsync fillsync_1
parse_latency $NIC_OFFLOAD fillsync fillsync_1

echo " "
echo "Throughput(MB/s)"
parse_tput $HOSTONLY readseq fillseq,readseq_1
parse_tput $NIC_OFFLOAD readseq fillseq,readseq_1
# parse_tput $HOSTONLY readrandom fillseq,readrandom_1
# parse_tput $NIC_OFFLOAD readrandom fillseq,readrandom_1
# parse_tput $HOSTONLY readhot fillseq,readhot_1
# parse_tput $NIC_OFFLOAD readhot fillseq,readhot_1
parse_tput $HOSTONLY fillseq fillseq_1
parse_tput $NIC_OFFLOAD fillseq fillseq_1
parse_tput $HOSTONLY fillrandom fillrandom_1
parse_tput $NIC_OFFLOAD fillrandom fillrandom_1
parse_tput $HOSTONLY fillsync fillsync_1
parse_tput $NIC_OFFLOAD fillsync fillsync_1

