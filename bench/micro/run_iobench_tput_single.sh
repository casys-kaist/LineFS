#!/bin/bash
FILE_SIZE="4G"
ROUNDS=5
THREADS=1

shutdown_assise() {
    while true
    do
	./iobench -p
	sleep 1
    done
}

if [ "$1" == "cpu" ]; then # measure throughput of iobench.
    echo "Measuring iobench throughput co-running with parsec."
    rm -f cpu.out
    for ROUND in $(seq 1 $ROUNDS); do
	/usr/bin/time -f "iobench time: %e" sudo numactl -N0 -m0 ./run.sh ./iobench -s -w sw $FILE_SIZE 4K $THREADS >> cpu.out &
	iobench_pid=$!
	sleep 15

	/home/yulistic/bench/parsec-3.0/run_parsec.sh &> /dev/null &
	sleep 2

	sudo ./iobench -p # start iobench

	sleep 60 # proper value?

	sudo pkill -9 ferret
	sleep 1
	sudo ./iobench -p # shutdown_fs
	wait $iobench_pid

	echo "Round $ROUND finished."
	sleep 1
    done
    grep throughput cpu.out | cut -d " " -f 3

elif [ "$1" == "parsec" ]; then # measure execution time of parsec.
    echo "Measuring parsec execution time co-running with iobench."
    rm -f parsec.out
    for ROUND in $(seq 1 $ROUNDS); do
	/usr/bin/time -f "iobench time: %e" sudo numactl -N0 -m0 ./run.sh ./iobench -s -w sw 10G 4K $THREADS &
	iobench_pid=$!

	sleep 17
	sudo ./iobench -p # start iobench
	sleep 2
	/home/yulistic/bench/parsec-3.0/run_parsec.sh short >> parsec.out 2>/dev/null &
	parsec_pid=$!

	wait $parsec_pid
	echo "Parsec finished."

	echo "Shutting down Assise"
	shutdown_assise &
	shutdown_pid=$!

	wait $iobench_pid
	echo "Round $ROUND finished."
	kill -9 $shutdown_pid

	sleep 2
    done
    grep QUERY parsec.out | cut -d " " -f 3

else
    rm -f solo.out
    echo "Iobench solo run."
    for ROUND in $(seq 1 $ROUNDS); do
	sudo numactl -N0 -m0 ./run.sh ./iobench -s -w sw $FILE_SIZE 4K $THREADS >> solo.out &
	# sudo numactl -N0 -m0 ./run.sh ./iobench -s -w sw $FILE_SIZE 4K 1 &
	iobench_pid=$!

	sleep 17
	echo "Send signal to start iobench."
	sudo ./iobench -p # start iobench

	sleep 40

	echo "Send signal to shutdown MLFS."
	sudo ./iobench -p # shutdown_fs
	wait $iobench_pid

	echo "Round $ROUND finished."
	sleep 2
    done
    grep throughput solo.out | cut -d " " -f 3
fi
