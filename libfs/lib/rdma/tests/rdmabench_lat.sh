#! /bin/bash

DIR=/root/projects/assise/libfs/lib/rdma/tests
SERVERS=(sdp2)
CLIENT=(sdp1)
HOSTNAME=sdp1

SERVERIP="172.17.15.4"

FILESIZE=1G
OPS=1000
THREAD=1
LOGDIR="../results/rdma"
SRCMEM=dram
DSTMEM=nvm

BENCH="LAT"
IOSIZES="{128B,1K,4K,8K,16K,32K,64K,128K,256K,512K,1M}"
SGECOUNTS="{1,2,4,8}"
BATCHSIZES="{1,2,4,8}"
MODES="{SYNC,PSYNC}"

ALL_READ="$(eval echo "$IOSIZES':'$MODES':'$SGECOUNTS':'$BATCHSIZES")"

# functions below should be modifed according to tesbed
function execute_on_hosts () {
	local cmd=$1
	local period=$2
	local -n arr=$3

	for host in "${arr[@]}"; do
		if [ "$HOSTNAME" = "$host" ]; then
			echo $cmd
			eval $cmd
		else
			echo "ssh $host $cmd"
			ssh $host $cmd
		fi
		sleep "$period"
	done
}

function start_server() {
	SERVER_CMD='sudo '$DIR'/write_lat'
	TMUX_SESSION="custom"

	execute_on_hosts "tmux send-keys -t custom:0.1 C-c Enter \"$SERVER_CMD\" Enter" "1s" SERVERS
}

function kill_server() {
	execute_on_hosts 'pkill -9 write_lat' "0s" SERVERS
}

#echo "$ALL"
for INPUT in $ALL_READ; do

	IFS=":" read -r IOSIZE MODE SGECOUNT BATCHSIZE <<< "$INPUT"

	kill_server
	start_server

	#exit 0

	echo "-------- $BENCH - $THREAD thread - $IOSIZE $SGECOUNT $BATCHSIZE - $MODE"


	OPT='-e '$SGECOUNT' -b '$BATCHSIZE' '

	if [ "$MODE" = PSYNC ]; then
		OPT=$OPT'-p'
	fi

	SRCMEM=$(echo "$SRCMEM" | tr '[:lower:]' '[:upper:]')
	DSTMEM=$(echo "$DSTMEM" | tr '[:lower:]' '[:upper:]')

	OUT=$LOGDIR'/'$BENCH'_'$IOSIZE'_'$SGECOUNT'_'$SRCMEM'_'$DSTMEM'_'$BATCHSIZE'_'$THREAD'_'$MODE

	SRCMEM=$(echo "$SRCMEM" | tr '[:upper:]' '[:lower:]')
	DSTMEM=$(echo "$DSTMEM" | tr '[:upper:]' '[:lower:]')

	PARAMS="$SERVERIP $SRCMEM $DSTMEM $IOSIZE $OPS $OPT"
	echo "Run ./write_lat $PARAMS 2>&1 $OUT"
	for i in `seq 1 1`;
	do
	./write_lat $PARAMS &> $OUT
	done
done

exit 0


