#! /usr/bin/sudo /bin/bash
source ../../scripts/global.sh
source scripts/consts.sh
source scripts/utils.sh

# IS_LOCAL=true
IS_LOCAL=false

# ROUNDS=5
# rm -f parsec_only.out
# for ROUND in $(seq 1 $ROUNDS); do
#     ${PARSEC_DIR}/run_parsec.sh >> parsec_only.out
#     sleep 3
# done

# grep QUERY parsec_only.out | cut -d ' ' -f 3

PARSEC_BENCH="streamcluster"

runStressNg() {
	stressng_cmd="${PROJ_DIR}/scripts/run_stress_ng.sh &> /dev/null &"
	$SSH_HOST_2 $stressng_cmd
	$SSH_HOST_3 $stressng_cmd

	if [ $IS_LOCAL = true ]; then
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
	echo "runParsec"
	parsec_cmd_long="(cd ${PARSEC_DIR}; ./scripts/run.sh -b $PARSEC_BENCH -m long) &> /dev/null &"
	$SSH_HOST_2 $parsec_cmd_long
	$SSH_HOST_3 $parsec_cmd_long

	if [ $IS_LOCAL = true ]; then
		(
			cd ${PARSEC_DIR} || exit
			./scripts/run.sh -b $PARSEC_BENCH -m long
		) &>/dev/null &
	fi

	sleep 5
}

measureParsec() {
	echo "measureParsec"

	parsec_out_file_name=$1
	parsec_out_path="${PARSEC_DIR}/outputs/${DATE}/$parsec_out_file_name"
	echo "Parsec output path: $parsec_out_path"

	parsec_cmd_short="${PARSEC_DIR}/scripts//run.sh -b $PARSEC_BENCH -m short &> $parsec_out_path &"

	# The order is intended to finish local parsec lastly.
	$SSH_HOST_3 "mkdir -p \"${PARSEC_DIR}/outputs/${DATE}\""
	$SSH_HOST_2 "mkdir -p \"${PARSEC_DIR}/outputs/${DATE}\""

	if [ $IS_LOCAL = true ]; then
		mkdir -p "${PARSEC_DIR}/outputs/${DATE}"
	fi

	$SSH_HOST_3 $parsec_cmd_short
	$SSH_HOST_2 $parsec_cmd_short
	if [ $IS_LOCAL = true ]; then
		${PARSEC_DIR}/scripts/run.sh -b $PARSEC_BENCH -m short &>$parsec_out_path &
		parsec_pid=$!
	fi

	# echo "Waiting parsec finishes."
	# if [ $IS_LOCAL = true ]; then
	#         wait $parsec_pid
	# fi

	# sleepAndCountdown 80 # Enough time for Parsec run.
	# echo "Parsec finished."
}

onParsecFinished() {
	parsec_out_file_name=$1

	echo "onParsecFinished."

	### get streamcluster result.
	if [ $IS_LOCAL = true ]; then
		grep Elapsed $parsec_out_path | cut -d ':' -f 2
	fi
	# replicas
	echo "$HOST_2 result:"
	$SSH_HOST_2 "grep Elapsed $parsec_out_path | cut -d ':' -f 2"
	echo "$HOST_3 result:"
	$SSH_HOST_3 "grep Elapsed $parsec_out_path | cut -d ':' -f 2"
}

measureParsecAndPrint() {
	echo "measureParsecAndPrint"

	measureParsec $1

	echo "Waiting parsec finishes."
	# if [ $IS_LOCAL = true ]; then
	#         wait $parsec_pid
	# fi
	sleepAndCountdown 80 # Enough time for Parsec run.
	echo "Parsec finished."

	sleep 1
	onParsecFinished $1
}

killParsec() {
	pkill -9 $PARSEC_BENCH &>/dev/null &
	$SSH_HOST_2 "pkill -9 $PARSEC_BENCH &> /dev/null &"
	$SSH_HOST_3 "pkill -9 $PARSEC_BENCH &> /dev/null &"
	sleep 1
}

print_usage() {
	echo "Usage: $0 <-r|-k>
    -r : run parsec($PARSEC_BENCH) on all replicas
    -s : run parsec($PARSEC_BENCH) as short mode to measure execution time
    -k : kill all parsec($PARSEC_BENCH) running on replicas
    -n : run stress-ng
    -q : kill all stress-ng running on replicas"
}

if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then # script is sourced.
	unset -f print_usage
else # script is executed directly.
	# echo "PARSEC_DIR: $PARSEC_DIR"
	# echo "PARSEC_BENCH: $PARSEC_BENCH"
	# echo "IS_LOCAL: $IS_LOCAL"
	# echo "HOST_1: $HOST_1"
	# echo "HOST_2: $HOST_2"
	# echo "HOST_3: $HOST_3"
	# echo "SSH_HOST_1: $SSH_HOST_1"
	# echo "SSH_HOST_2: $SSH_HOST_2"
	# echo "SSH_HOST_3: $SSH_HOST_3"

	while getopts "rksnq?h" opt; do
		case $opt in
		n)
			echo "Running stress-ng on replicas."
			runStressNg
			echo "Done."
			exit
			;;
		q)
			echo "Killing all stress-ng running on replicas."
			killStressNg
			echo "Done."
			exit
			;;
		r)
			echo "Running parsec on replicas."
			runParsec
			echo "Done."
			exit
			;;
		k)
			echo "Killing all parsec running on replicas."
			killParsec
			echo "Done."
			exit
			;;
		s)
			echo "Running parsec on replicas (measuring execution time)."
			measureParsecAndPrint parsec.out
			echo "Done."
			exit
			;;
		h | ?)
			print_usage
			exit 2
			;;
		esac
	done
fi
