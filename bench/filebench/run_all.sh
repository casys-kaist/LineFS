#!/bin/bash

printResults() {
	out_file=$1
	grep -r "IO Summary" "$out_file" | cut -d ' ' -f 6
}

{
	source ../../scripts/global.sh

	OUT_DIR="output"
	CONF_DIR="$OUT_DIR/configs"

	sudo rm -rf $OUT_DIR
	mkdir -p $OUT_DIR
	mkdir -p "$CONF_DIR"

	# Filebench is not patched to adopt memcopy batching.
	setMemcpyBatching 0

	## Dump configs.
	dumpConfigs "$CONF_DIR"

	buildLineFS
	# ./run_bench.sh -t nic    # Run with LineFS
	./run_bench.sh -t nic -c # Run with LineFS and a cpu-intensive job.

	buildAssise
	# ./run_bench.sh -t hostonly    # Run with Assise
	./run_bench.sh -t hostonly -c # Run with Assise and a cpu-intensive job.

	## Restore config.
	setMemcpyBatching 1

	# Print results
	echo "################# Filebench Results #################"
	echo "Varmail (ops/s)"
	echo -n "linefs "
	printResults output/nic/output.varmail_cpu
	echo -n "assise "
	printResults output/hostonly/output.varmail_cpu

	echo "Fileserver (ops/s)"
	echo -n "linefs "
	printResults output/nic/output.fileserver_cpu
	echo -n "assise "
	printResults output/hostonly/output.fileserver_cpu
	echo "#####################################################"

	exit
}
