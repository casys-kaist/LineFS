#!/bin/bash
{
	source ../../scripts/global.sh

	# DATE=$(date +"%y%m%d-%H%M%S")
	TPUT_BENCH_SH="run_iobench_tput_interfere.sh"
	LINEFS="linefs"
	ASSISE="assise"
	ASSISE_OPT="assise-opt"
	SOLO="solo"
	INTERFERE_OUT_DIR="/tmp/out_interfere"

	runStreamclusterSolo() {
		echo "******************** RUN INTERFERENCE EXP *************************"
		echo "Streamcluster solo run."
		echo "*******************************************************************"
		echo "Run streamcluster solo."
		sudo scripts/parsec_ctl.sh -s -l >$INTERFERE_OUT_DIR/$SOLO
	}

	killStreamcluster() {
		echo "Killing streamcluster."
		sudo scripts/parsec_ctl.sh -k &>/dev/null
	}

	runInterferenceExp() {
		sys="$1"
		out_file="$INTERFERE_OUT_DIR/${sys}"

		echo "******************** RUN INTERFERENCE EXP *************************"
		echo "System       	: $sys"
		echo "*******************************************************************"

		if [ "$sys" = "$LINEFS" ]; then
			sudo scripts/${TPUT_BENCH_SH} -t linefs | tee $out_file
		else
			sudo scripts/${TPUT_BENCH_SH} -t assise | tee $out_file
		fi
	}

	printInterferenceExpResult() {
		sys="$1"
		out_file="$INTERFERE_OUT_DIR/${sys}"

		echo "$sys"
		grep -A1 "Primary" $out_file

		grep "Throughput" $out_file
	}

	killIOBench() {
		echo "Kill IOBench."
		sudo pkill -9 iobench &>/dev/null
	}

	# Main.
	if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then # script is executed directly.
		# Kill all running processes.
		killIOBench
		killStreamcluster
		mkdir -p $INTERFERE_OUT_DIR

		## Streamcluster solo
		runStreamclusterSolo

		## Turn off Async Replication for LineFS and Assise.
		setAsyncReplicationOff

		## LineFS
		buildLineFS
		runInterferenceExp $LINEFS
		killIOBench # Kill suspended iobenchs

		## Assise
		buildAssise
		runInterferenceExp $ASSISE
		killIOBench # Kill suspended iobenchs

		## Assise-opt # XXX: Not-stable currently.
		# setAsyncReplicationOn
		# runInterferenceExp $ASSISE_OPT
		# killIOBench # Kill suspended iobenchs
		# setAsyncReplicationOff # restore Async Replication config.

		## Print results.
		echo "###################################################################"
		echo "#   Interference Experiment Result"
		echo "#     - Streamcluster execution time in seconds"
		echo "#     - Throughput in MB/s"
		echo "###################################################################"
		printInterferenceExpResult $SOLO
		printInterferenceExpResult $LINEFS
		printInterferenceExpResult $ASSISE
		# printInterferenceExpResult $ASSISE_OPT
	fi

	exit
}
