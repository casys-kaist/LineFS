#!/bin/bash
{
	source ../../scripts/global.sh

	# DATE=$(date +"%y%m%d-%H%M%S")
	# LOG_DIR="out_all/${DATE}"
	LOG_DIR="out_all"
	LAT_BENCH_SH="run_iobench_lat.sh"
	TPUT_BENCH_SH="run_iobench_tput.sh"
	LINEFS="linefs"
	ASSISE="assise"

	runLatencyMicrobench() {
		sys="$1"
		cpu_job="$2"
		out_dir="${LOG_DIR}/lat/${sys}"

		if [ "$cpu_job" = "streamcluster" ]; then
			CPU_ARG="-c"
		else
			CPU_ARG=""
			cpu_job="solo"
		fi

		mkdir -p $out_dir

		printBenchInfo "Latency microbenchmark" "$sys" "$cpu_job"

		if [ "$sys" = "$LINEFS" ]; then
			sudo scripts/${LAT_BENCH_SH} -t nic "$CPU_ARG" | tee "${out_dir}/result_${cpu_job}.out"
		else
			sudo scripts/${LAT_BENCH_SH} -t hostonly "$CPU_ARG" | tee "${out_dir}/result_${cpu_job}.out"
		fi
	}

	runThroughputMicrobench() {
		sys="$1"
		cpu_job="$2"
		out_dir="$LOG_DIR/tput/${sys}"

		if [ "$cpu_job" = "streamcluster" ]; then
			CPU_ARG="-c"
		else
			CPU_ARG=""
			cpu_job="solo"
		fi

		mkdir -p $out_dir

		printBenchInfo "Throughput microbenchmark" "$sys" "$cpu_job"
		if [ "$sys" = "$LINEFS" ]; then
			sudo scripts/${TPUT_BENCH_SH} -t nic "$CPU_ARG" | tee "${out_dir}/result_${cpu_job}.out"
		else
			sudo scripts/${TPUT_BENCH_SH} -t hostonly "$CPU_ARG" | tee "${out_dir}/result_${cpu_job}.out"
		fi
	}

	printBenchInfo() {
		echo "**************************** RUN BENCH ****************************"
		echo "Benchmark     	: $1"
		echo "System       	: $2"
		echo "Co-running app 	: $3"
		echo "*******************************************************************"
	}

	### Latency microbenchmark
	printLatencyMicrobenchResults() {
		sys="$1"
		out_dir="${LOG_DIR}/lat/${sys}"

		echo ""
		echo "############################################"
		if [ "$sys" = "$LINEFS" ]; then
			echo -e "#   Latency microbench results of \e[1;36m$sys\e[0m"	# red
		else
			echo -e "#   Latency microbench results of \e[1;35m$sys\e[0m"	# red
		fi
		echo "############################################"

		echo "$sys solo:"
		# cat results/lat/linefs/solo/sw/output.txt
		for dir_name in results/lat/${sys}/solo/*; do
			echo "$dir_name" | cut -d '/' -f 5
			cat "${dir_name}/output.txt"
		done

		echo "$sys cpu-busy:"
		# cat results/lat/linefs/cpu/sw/output.txt
		for dir_name in results/lat/$sys/cpu/*; do
			echo "$dir_name" | cut -d '/' -f 5
			cat "${dir_name}/output.txt"
		done

	}

	### Throughput microbenchmark
	printThroughputMicrobenchResults() {
		sys="$1"
		out_dir="${LOG_DIR}/tput/${sys}"

		echo ""
		echo "###############################################"
		if [ "$sys" = "$LINEFS" ]; then
			echo -e "#   Throughput microbench results of \e[1;36m$sys\e[0m"	# red
		else
			echo -e "#   Throughput microbench results of \e[1;35m$sys\e[0m"	# red
		fi
		echo "###############################################"
		echo "$sys solo:"
		grep Aggregated ${out_dir}/result_solo.out | cut -d ':' -f 2 | tee ${out_dir}/result_solo.txt

		echo ""
		echo "$sys cpu-busy:"
		grep Aggregated ${out_dir}/result_streamcluster.out | cut -d ':' -f 2 | tee ${out_dir}/result_cpu.txt
	}

	# Main.
	if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then # script is executed directly.
		## Kill all iobench processes.
		sudo pkill -9 iobench

		## Build linefs.
		buildLineFS

		## Run LineFS.
		runLatencyMicrobench $LINEFS
		runLatencyMicrobench $LINEFS streamcluster
		runThroughputMicrobench $LINEFS
		runThroughputMicrobench $LINEFS streamcluster

		## Build assise.
		buildAssise

		## Run Assise.
		runLatencyMicrobench $ASSISE
		runLatencyMicrobench $ASSISE streamcluster
		runThroughputMicrobench $ASSISE
		runThroughputMicrobench $ASSISE streamcluster

		## Print results.
		printLatencyMicrobenchResults $LINEFS
		printLatencyMicrobenchResults $ASSISE
		printThroughputMicrobenchResults $LINEFS
		printThroughputMicrobenchResults $ASSISE
	fi

	exit
}
