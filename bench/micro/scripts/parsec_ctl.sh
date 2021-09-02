#! /usr/bin/sudo /bin/bash
source ../../scripts/global.sh
source scripts/consts.sh
source scripts/utils.sh

# ROUNDS=5
# rm -f parsec_only.out
# for ROUND in $(seq 1 $ROUNDS); do
#     ${PARSEC_DIR}/run_parsec.sh >> parsec_only.out
#     sleep 3
# done

# grep QUERY parsec_only.out | cut -d ' ' -f 3

PARSEC_BENCH="streamcluster"
PARSEC_OUT_FILE_NAME="parsec.out"

runStressNg() {
	is_local=$1

	stressng_cmd="${PROJ_DIR}/scripts/run_stress_ng.sh &> /dev/null &"
	$SSH_HOST_2 $stressng_cmd
	$SSH_HOST_3 $stressng_cmd

	if [ $is_local = true ]; then
		${PROJ_DIR}/scripts/run_stress_ng.sh &>/dev/null &
	fi

	sleep 5
}

killStressNg() {
	pkill -9 stress-ng &>/dev/null &
	$SSH_HOST_2 "pkill -9 stress-ng &> /dev/null &"
	$SSH_HOST_3 "pkill -9 stress-ng &> /dev/null &"

	sleep 2
}

runParsec() {
	is_local=$1

	echo "runParsec"
	parsec_cmd_long="(cd ${PARSEC_DIR}; ./scripts/run.sh -b $PARSEC_BENCH -m long) &> /dev/null &"
	$SSH_HOST_2 $parsec_cmd_long
	$SSH_HOST_3 $parsec_cmd_long

	if [ $is_local = true ]; then
		(
			cd ${PARSEC_DIR} || exit
			./scripts/run.sh -b $PARSEC_BENCH -m long
		) &>/dev/null &
	fi

	sleep 5
}

measureParsec() {
	parsec_out_file_name=$1
	is_local=$2

	parsec_out_path_host3="${PARSEC_DIR}/outputs/$HOST_3/$parsec_out_file_name"
	parsec_out_path_host2="${PARSEC_DIR}/outputs/$HOST_2/$parsec_out_file_name"
	parsec_out_path_host1="${PARSEC_DIR}/outputs/$HOST_1/$parsec_out_file_name"

	# echo "Parsec output path:"
	# echo -e "\tPrimary  : $parsec_out_path_host1"
	# echo -e "\tReplica1 : $parsec_out_path_host2"
	# echo -e "\tReplica2 : $parsec_out_path_host3"

	# The order is intended to finish local parsec lastly.
	$SSH_HOST_3 "mkdir -p \"${PARSEC_DIR}/outputs/$HOST_3\""
	$SSH_HOST_2 "mkdir -p \"${PARSEC_DIR}/outputs/$HOST_2\""
	if [ $is_local = true ]; then
		mkdir -p "${PARSEC_DIR}/outputs/$HOST_1"
	fi

	parsec_cmd_short="(cd ${PARSEC_DIR}; ./scripts/run.sh -b $PARSEC_BENCH -m short)"
	# parsec_cmd_short="(cd ${PARSEC_DIR}; ./scripts/run.sh -b $PARSEC_BENCH -m short) &> $parsec_out_path_host3 &"
	# parsec_cmd_short="(cd ${PARSEC_DIR}; ./scripts/run.sh -b $PARSEC_BENCH -m short) &> $parsec_out_path &"

	$SSH_HOST_3 $parsec_cmd_short &>$parsec_out_path_host3 &
	$SSH_HOST_2 $parsec_cmd_short &>$parsec_out_path_host2 &

	if [ $is_local = true ]; then
		(
			cd ${PARSEC_DIR} || exit
			./scripts/run.sh -b $PARSEC_BENCH -m short
		) &>$parsec_out_path_host1 &
		parsec_pid=$!
	fi
}

printStremaclusterExeTime() {
	is_local=$1

	echo "---------------------------------"
	echo "Elapsed time (sec):"

	### get streamcluster result.
	echo -n "Primary  "
	if [ $is_local = true ]; then
		grep Elapsed $parsec_out_path_host1 | cut -d ':' -f 2
	fi

	# replicas
	echo -n "Replica  "
	$SSH_HOST_2 "grep Elapsed $parsec_out_path_host2 | cut -d ':' -f 2"
	# echo -n "Replica1 "
	# $SSH_HOST_2 "grep Elapsed $parsec_out_path_host2 | cut -d ':' -f 2"
	# echo -n "Replica2 "
	# $SSH_HOST_3 "grep Elapsed $parsec_out_path_host3 | cut -d ':' -f 2"

	echo "---------------------------------"
}

measureParsecAndPrint() {
	parsec_out_file_name=$1
	is_local=$2

	measureParsec $parsec_out_file_name $is_local

	echo "Waiting parsec finishes."
	if [ $is_local = true ]; then
		wait $parsec_pid
	fi
	# sleepAndCountdown 80 # Enough time for Parsec run.
	echo "Parsec finished."

	sleep 10
	printStremaclusterExeTime $is_local
}

killParsec() {
	pkill -9 $PARSEC_BENCH &>/dev/null &
	$SSH_HOST_2 "pkill -9 $PARSEC_BENCH &> /dev/null &"
	$SSH_HOST_3 "pkill -9 $PARSEC_BENCH &> /dev/null &"
	sleep 1
}

print_usage() {
	echo "Usage: $0 [-l] <-r|-k|-s|-n|-q>
    -l : run primary machine too
    -r : run parsec($PARSEC_BENCH) on all replicas
    -s : run parsec($PARSEC_BENCH) as short mode to measure execution time
    -k : kill all parsec($PARSEC_BENCH) running on replicas
    -n : run stress-ng
    -q : kill all stress-ng running on replicas
    "
}

if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then # script is sourced.
	unset -f print_usage
else # script is executed directly.
	# echo "PARSEC_DIR: $PARSEC_DIR"
	# echo "PARSEC_BENCH: $PARSEC_BENCH"
	# echo "is_local: $is_local"
	# echo "HOST_1: $HOST_1"
	# echo "HOST_2: $HOST_2"
	# echo "HOST_3: $HOST_3"
	# echo "SSH_HOST_1: $SSH_HOST_1"
	# echo "SSH_HOST_2: $SSH_HOST_2"
	# echo "SSH_HOST_3: $SSH_HOST_3"

	RUN_PRIMARY=false
	while getopts "lrksnpq?h" opt; do
		case $opt in
		l)
			RUN_PRIMARY=true
			;;
		n)
			echo "Running stress-ng on replicas."
			RUN_MODE=runStressNg
			;;
		q)
			echo "Killing all stress-ng running on replicas."
			RUN_MODE=killStressNg
			;;
		r)
			echo "Running streamcluster on replicas."
			RUN_MODE=runParsec
			;;
		k)
			echo "Killing all streamcluster running on replicas."
			RUN_MODE=killParsec
			;;
		s)
			echo "Running streamcluster on replicas (measuring execution time)."
			RUN_MODE="measureParsecAndPrint $PARSEC_OUT_FILE_NAME true"
			;;
		p)
			echo "Print Streamcluster execution time."
			RUN_MODE="printStremaclusterExeTime true"
			parsec_out_path_host2="${PARSEC_DIR}/outputs/$HOST_2/$PARSEC_OUT_FILE_NAME"
			parsec_out_path_host1="${PARSEC_DIR}/outputs/$HOST_1/$PARSEC_OUT_FILE_NAME"
			;;
		h | ?)
			print_usage
			exit 2
			;;
		esac
	done

	$RUN_MODE $RUN_PRIMARY
	echo "Done."
	exit
fi
