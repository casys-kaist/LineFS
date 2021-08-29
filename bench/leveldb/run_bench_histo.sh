#! /usr/bin/sudo /bin/bash
SYSTEM="assise"
DIR="/mlfs"
RUN_PARSEC=0

print_usage() {
	echo "Usage: $0 [ -t linefs|assise ] [ -c ]
	-t : System type. <linefs|assise> (assise is default)
	-c : Run with cpu-intensive job, streamcluster."
}

while getopts "ct:?h" opt; do
	case $opt in
	c)
		RUN_PARSEC=1
		;;
	t)
		SYSTEM=$OPTARG
		if [ $SYSTEM != "assise" ] && [ $SYSTEM != "linefs" ]; then
			print_usage
			exit 2
		fi
		;;
	h | ?)
		print_usage
		exit 2
		;;
	esac
done

OUTPUT_DIR="outputs/${SYSTEM}"
RESULT_DIR="results/${SYSTEM}/results.txt"

run_parsec() {
	(
		cd ../../micro
		./scripts/parsec_ctl.sh -r
	) # parsec
	# (cd ../../micro; ./scripts/parsec_ctl.sh -n) # stress-ng
}

quit_parsec() {
	(
		cd ../../micro
		./scripts/parsec_ctl.sh -k
	) # parsec
	# (cd ../../micro; ./scripts/parsec_ctl.sh -q) # stress-ng
}

run_leveldb() {
	for WL in fillseq,readseq fillseq,readrandom fillseq,readhot fillseq fillrandom fillsync; do
		for ROUND in {1..1}; do
			# Run parsec.
			if [ "$RUN_PARSEC" == "cpu" ]; then
				run_parsec
				sleep 3
			fi

			echo "Run LevelDB: $WL"

			if [ "$WL" == "fillsync" ]; then
				# nice -n -20 ./run.sh ./db_bench.mlfs --db=$DIR --num=1000000000 --histogram=1 --value_size=1024 --benchmarks=${WL} 2>&1 | tee ./${OUTPUT_DIR}/${WL}_${ROUND}
				./run.sh ./db_bench.mlfs --db=$DIR --num=1000000000 --histogram=1 --value_size=1024 --benchmarks=${WL} 2>&1 | tee ./${OUTPUT_DIR}/${WL}_${ROUND}
			else
				# nice -n -20 ./run.sh ./db_bench.mlfs --db=$DIR --num=1000000 --histogram=1 --value_size=1024 --benchmarks=${WL} 2>&1 | tee ./${OUTPUT_DIR}/${WL}_${ROUND}
				./run.sh ./db_bench.mlfs --db=$DIR --num=1000000 --histogram=1 --value_size=1024 --benchmarks=${WL} 2>&1 | tee ./${OUTPUT_DIR}/${WL}_${ROUND}
			fi

			# Terminate parsec.
			if [ "$RUN_PARSEC" == "cpu" ]; then
				quit_parsec
			fi

			sleep 1
		done
	done
}

if [ "$RUN_PARSEC" == "cpu" ]; then
	echo "Run LevelDB with parsec."
	OUTPUT_DIR="${OUTPUT_DIR}_cpu"
fi

echo "------ Configurations -------"
echo "SYSTEM     : $SYSTEM"
echo "CPU_JOB    : $RUN_PARSEC"
echo "OUTPUT_DIR : $OUTPUT_DIR"
echo "RESULT_DIR : $RESULT_DIR"
echo "-----------------------------"

mkdir -p $OUTPUT_DIR
mkdir -p $RESULT_DIR

# Quit already running streamcluster instance.
quit_parsec

# Run NICFSes and kernel workers of LineFS or KernFSes of Assise.
if [ $SYSTEM == "assise" ]; then
	(
		cd ../../micro
		scripts/reset_kernfs.sh -t "hostonly"
	)
elif [ $SYSTEM == "linefs" ]; then
	(
		cd ../../micro
		scripts/reset_kernfs.sh -t "nic"
	)
fi

# Run leveldb bench.
run_leveldb

# Kill NICFSes and kernel workers of LineFS or KernFSes of Assise.
if [ $SYSTEM == "assise" ]; then
	(
		cd ../../micro
		scripts/reset_kernfs.sh -k -t "hostonly"
	)
elif [ $SYSTEM == "linefs" ]; then
	(
		cd ../../micro
		scripts/reset_kernfs.sh -k -t "nic"
	)
fi

echo "------ Configurations -------"
echo "SYSTEM     : $SYSTEM"
echo "CPU_JOB    : $RUN_PARSEC"
echo "OUTPUT_DIR : $OUTPUT_DIR"
echo "RESULT_DIR : $RESULT_DIR"
echo "-----------------------------"

# Parse results.
./parse_results.sh $OUTPUT_DIR | tee $RESULT_DIR
