#! /usr/bin/sudo /bin/bash
{
	SYSTEM="assise"
	DIR="/mlfs"
	RUN_PARSEC="solo"
	HISTOGRAM=""

	print_usage() {
		echo "Usage: $0 [ -t linefs|assise ] [ -c ] [ -s ]"
		echo -e "\t-t : System type. <linefs|assise> (assise is default)"
		echo -e "\t-c : Run with cpu-intensive job, streamcluster."
		echo -e "\t-s : Print histogram."
	}

	run_parsec() {
		# parsec
		(
			cd ../../micro || exit
			./scripts/parsec_ctl.sh -r
		)
	}

	kill_parsec() {
		# parsec
		(
			cd ../../micro || exit
			./scripts/parsec_ctl.sh -k
		)
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
					# nice -n -20 ./run.sh ./db_bench.mlfs --db=$DIR --num=1000000000 "$HISTOGRAM" --value_size=1024 --benchmarks=${WL} 2>&1 | tee "./${OUTPUT_DIR}/${WL}_${ROUND}"
					./run.sh ./db_bench.mlfs --db=$DIR --num=1000000000 "$HISTOGRAM" --value_size=1024 --benchmarks=${WL} 2>&1 | tee "./${OUTPUT_DIR}/${WL}_${ROUND}"
				else
					# nice -n -20 ./run.sh ./db_bench.mlfs --db=$DIR --num=1000000 "$HISTOGRAM" --value_size=1024 --benchmarks=${WL} 2>&1 | tee "./${OUTPUT_DIR}/${WL}_${ROUND}"
					./run.sh ./db_bench.mlfs --db=$DIR --num=1000000 "$HISTOGRAM" --value_size=1024 --benchmarks=${WL} 2>&1 | tee "./${OUTPUT_DIR}/${WL}_${ROUND}"
				fi

				# Terminate parsec.
				if [ "$RUN_PARSEC" == "cpu" ]; then
					kill_parsec
				fi

				sleep 1
			done
		done
	}

	print_config() {
		echo "------ Configurations -------"
		echo "SYSTEM     : $SYSTEM"
		echo "CPU_JOB    : $RUN_PARSEC"
		echo "OUTPUT_DIR : $OUTPUT_DIR"
		echo "RESULT_DIR : $RESULT_DIR"
		echo "-----------------------------"
	}

	run_kernfs() {
		# Run NICFSes and kernel workers of LineFS or KernFSes of Assise.
		if [ "$SYSTEM" == "assise" ]; then
			(
				cd ../../micro || exit
				scripts/reset_kernfs.sh -t "hostonly"
			)
		elif [ "$SYSTEM" == "linefs" ]; then
			(
				cd ../../micro || exit
				scripts/reset_kernfs.sh -t "nic"
			)
		fi
	}

	kill_kernfs() {
		# Kill NICFSes and kernel workers of LineFS or KernFSes of Assise.
		if [ "$SYSTEM" == "assise" ]; then
			(
				cd ../../micro || exit
				scripts/reset_kernfs.sh -k -t "hostonly"
			)
		elif [ "$SYSTEM" == "linefs" ]; then
			(
				cd ../../micro || exit
				scripts/reset_kernfs.sh -k -t "nic"
			)
		fi
	}

	# Main.
	if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then # script is executed directly.
		while getopts "cst:?h" opt; do
			case $opt in
			c)
				RUN_PARSEC="cpu"
				;;
			s)
				HISTOGRAM="--histogram=1"
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

		OUTPUT_DIR="outputs/${SYSTEM}/${RUN_PARSEC}"
		RESULT_DIR="results/${SYSTEM}/${RUN_PARSEC}"

		print_config

		mkdir -p $OUTPUT_DIR
		mkdir -p $RESULT_DIR

		# Quit already running streamcluster instance.
		kill_parsec

		run_kernfs

		# Run leveldb bench.
		run_leveldb

		kill_kernfs

		print_config

		# Parse results.
		./parse_results.sh "$OUTPUT_DIR" | tee "$RESULT_DIR"/results.txt
	fi

	exit
}
