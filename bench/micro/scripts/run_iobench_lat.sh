#! /usr/bin/sudo /bin/bash
source scripts/reset_kernfs.sh
source scripts/parsec_ctl.sh
source scripts/run_hyperloop.sh

# echo "Host project dir path  : $PROJ_DIR"
# echo "NIC project dir path   : $NIC_PROJ_DIR"
# echo "Parsec dir path (host) : $PARSEC_DIR"

# OPS="sw sr rr rw"
OPS="sw"
# IO_SIZES="4K 16K 64K 1M"
IO_SIZES="16K"
FILE_SIZE="1G"
# JOB_NAME="$DATE"
JOB_NAME=""
TYPE="hostonly"
ACTION="solo"
HYPERLOOP=""
HYPERLOOP_TRACE_FILE_PREFIX="${PROJ_DIR}/libfs/lib/hyperloop/trace/micro/latency/sw.1g."

print_usage() {
	echo "Usage: $0 [ -t nic|hostonly ] [ -c ]
    -t : system type <nic|hostonly> (hostonly is default)
    -c : run with cpu-intensive job
    -y : run with hyperloop"
}

while getopts "t:cy?h" opt; do
	case $opt in
	c)
		ACTION="cpu"
		echo "Run with CPU-intensive job, parsec."
		;;
	t)
		TYPE=$OPTARG
		if [ $TYPE = "nic" ]; then
			echo "Run benchmark on LineFS."
			DIR_PATH="./results/lat/linefs/${JOB_NAME}"
		elif [ $TYPE = "hostonly" ]; then
			echo "Run benchmark on Assise."
			DIR_PATH="./results/lat/assise/${JOB_NAME}"
		else
			print_usage
			exit 2
		fi
		;;
	y)
		HYPERLOOP="true"
		echo "Running bench as a Hyperloop mode."
		;;
	h | ?)
		print_usage
		exit 2
		;;
	esac
done

if [ "$ACTION" = "cpu" ]; then
	DIR_PATH=${DIR_PATH}/cpu
else
	DIR_PATH=${DIR_PATH}/solo
fi

# Kill processes already running.
# if [ "$ACTION" = "cpu" ]; then
echo "Kill parsec."
killParsec
# killStressNg
# fi

# if [ "$HYPERLOOP" = "true" ]; then
killHyperloopServers
# fi
# kernfs is reset by reset_kernfs().

# Main loop.
for OP in $OPS; do
	FILE_PATH="${DIR_PATH}/${OP}"
	mkdir -p $FILE_PATH

	for IO_SIZE in $IO_SIZES; do
		RUN_CMD="sudo $PINNING ./run.sh ./iobench_lat -s $OP $FILE_SIZE $IO_SIZE 1"
		FILE_NAME="${FILE_PATH}/${OP}_${FILE_SIZE}_${IO_SIZE}_${JOB_NAME}"

		if [ "$HYPERLOOP" = "true" ]; then
			trace_path="${HYPERLOOP_TRACE_FILE_PREFIX}${IO_SIZE}"
			replaceHyperloopTracePath $trace_path
			runHyperloopServers
		fi

		reset_kernfs $TYPE

		if [ "$ACTION" = "cpu" ]; then
			echo "Run parsec."
			runParsec
			# runStressNg
		fi

		echo "Run Latency microbenchamrk with command: $RUN_CMD"
		echo "$RUN_CMD" >$FILE_NAME
		/usr/bin/time -f "Benchmark finished.\nElapsed time: %e seconds" $RUN_CMD >>$FILE_NAME
		# time $RUN_CMD | tee $FILE_NAME
		echo Done.
		sleep 10

		if [ "$ACTION" = "cpu" ]; then
			echo "Kill parsec."
			killParsec
			# killStressNg
		fi

		if [ "$HYPERLOOP" = "true" ]; then
			killHyperloopServers
		fi
	done

	# parsing the result files.
	# echo "python scripts/parsing_lat.py $FILE_PATH"
	python scripts/parsing_lat.py $FILE_PATH
done

# print to the console.
echo "--- Results ------------------------------"
for DIR_NAME in ${DIR_PATH}/*; do
	echo "Output Directory: $DIR_NAME"
	cat ${DIR_NAME}/output.txt
done
echo "------------------------------------------"

# shutdown kernfs.
kill_kernfs $TYPE

# shutdown hyperloop servers.
if [ "$HYPERLOOP" = "true" ]; then
	killHyperloopServers
fi
