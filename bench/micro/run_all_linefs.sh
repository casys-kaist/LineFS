#!/bin/bash
LOG_DIR="out_all"

printBenchInfo() {
	echo "**************************** RUN BENCH ****************************"
	echo "Benchmark      : $1"
	echo "System         : $2"
	echo "Co-running app : $3"
	echo "*******************************************************************"
}

### Latency microbenchmark
printLatencyMicrobenchResults() {
	out_dir="$LOG_DIR/lat/linefs"

	echo "##################################"
	echo "#   Latency microbench results   #"
	echo "##################################"

	echo "linefs solo:"
	# cat results/lat/linefs/solo/sw/output.txt
	for dir_name in results/lat/linefs/solo/*; do
		echo "$dir_name" | cut -d '/' -f 5
		cat ${dir_name}/output.txt
	done

	echo ""
	echo "linefs cpu-busy:"
	# cat results/lat/linefs/cpu/sw/output.txt
	for dir_name in results/lat/linefs/cpu/*; do
		echo "$dir_name" | cut -d '/' -f 5
		cat ${dir_name}/output.txt
	done

}

runLatencyMicrobench() {
	bench="run_iobench_lat.sh"
	out_dir="$LOG_DIR/lat/linefs"
	mkdir -p $out_dir

	printBenchInfo "Latency microbenchmark" "LineFS" "N/A"
	scripts/${bench} -t nic | tee ${out_dir}/solo.out # LineFS solo run.

	printBenchInfo "Latency microbenchmark" "LineFS" "Streamcluster"
	scripts/${bench} -t nic -c | tee ${out_dir}/cpu.out # LineFS runs with streamcluster.
}

# scripts/${bench} -y # Assise

### Throughput microbenchmark
printThroughputMicrobenchResults() {
	out_dir="$LOG_DIR/tput/linefs"

	echo "#####################################"
	echo "#   Throughput microbench results   #"
	echo "#####################################"
	echo "linefs solo:"
	grep Aggregated ${out_dir}/solo.out | cut -d ':' -f 2 | tee ${out_dir}/result_solo.txt

	echo ""
	echo "linefs cpu-busy:"
	grep Aggregated ${out_dir}/cpu.out | cut -d ':' -f 2 | tee ${out_dir}/result_cpu.txt
}

runThroughputMicrobench() {
	bench="run_iobench_tput.sh"
	out_dir="$LOG_DIR/tput/linefs"
	mkdir -p $out_dir

	printBenchInfo "Throughput microbenchmark" "LineFS" "N/A"
	scripts/${bench} -t nic | tee ${out_dir}/solo.out # LineFS solo run.

	# printBenchInfo "Throughput microbenchmark" "LineFS" "Streamcluster"
	# scripts/${bench} -t nic -c | tee ${out_dir}/cpu.out # LineFS runs with streamcluster.
}

# bench="run_iobench_tput_new.sh"
# sudo scripts/${bench} -t nic -a | tee nic_out_nolocal.a_24
# sudo scripts/${bench} -t nic -c | tee nic_out_nolocal.c_24
# sudo scripts/${bench} -t nic    | tee nic_out_nolocal.solo_24

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then # script is executed directly.
	sudo pkill -9 iobench  # Kill all iobench processes.
	# runLatencyMicrobench
	runThroughputMicrobench
	# printLatencyMicrobenchResults
	printThroughputMicrobenchResults
fi