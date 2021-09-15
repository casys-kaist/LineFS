#!/bin/bash
{
	source ../../../scripts/global.sh

	setConfig() {
		setMemcpyBatching 0
	}

	restoreConfig() {
		setMemcpyBatching 1
	}

	setConfig
	buildLineFS
	./run_bench_histo.sh -t linefs -c # Run LineFS with streamcluster.
	restoreConfig

	buildAssise
	./run_bench_histo.sh -t assise -c # Run Assise with streamcluster.

	echo "############ LevelDB results ############"
	echo "linefs running with streamcluster"
	grep -A6 "Latency" "results/linefs/cpu/results.txt"
	echo ""
	echo "assise running with streamcluster"
	grep -A6 "Latency" "results/assise/cpu/results.txt"
	echo "#########################################"

	exit
}
