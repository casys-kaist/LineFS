#!/bin/bash
# Get aggregated throughput of all processes.
# Each throughput is stored to a file named as "error.<proc id>.log".

READ_FILE="mon_result/error.*.log"
FIRST_FILE="mon_result/error.1.log"
READ_FILE_COUNT=`find mon_result/ -name "error.*.log" | wc -l`
OUT_FILE="mon_result/sum.log"

#Init
rm -rf $OUT_FILE

# echo "Total file count: $READ_FILE_COUNT"

line_cnt=0
sum=0

while read -r line; do
    sum=0
    line_cnt=$((line_cnt+1))
    #echo "Line no:$line_cnt $line"
    for f in $READ_FILE
    do
        # echo "Parsing file: $f"
        wsize=`awk "NR==$line_cnt" $f | cut -d ' ' -f 2`
        sum=$((sum + wsize))
        # echo "sum: $sum"

    done
    echo "$line_cnt $sum" >> $OUT_FILE
done < $FIRST_FILE

# echo "Done. Output file: $OUT_FILE"
