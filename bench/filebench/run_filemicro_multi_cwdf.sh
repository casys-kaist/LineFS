#! /bin/bash

FB_FILE="filemicro_multi_cwdf.f"

sed -i "s/\(eventrate *= *\).*/\1$1/" $FB_FILE
sed -i "s/\(meanappendsize *= *\).*/\1$2/" $FB_FILE
#sed -i "s/\(iters *= *\).*/\1$3/" $FB_FILE

rm -rf results/FM_$1\_$2\_$3\_$4.out
#rm -rf results/bw.tmp

#collectl -sx -i 10 -c 3 > results/bw.tmp &

./run_multi.sh ./filebench.mlfs -f $FB_FILE > results/FM_$1\_$2\_$3\_$4.out

#sleep 2

#cat results/bw.tmp >> results/FM_$1\_$2\_$3.out

#./run.sh strace -ff ./filebench.mlfs -f varmail_mlfs.f 2> a.strace
