#!/bin/bash
{
	source ../../scripts/global.sh

	# DATE=$(date +"%y%m%d-%H%M%S")
	TPUT_BENCH_SH="run_iobench_tput.sh"
	LINEFS="linefs"
	ASSISE="assise"
	ASSISE_OPT="assise-opt"
	SOLO="solo"
	INTERFERE_OUT_DIR="/tmp/out_interfere"


	runStreamclusterSolo() {
		echo "Run streamcluster solo."
		sudo scripts/parsec_ctl.sh -s -l > $INTERFERE_OUT_DIR/$SOLO
	}

	killStreamcluster() {
		echo "Killing streamcluster."
		sudo scripts/parsec_ctl.sh -k &> /dev/null
	}

	runInterferenceExp() {
		sys="$1"
		out_file="$INTERFERE_OUT_DIR/${sys}"

		echo "******************** RUN INTERFERENCE EXP *************************"
		echo "System       	: $sys"
		echo "*******************************************************************"

		# Run only 1 libfs process case.
		sed -i '/^NPROCS=/c\NPROCS="1"' scripts/$TPUT_BENCH_SH

		if [ "$sys" = "$LINEFS" ]; then
			sudo scripts/${TPUT_BENCH_SH} -t nic -a | tee $out_file
		else
			sudo scripts/${TPUT_BENCH_SH} -t hostonly -a | tee $out_file
		fi

		# Restore modified line.
		sed -i '/^NPROCS=/c\NPROCS="1 2 4 8"' scripts/$TPUT_BENCH_SH
	}

	printInterferenceExpResult () {
		sys="$1"
		out_file="$INTERFERE_OUT_DIR/${sys}"

		echo "$sys"
		grep -A1 "Primary" $out_file
	}

	# Main.
	if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then # script is executed directly.
		# Kill all running processes.
		sudo pkill -9 iobench
		killStreamcluster
		mkdir -p $INTERFERE_OUT_DIR

		## Streamcluster solo
		runStreamclusterSolo

		## Turn off Async Replication for LineFS and Assise.
		setAsyncReplicationOff

		## LineFS
		buildLineFS
		runInterferenceExp $LINEFS

		## Assise
		buildAssise
		runInterferenceExp $ASSISE
		
		## Assise-opt
		setAsyncReplicationOn
		runInterferenceExp $ASSISE_OPT
		setAsyncReplicationOff # restore Async Replication config.

		## Print results.
		printInterferenceExpResult $SOLO
		printInterferenceExpResult $LINEFS
		printInterferenceExpResult $ASSISE
		printInterferenceExpResult $ASSISE_OPT
	fi

	exit
}
