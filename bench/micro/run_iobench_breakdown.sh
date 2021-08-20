#!/bin/bash

# TYPES="sw sr rr rw"
TYPES="sw"
# IO_SIZES="128B 1K 4K 16K 64K 1M"
#IO_SIZES="1M 64K 16K 4K 1K 128B"
#IO_SIZES="4K 16K 64K 1M"
IO_SIZES="1M 64K 16K 4K 1K"
# IO_SIZES="1M 64K 16K 4K"
#FILE_SIZE="150M"
FILE_SIZE="3600M"
DATE=$(date +"%y%m%d-%H%M%S")
JOB_NAME="$DATE"
DIR_PATH="./results/${JOB_NAME}"    # MUST be changed.
DROP_CACHE='echo 3 | sudo tee /proc/sys/vm/drop_caches' # drop buffer cache.

dropCache()
{
    echo 3 | sudo tee /proc/sys/vm/drop_caches
}

./mkfs.sh

for TYPE in $TYPES
do
    FILE_PATH="${DIR_PATH}/${TYPE}"
    echo "mkdir -p $FILE_PATH"
    mkdir -p $FILE_PATH

    for IO_SIZE in $IO_SIZES
    do
        #RUN_CMD="numactl -N 0 -m 0 ./iobench -s $TYPE $FILE_SIZE $IO_SIZE 24"
        RUN_CMD="numactl -N 0 -m 0 ./iobench -s $TYPE $FILE_SIZE $IO_SIZE 1"
        FILE_NAME="${FILE_PATH}/${TYPE}_${FILE_SIZE}_${IO_SIZE}_${JOB_NAME}"

	echo Running: $RUN_CMD

	sync
	dropCache

        ssh bull2-nic sync
        ssh bull2-nic $DROP_CACHE

#	echo "$RUN_CMD" > $FILE_NAME
#	echo "########" >> $FILE_NAME
#	free -h >> $FILE_NAME
#	echo "########" >> $FILE_NAME
	sleep 2
        $RUN_CMD | tee -a "breakdown_${JOB_NAME}.log"
        # $RUN_CMD >> $FILE_NAME
#	echo "########" >> $FILE_NAME
#	free -h >> $FILE_NAME
	echo Done.
    done

    # parsing the result files.
    # python parsing_thu.py $FILE_PATH
done
grep "avg" "breakdown_${JOB_NAME}.log" | cut -d '(' -f 2 | cut -d ' ' -f 1

# print to the console.
#for DIR_NAME in ${DIR_PATH}/* ;
#do
#    echo $DIR_NAME
#    cat ${DIR_NAME}/output.txt
#done
