#! /usr/bin/sudo /bin/bash
source scripts/reset_kernfs.sh
source scripts/parsec_ctl.sh
source scripts/run_hyperloop.sh
source scripts/utils.sh

# echo "Host project dir path  : $PROJ_DIR"
# echo "NIC project dir path   : $NIC_PROJ_DIR"
# echo "Parsec dir path (host) : $PARSEC_DIR"

NPROCS="1 2 4 8"
# OUTPUT_DIR="./outputs/${DATE}"
# RESULT_DIR="./results/${DATE}"
OUTPUT_DIR="./outputs/tput"
RESULT_DIR="./results/tput"
OPS="sw"
# TOTAL_FILE_SIZE="8192" # 8G
TOTAL_FILE_SIZE="12288" # 12G
# TOTAL_FILE_SIZE="16384" # 16G
# TOTAL_FILE_SIZE="24576" # 24G
# TOTAL_FILE_SIZE="46080" # 45G
# TOTAL_FILE_SIZE="36864" # 36G
# TOTAL_FILE_SIZE="49152" # 48G
# FSIZE="1024" # in MB.
# IOSIZES="1M 64K 16K 4K 1K"
IOSIZES="16K"
ROUNDS=3
TYPE="hostonly"
ACTION="run"
CPU="solo"
HYPERLOOP=""
HYPERLOOP_TRACE_FILE_PREFIX="${PROJ_DIR}/libfs/lib/hyperloop/trace/micro/tput/mp"

exe_proc() {
    # Arguments
    id=$1
    op=$2
    iosize=$3
    proc_num=$4
    round=$5
    cpu=$6

    out_file_path=${OUTPUT_DIR}/${op}.${iosize}.p${proc_num}.r${round}.id${id}

    if [ "$cpu" = "measure-exetime" ]; then
        run_bin="iobench.infinite"
    else
        run_bin="iobench"
    fi

    ### Use private directories. If Lease is disabled, we need to run libfs/tests/mkdir_test first.
    #
    ## Normal priority
    # echo "sudo FILE_ID=$id $PINNING ./run.sh ./${run_bin} -d /mlfs/$id -s -w $op $(($TOTAL_FILE_SIZE/$proc_num))M $iosize 1 > $out_file_path"
    # sudo FILE_ID=$id $PINNING ./run.sh ./${run_bin} -d /mlfs/$id -s -w $op $(($TOTAL_FILE_SIZE/$proc_num))M $iosize 1 > $out_file_path &
    #
    ## Higher priority.
    echo "sudo FILE_ID=$id nice -n -20 $PINNING ./run.sh ./${run_bin} -d /mlfs/$id -s -w $op $(($TOTAL_FILE_SIZE / $proc_num))M $iosize 1 > $out_file_path"
    sudo FILE_ID=$id nice -n -20 $PINNING ./run.sh ./${run_bin} -d /mlfs/$id -s -w $op $(($TOTAL_FILE_SIZE / $proc_num))M $iosize 1 >$out_file_path &
}

exe_all_procs() {
    op=$1
    iosize=$2
    proc_num=$3
    round=$4
    cpu=$5

    echo "------------- Config ---------------"
    echo " Operation           : $op"
    echo " Total_file_size(MB) : $TOTAL_FILE_SIZE"
    echo " IO_size             : $iosize"
    echo " Num_of_processes    : $proc_num"
    echo " Round               : $round/$ROUNDS"
    echo " Run_mode            : $cpu"
    echo "------------------------------------"

    # Reset shm value.
    ../../utils/shm_tool/shm -p /iobench_shm -w 0 &> /dev/null
    echo "shm_val=$(../../utils/shm_tool/shm -p /iobench_shm -r)"

    # run processes and store pids in array.
    echo "Start $proc_num processes."
    for i in $(seq 1 $proc_num); do
        exe_proc $i $op $iosize $proc_num $round $cpu
        pids[${i}]=$!
    done

    echo "Waiting for the processes to be ready."
    shm_val=0
    while [ $shm_val -ne "$proc_num" ]; do
        shm_val=$(../../utils/shm_tool/shm -p /iobench_shm -r)
        echo -e "\tready/total: $shm_val/$proc_num"
        sleepAndCountdown 5
    done
    echo ""

    echo "All processes are ready."
    sleep 1

    if [ "$cpu" = "cpu-busy" ]; then
        echo "Run parsec."
        runParsec
    fi

    # Get start time.
    iobench_start_time=$(date +%s.%N)

    # Send signal to all processes.
    echo "Send signal to start iobench."
    # ./iobench -p
    ../../utils/shm_tool/shm -p /iobench_shm -w 0 &> /dev/null

    if [ "$cpu" = "measure-exetime" ]; then
        measureParsecAndPrint "${op}.${iosize}.p${proc_num}.r${round}"

        echo "Kill iobench processes."
        sudo pkill -9 -f iobench.infinite

    else
        shm_val=0
        while [ $shm_val -ne "$proc_num" ]; do
            shm_val=$(../../utils/shm_tool/shm -p /iobench_shm -r)
            echo -e "\tfinished/total: $shm_val/$proc_num"
            sleepAndCountdown 5
        done

        # Get end time. Note that end_time is the end time of the last process. The
        # other process may finish much earlier.
        iobench_end_time=$(date +%s.%N)

        # Get iobench execution time.
        iobench_exetime=$(echo "$iobench_end_time - $iobench_start_time" | bc -l)
        echo -n "Benchmark elapsed time: "
        # echo "$iobench_exetime"
        echo -n $(LC_ALL=C /usr/bin/printf "%.*f\n" "3" "$iobench_exetime")
        echo " seconds"

        if [ "$cpu" = "cpu-busy" ]; then
            echo "Kill parsec."
            killParsec
        fi

        echo "Send signal to quit benchmark processes."
        ./iobench -p

        # wait for all pids.
        echo "Waiting for the processes to exit."
        for pid in ${pids[*]}; do
            wait $pid
        done

    fi

    # grep results and write to result file.
    result_file="${RESULT_DIR}/${op}.${iosize}.p${proc_num}.r${round}"
    echo "result_file: $result_file"
    echo "$round" >$result_file
    grep -h "synchronous digest" $OUTPUT_DIR/${op}.${iosize}.p${proc_num}.r${round}.id${i} | wc -l >>$result_file
    grep -h "Throughput" $OUTPUT_DIR/${op}.${iosize}.p${proc_num}.r${round}.id* >>$result_file

    # grep sync digest.
    # echo "# of synchronous digestions:"
    # for i in $(seq 1 $proc_num); do
    #     grep "synchronous digest" $OUTPUT_DIR/${op}.${iosize}.p${proc_num}.r${round}.id${i}
    # done
    # for i in $(seq 1 $proc_num); do
    #     grep "synchronous digest" $OUTPUT_DIR/${op}.${iosize}.p${proc_num}.r${round}.id${i} | wc -l
    # done

    # grep throughput
    # echo "Throughput (MB/s):"
    # for i in $(seq 1 $proc_num); do
    #     grep -h "Throughput" $OUTPUT_DIR/${op}.${iosize}.p${proc_num}.r${round}.id${i} | cut -d ' ' -f 2
    # done
}

print_result() {
    # Print results.
    for OP in $OPS; do
        for IOSIZE in $IOSIZES; do
            for PROCS in $NPROCS; do
                for ROUND in $(seq 1 $ROUNDS); do
                    # grep -h Throughput ${RESULT_DIR}/${OP}.${IOSIZE}.p${PROCS}.r${ROUND} | cut -d ' ' -f 2
                    echo -n "Aggregated_throughput(MB/s):${OP}_${IOSIZE}_${PROCS}procs_${ROUND}round "
                    grep -h Throughput ${RESULT_DIR}/${OP}.${IOSIZE}.p${PROCS}.r${ROUND} | cut -d ' ' -f 2 | awk '{ total += $1} END { printf("%.3f\n", total) }'
                done
            done
        done
    done
}

print_usage() {
    echo "Usage: $0 [ -t nic|hostonly ] [ -p dir_path ] [ -c|-a ] [ -y ]
    -t : system type <nic|hostonly> (hostonly is default)
    -p : print result of <dir_path>
    -c : run with cpu-intensive job
    -a : measure parsec execution time (kernfs pinning mode) (File size should be large enough. Recommend: 10G file size, 4K io size)
    -y : run with hyperloop"
}

# Parse command options.
while getopts "t:p:cay?h" opt; do
    case $opt in
    a)
        CPU="measure-exetime"
        echo "Measure parsec execution time."
        ;;
    c)
        CPU="cpu-busy"
        echo "Run with CPU-intensive job, parsec."
        ;;
    p)
        OUTPUT_DIR=$OPTARG
        RESULT_DIR=$OPTARG
        ACTION="print"
        echo "Print mode. OPTARG:$OPTARG OUTPUT_DIR:$OUTPUT_DIR RESULT_DIR:$RESULT_DIR"
        ;;
    t)
        TYPE=$OPTARG
        if [ $TYPE != "nic" -a $TYPE != "hostonly" ]; then
            print_usage
            exit 2
        fi
        ;;
    y)
        HYPERLOOP="true"
        echo "Running bench as a Hyperloop mode."
        ;;
    h | ?)
        print_usage
        exit 2
        ;;
    esac
done

# Check options.
if [ "$HYPERLOOP" = "true" -a "$TYPE" != "hostonly" ]; then
    echo "Hyperloop mode should run as hostonly."
    exit 2
fi

# output directory.
if [ "$ACTION" = "print" ]; then
    echo "output dir:$OUTPUT_DIR"
    echo "result dir:$RESULT_DIR"
elif [ "$TYPE" = "nic" ]; then
    OUTPUT_DIR=${OUTPUT_DIR}/linefs
    RESULT_DIR=${RESULT_DIR}/linefs
else
    OUTPUT_DIR=${OUTPUT_DIR}/assise
    RESULT_DIR=${RESULT_DIR}/assise
fi

if [ "$CPU" = "cpu-busy" ]; then
    OUTPUT_DIR=${OUTPUT_DIR}/cpu
    RESULT_DIR=${RESULT_DIR}/cpu
else
    OUTPUT_DIR=${OUTPUT_DIR}/solo
    RESULT_DIR=${RESULT_DIR}/solo
fi

# print mode.
if [ $ACTION = "print" ]; then
    echo "Run as Print mode."
    print_result
    exit 0
fi

# Kill processes.
# if [ "$CPU" = "cpu-busy" ]; then
echo "Kill parsec."
killParsec
# fi

# if [ "$HYPERLOOP" = "true" ]; then
killHyperloopServers
# fi

# kernfs is reset by reset_kernfs().

# reset dirs.
[ -d $OUTPUT_DIR ] && rm -rf $OUTPUT_DIR/*
mkdir -p $OUTPUT_DIR

[ -d $RESULT_DIR ] && rm -rf $RESULT_DIR/*
mkdir -p $RESULT_DIR

# Build shm tool if required.
if [ ! -f ${PROJ_DIR}/utils/shm_tool/shm ]; then
    echo "No shm bin found. Build it."
    (
        cd ${PROJ_DIR}/utils/shm_tool
        make
    )
fi

# execute.
for OP in $OPS; do
    for IOSIZE in $IOSIZES; do
        for PROCS in $NPROCS; do
            for ROUND in $(seq 1 $ROUNDS); do
                if [ "$HYPERLOOP" = "true" ]; then
                    trace_path="${HYPERLOOP_TRACE_FILE_PREFIX}/tput_${PROCS}proc/sw.${IOSIZE}.p${PROCS}.r1.id"
                    replaceHyperloopTracePath $trace_path
                    for p in $(seq 1 $PROCS); do
                        runHyperloopServersForTput $p
                    done

                fi

                reset_kernfs $TYPE

                if [ "$PROCS" -gt 1 ] && [ "$HYPERLOOP" != "true" ]; then
                    echo "Make private directories for each processes. (NPROCS = $PROCS)"
                    (cd ../../libfs/tests && sudo ./run.sh ./mkdir_test &> /dev/null)
                    echo "Done."
                fi

                exe_all_procs $OP $IOSIZE $PROCS $ROUND $CPU

                if [ "$HYPERLOOP" = "true" ]; then
                    killHyperloopServers
                fi
                sleep 2
            done
        done
    done
done

# Print results.
print_result

# shutdown kernfs.
kill_kernfs $TYPE
if [ "$cpu" = "cpu-busy" ]; then
    echo "Kill parsec."
    killParsec
fi
exit
