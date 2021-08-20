#!/bin/bash
# Run/kill parsec on three host machines. Parsec runs in long mode.
# Refer to bench/parsec/scripts/run.sh for long mode.
source scripts/global.sh
KILL=0

print_usage()
{
	echo "Run parsec (without -k option).
Usage: $0 [ -k ]
	-k : Kill parsec."
}

runParsec()
{
	(cd ${PROJ_DIR}/bench/parsec; ./scripts/run.sh -m long -b streamcluster &> /dev/null &)
	ssh $HOST_2 "(cd ${PROJ_DIR}/bench/parsec; ./scripts/run.sh -m long -b streamcluster &> /dev/null &)"
	ssh $HOST_3 "(cd ${PROJ_DIR}/bench/parsec; ./scripts/run.sh -m long -b streamcluster &> /dev/null &)"
}

killParsec()
{
	sudo pkill -9 streamcluster
	ssh $HOST_2 "sudo pkill -9 streamcluster &> /dev/null &"
	ssh $HOST_3 "sudo pkill -9 streamcluster &> /dev/null &"
}

while getopts "k?h" opt
do
case $opt in
	k)
		KILL=1
		;;
	h|?)
		print_usage
		exit 2
		;;
esac
done

if [ $KILL = 1 ]; then
	killParsec
else
	runParsec
fi
