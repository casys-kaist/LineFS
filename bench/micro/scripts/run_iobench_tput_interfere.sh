#! /usr/bin/sudo /bin/bash
{
    # set -vx
    source scripts/reset_kernfs.sh
    source scripts/parsec_ctl.sh
    source scripts/utils.sh

    # echo "Host project dir path  : $PROJ_DIR"
    # echo "NIC project dir path   : $NIC_PROJ_DIR"
    # echo "Parsec dir path (host) : $PARSEC_DIR"

    NPROC="1"
    OUTPUT_DIR="./outputs/tput"
    RESULT_DIR="./results/tput"
    # TOTAL_FILE_SIZE="12288" # 12G
    TOTAL_FILE_SIZE="14336" # 16G
    # TOTAL_FILE_SIZE="16384" # 16G
    # TOTAL_FILE_SIZE="36864" # 36G
    IOSIZE="16K"
    TYPE="assise"
    ACTION="run"

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

        is_local=true

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

        # grep results and write to result file.
        result_file="${RESULT_DIR}/${iosize}.p${proc_num}"
        echo "result_file: $result_file"
        grep -hc "synchronous digest" $OUTPUT_DIR/${iosize}.p${proc_num}.id${i} >>$result_file
        grep -h "Throughput" $OUTPUT_DIR/${iosize}.p${proc_num}.id* >>$result_file
        cat "$result_file"

        # Suspended IObenches will be killed by run_interference_exp.sh
    }

    print_result() {
        # Print results.
        for IOSIZE in $IOSIZES; do
            echo -n "Aggregated_throughput(MB/s):${IOSIZE}_${NPROC}procs "
            grep -h Throughput ${RESULT_DIR}/${IOSIZE}.p${NPROC} | cut -d ' ' -f 2 | awk '{ total += $1} END { printf("%.3f\n", total) }'
        done
    }

    print_usage() {
        echo "Usage: $0 [ -t linefs|assise ] [ -p dir_path ]
    -t : system type <linefs|assise> (assise is default)
    -p : print result of <dir_path>"
    }

    # Main.
    if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then # script is executed directly.

        # Parse command options.
        while getopts "t:p:?h" opt; do
            case $opt in
            p)
                OUTPUT_DIR=$OPTARG
                RESULT_DIR=$OPTARG
                ACTION="print"
                echo "Print mode. OPTARG:$OPTARG OUTPUT_DIR:$OUTPUT_DIR RESULT_DIR:$RESULT_DIR"
                ;;
            t)
                TYPE=$OPTARG
                if [ "$TYPE" != "linefs" ] && [ "$TYPE" != "assise" ]; then
                    print_usage
                    exit 2
                fi
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
        else
            OUTPUT_DIR=${OUTPUT_DIR}/assise
            RESULT_DIR=${RESULT_DIR}/assise
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
        if [ "$TYPE" = "linefs" ]; then
            reset_kernfs "nic"
        elif [ "$TYPE" = "assise" ]; then
            reset_kernfs "hostonly"
        else
            echo "Unknown system."
            exit 1
        fi

        if [ "$NPROC" -gt 1 ]; then
            echo "Make private directories for each processes. (num_procs = $NPROC)"
            (cd ../../libfs/tests && sudo ./run.sh ./mkdir_test &>/dev/null)
            echo "Done."
        fi

        echo "nprocs = $NPROC"
        exe_all_procs $IOSIZE 1

        sleep 2

        # Print results.
        print_result

        # shutdown kernfs.
        if [ "$TYPE" = "linefs" ]; then
            kill_kernfs "nic"
        elif [ "$TYPE" = "assise" ]; then
            kill_kernfs "hostonly"
        else
            echo "Unknown system."
            exit 1
        fi

        echo "Done."
    fi
    exit
}
