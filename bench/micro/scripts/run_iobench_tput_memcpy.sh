#! /usr/bin/sudo /bin/bash
{
    # set -vx
    source scripts/reset_kernfs.sh
    source scripts/parsec_ctl.sh
    source scripts/utils.sh

    # echo "Host project dir path  : $PROJ_DIR"
    # echo "NIC project dir path   : $NIC_PROJ_DIR"
    # echo "Parsec dir path (host) : $PARSEC_DIR"

    NPROC="4"
    OUTPUT_DIR="./outputs/tput"
    RESULT_DIR="./results/tput"
    # TOTAL_FILE_SIZE="12288" # 12G
    # TOTAL_FILE_SIZE="14336" # 14G
    TOTAL_FILE_SIZE="16384" # 16G
    # TOTAL_FILE_SIZE="36864" # 36G
    IOSIZE="16K"
    TYPE="linefs"
    ACTION="run"
    MAKE_PRIVATE_DIR="true"

    exe_proc() {
        # Arguments
        id=$1
        iosize=$2
        proc_num=$3

        out_file_path=${OUTPUT_DIR}/${iosize}.p${proc_num}.id${id}

        run_bin="iobench"

        ## Normal priority
        echo "sudo FILE_ID=$id $PINNING ./run.sh ./${run_bin} -d /mlfs/$id -s -w sw $(($TOTAL_FILE_SIZE / $proc_num))M $iosize 1 > $out_file_path"
        sudo FILE_ID=$id $PINNING ./run.sh ./${run_bin} -d /mlfs/$id -s -w sw $(($TOTAL_FILE_SIZE / $proc_num))M $iosize 1 >$out_file_path &
    }

    exe_all_procs() {
        iosize=$1
        proc_num=$2

        echo "------------- Config ---------------"
        echo " Operation           : Sequential Write"
        echo " Total_file_size(MB) : $TOTAL_FILE_SIZE"
        echo " IO_size             : $iosize"
        echo " Num_of_processes    : $proc_num"
        echo "------------------------------------"

        # Reset shm value.
        ../../utils/shm_tool/shm -p /iobench_shm -w 0 &>/dev/null
        echo "shm_val=$(../../utils/shm_tool/shm -p /iobench_shm -r)"

        # run processes and store pids in array.
        echo "Start $proc_num processes."
        for i in $(seq 1 $proc_num); do
            exe_proc $i $iosize $proc_num
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

        is_local=false

        # Start streamcluster
        measureParsec "${iosize}.p${proc_num}" $is_local

        sleep 2

        # Start tput microbench (iobench)
        # Send signal to all processes.
        echo "Send signal to start iobench."
        # ./iobench -p
        ../../utils/shm_tool/shm -p /iobench_shm -w 0 &>/dev/null

        # Wait for streamcluster done.
        waitParsec $is_local
        sleep 1 # Wait for the replica's streamcluster.
        printStremaclusterExeTime $is_local

        echo "Send signal to quit benchmark processes."
        # ./iobench -p
        ../../utils/shm_tool/shm -p /iobench_shm -w 0 &>/dev/null

        # wait for all pids.
        echo "Waiting for the benchmark processes to exit."
        for pid in ${pids[*]}; do
            wait $pid
        done

        # grep results and write to result file.
        result_file="${RESULT_DIR}/${iosize}.p${proc_num}"
        echo "result_file: $result_file"
        grep -hc "synchronous digest" $OUTPUT_DIR/${iosize}.p${proc_num}.id${i} >>$result_file
        grep -h "Throughput" $OUTPUT_DIR/${iosize}.p${proc_num}.id* >>$result_file
        cat "$result_file"
    }

    print_result() {
        # Print results.
        for IOSIZE in $IOSIZES; do
            echo -n "Aggregated_throughput(MB/s):${IOSIZE}_${NPROC}procs "
            grep -h Throughput ${RESULT_DIR}/${IOSIZE}.p${NPROC} | cut -d ' ' -f 2 | awk '{ total += $1} END { printf("%.3f\n", total) }'
        done
    }

    print_usage() {
        echo "Usage: $0 [ -n ] [ -p dir_path ] [ -r ]
    -n : Do not formatting device.
    -p : Print result of <dir_path>.
    -r : Prepare for no-copy run."
    }

    prepare_nocopy_exp() {
		# Format device and mkdir private directories before configuring LineFS to NO-COPY.
		# It is required because LineFS cannot create private directories with the NO-COPY option.
		reset_kernfs "nic"
		# Make private directories
		echo "Make private directories for each processes. (num_procs = 4)"
		(cd "${PROJ_DIR}/libfs/tests" && sudo ./run.sh ./mkdir_test &>/dev/null)
    }

    # Main.
    if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then # script is executed directly.

        # Parse command options.
        while getopts "np:r?h" opt; do
            case $opt in
            n)
                MAKE_PRIVATE_DIR="false"
                ;;
            p)
                OUTPUT_DIR=$OPTARG
                RESULT_DIR=$OPTARG
                ACTION="print"
                echo "Print mode. OPTARG:$OPTARG OUTPUT_DIR:$OUTPUT_DIR RESULT_DIR:$RESULT_DIR"
                ;;
            r)
                prepare_nocopy_exp
                exit
                ;;
            h | ?)
                print_usage
                exit 2
                ;;
            esac
        done

        # output directory.
        if [ "$ACTION" = "print" ]; then
            echo "output dir:$OUTPUT_DIR"
            echo "result dir:$RESULT_DIR"
        elif [ "$TYPE" = "linefs" ]; then
            OUTPUT_DIR=${OUTPUT_DIR}/linefs
            RESULT_DIR=${RESULT_DIR}/linefs
        fi

        # print mode.
        if [ $ACTION = "print" ]; then
            echo "Run as Print mode."
            print_result
            exit 0
        fi

        echo "Kill parsec."
        killParsec

        # kernfs is reset by reset_kernfs().

        # reset dirs.
        [ -d "$OUTPUT_DIR" ] && rm -rf "${OUTPUT_DIR:?}"
        mkdir -p "$OUTPUT_DIR"

        [ -d $RESULT_DIR ] && rm -rf "${RESULT_DIR:?}"
        mkdir -p "$RESULT_DIR"

        # Build shm tool if required.
        if [ ! -f "${PROJ_DIR}"/utils/shm_tool/shm ]; then
            echo "No shm bin found. Build it."
            (
                cd "${PROJ_DIR}"/utils/shm_tool || exit
                make
            )
        fi

        # execute.
        if [ "$MAKE_PRIVATE_DIR" = "true" ]; then
            reset_kernfs "nic"
        else
            reset_kernfs_without_formatting "nic"
        fi

        if [ "$NPROC" -gt 1 ] && [ "$MAKE_PRIVATE_DIR" = "true" ]; then
            echo "Make private directories for each processes. (num_procs = $NPROC)"
            (cd ../../libfs/tests && sudo ./run.sh ./mkdir_test &>/dev/null)
            echo "Done."
        fi

        echo "nprocs = $NPROC"
        exe_all_procs $IOSIZE $NPROC

        sleep 2

        # Print results.
        print_result

        # shutdown kernfs.
        kill_kernfs "nic"

        echo "Done."
    fi
    exit
}
