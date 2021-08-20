#!/bin/bash
BIN="iobench_monitor"
#BIN="iobench_monitor.normal"
OUTDIR="mon_result"
DROP_CACHE='echo 3 | sudo tee /proc/sys/vm/drop_caches' # drop buffer cache.
PROCS=$1

PARSEC_PATH="/home/yulistic/src/parsec-3.0"

plot_graph() {
    while true
    do
	# Get sum.
	sudo ./mon_get_aggr_tput.sh

	# Plot graph to console.
	gnuplot mon_tput.plot

	sleep 5
    done
}

shutdown_assise() {
    while true
    do
	./$BIN -p
	sleep 1
    done

}

# Drop cache.
# sync
# echo 3 | sudo tee /proc/sys/vm/drop_caches
# ssh libra05 sync
# ssh libra05 $DROP_CACHE

# Erase files if exist.
[ -d $OUTDIR ] && rm -rf $OUTDIR
# [ -f output.log ] && rm output.log

mkdir -p $OUTDIR

for run in $(seq 1 $PROCS)
do
	#FILE_ID=$run numactl -N0 -m0 ./run.sh $BIN rr 128M 64K 1 >> output.log &
	# FILE_ID=$run numactl -N0 -m0 ./run.sh $BIN sw 512M 4K 1 -i $run > output.${run}.log &
	#FILE_ID=$run numactl -N0 -m0 ./run.sh $BIN sr 512M 64K 1 &
        #FILE_ID=$run numactl -N0 -m0 ./${BIN} sw -s -w 18G 4K 1 -i $run > ${OUTDIR}/output.${run}.log 2> ${OUTDIR}/error.${run}.log &
        # FILE_ID=$run numactl -N0 -m0 ./${BIN} sw -s -w 256M 4K 1 -i $run > ${OUTDIR}/output.${run}.log 2> ${OUTDIR}/error.${run}.log &
	if [ $PROCS == "1" ]; then
	    fsize="2G"
	else
	    fsize="1G"
	fi
	FILE_ID=$run numactl -N0 -m0 ./run.sh ./${BIN} -d /mlfs/$run sw -s -w -r $fsize 4K 1 -i $run > ${OUTDIR}/output.${run}.log 2> ${OUTDIR}/error.${run}.log &
	pids[${run}]=$!
done

# echo "Run timer process."
# numactl -N0 -m0 ./run.sh $BIN -t &

# sleep 100
sleep $((10 + ${PROCS}*6))
# sleep $((30 + ${PROCS}*6))

echo "Start I/O processes."
./$BIN -p

echo "Run timer process. Throughput(MB/s) will be logged in '${OUTDIR}/error.*.log'."
nice -n -20 ./$BIN -t &

# Start plottting thread.
# plot_graph &
# plot_pid=$!
# echo "Plot pid: $plot_pid"

# Start PARSEC after 5 seconds.
#sleep 5
#${PARSEC_PATH}/bin/parsecmgmt -a run -p swaptions -n 64 -i native > ${OUTDIR}/parsec.log &

# Wait until write done.
# W/ parsec.
#sleep 25

# No parsec.
sleep 60
./$BIN -q # Stop bench.
echo "Kill plot process. pid:$plot_pid"
kill -9 $plot_pid

# number=$(grep -oP '(?<=Throughput: )[0-9]+' ${OUTDIR}/output.log | sort -n | head -1)
# echo "Throughput (MB/s):"
# echo "$PROCS*$number" | bc -l

# grep Throughput ${OUTDIR}/output.*.log

# Get sum.
sudo ./mon_get_aggr_tput.sh

# Plot graph to console.
gnuplot mon_tput.plot

echo "Shutting down Assise."
shutdown_assise &
shutdown_pid=$!

# wait for all processes.
echo "Waiting for all processes to exit"
for pid in ${pids[*]}; do
    wait $pid
done

kill -9 $shutdown_pid

# Kill monitor process.
sudo pkill -9 iobench
