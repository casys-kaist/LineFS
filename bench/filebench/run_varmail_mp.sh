#! /bin/bash
# 1. Run this script and wait until all the processes start. Each master process
#   waits shm being 0 at this point.
# 2. Make shm(/dev/shm/shm_filebench_mp) to 0. You can use shm_tool in utils
#   directory as below.
#	cd utils/shm_tool && make
#   	sudo ./shm -p /shm_filebench_mp -w 0
#   It makes master processes start to measure time. (For example 30 seconds.)
# 3. Bench output is recorded to $OUTPUT directory.

NPROC=2
OUTPUT="outputs"

mkdir -p $OUTPUT

for i in `seq 1 $NPROC`
do
    echo "MLFS=1 ./filebench.mlfs -f varmail_workloads/varmail_mlfs_$i.f"
    SHM_WAIT=1 ./run.sh ./filebench.mlfs -f varmail_workloads/varmail_mlfs_$i.f > $OUTPUT/output_$i.log &
    sleep 5
done
echo "$i processes started";
