#!/bin/bash
NPROCS="1"
OUTPUT_DIR="./outputs"
RESULT_DIR="./results"
OPS="sw"
# TOTAL_FILE_SIZE="4096"
TOTAL_FILE_SIZE="12280"
FSIZE="1024" # in MB.
IOSIZES="4K"
ROUNDS=1
THREADS=1

exe_proc(){
    # Arguments
    id=$1
    op=$2
    iosize=$3
    proc_num=$4
    round=$5

    out_file_path=${OUTPUT_DIR}/${op}.${iosize}.p${proc_num}.r${round}.id${id}

    # sudo FILE_ID=$id numactl -N0 -m0 ./run.sh ./iobench -s -w $op ${FSIZE}M $iosize 1 > $out_file_path &

    # Use private directories. If Lease is disabled, we need to run libfs/tests/mkdir_test first.
    # sudo FILE_ID=$id numactl -N0 -m0 ./run.sh ./iobench -d /mlfs/$id -s -w $op ${FSIZE}M $iosize 1 > $out_file_path &
    echo "sudo FILE_ID=$id numactl -N0 -m0 ./run.sh ./iobench -d /mlfs/$id -s -w $op $(($TOTAL_FILE_SIZE/$proc_num))M $iosize $THREADS > $out_file_path &"
    sudo FILE_ID=$id numactl -N0 -m0 ./run.sh ./iobench -d /mlfs/$id -s -w $op $(($TOTAL_FILE_SIZE/$proc_num))M $iosize $THREADS > $out_file_path &
    pids[${id}]=$!
}

exe_all_procs(){
    op=$1
    iosize=$2
    proc_num=$3
    round=$4

    echo "------------- Config ---------------"
    echo " operation = $op"
    # echo " total_file_size(MB) = $((${FSIZE}*${proc_num}))"
    echo " total_file_size(MB) = $TOTAL_FILE_SIZE"
    echo " io_size = $iosize"
    echo " num_of_processes = $proc_num"
    echo " round = $round"
    echo "------------------------------------"

    # run processes and store pids in array.
    echo "Start $proc_num processes."
    for i in $(seq 1 $proc_num); do
	exe_proc $i $op $iosize $proc_num $round
	pids[${i}]=$!
    done

    echo "Waiting for the processes to be ready."
    # Adjust waiting time.
    sleep $((8 + ${proc_num}*7))

    # Send signal to all processes.
    echo "Send signal to start iobench."
    ./iobench -p

    sleep 30

    echo "Send signal to shutdown Assise."
    ./iobench -p

    # wait for all pids.
    echo "Waiting for the processes to exit"
    for pid in ${pids[*]}; do
	wait $pid
    done

    # grep results.
    result_file="${RESULT_DIR}/${op}.${iosize}.p${proc_num}.r${round}"
    echo "result_file: $result_file"
    echo "$round" > $result_file
    grep -h "Throughput" $OUTPUT_DIR/${op}.${iosize}.p${proc_num}.r${round}.id* | tee -a $result_file
    echo
}

# reset dirs.
[ -d $OUTPUT_DIR ] && rm -rf $OUTPUT_DIR/*
mkdir -p $OUTPUT_DIR

[ -d $RESULT_DIR ] && rm -rf $RESULT_DIR/*
mkdir -p $RESULT_DIR

# execute.
for OP in $OPS; do
    for IOSIZE in $IOSIZES; do
	for PROCS in $NPROCS; do
	    for ROUND in $(seq 1 $ROUNDS); do
		exe_all_procs $OP $IOSIZE $PROCS $ROUND
		sleep 2
	    done
	done
    done
done

# Print results.
for OP in $OPS; do
    for IOSIZE in $IOSIZES; do
	for PROCS in $NPROCS; do
	    for ROUND in $(seq 1 $ROUNDS); do
		echo "${OP}_${IOSIZE}_${PROCS}_${ROUND}"
		grep -h Throughput ${RESULT_DIR}/${OP}.${IOSIZE}.p${PROCS}.r${ROUND} | cut -d ' ' -f 2
		echo -n "Sum: "
		grep -h Throughput ${RESULT_DIR}/${OP}.${IOSIZE}.p${PROCS}.r${ROUND} | cut -d ' ' -f 2 | awk '{ total += $1} END { printf("%.3f\n", total) }'
	    done
	done
    done
done
exit
