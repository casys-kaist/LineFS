#!/bin/bash
source ../../scripts/global.sh

USAGE="Usage: $0 -m <long|short> -b <bench_name>"
PARSEC_PATH="${PROJ_DIR}/bench/parsec/parsec-3.0"

if [ $# -eq 0 ] ; then
    echo $USAGE
fi

while getopts "m:b:?h" opt
do
    case $opt in
	b)
	    BENCH=$OPTARG
	    ;;
	m)
	    MODE=$OPTARG
	    ;;
	h|?)
	    echo $USAGE
	    exit 2
	    ;;
    esac
done

case $BENCH in
    streamcluster)
	if [ $MODE = "short" ]
	then
	    # (TIMEFORMAT='Elapsed:%3R'; time $PINNING ${PARSEC_PATH}/pkgs/kernels/streamcluster/inst/amd64-linux.gcc/bin/streamcluster 10 20 128 1000000 200000 5000 none output.txt 16)
	    (TIMEFORMAT='Elapsed:%3R'; time $PINNING ${PARSEC_PATH}/pkgs/kernels/streamcluster/inst/amd64-linux.gcc/bin/streamcluster 10 20 128 600000 200000 5000 none /tmp/output.txt 16)
	else
	    (TIMEFORMAT='Elapsed:%3R'; time $PINNING ${PARSEC_PATH}/pkgs/kernels/streamcluster/inst/amd64-linux.gcc/bin/streamcluster 10 20 128 24000000 200000 5000 none /tmp/output.txt 16)
	fi
	;;
esac
