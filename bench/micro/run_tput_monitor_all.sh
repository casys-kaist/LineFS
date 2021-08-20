#!/bin/bash
# sudo ./run_iobench_tput_monitor.sh 1
# cat mon_result/sum.log
# mkdir -p mon_result/1
# cp mon_result/* mon_result/1

# sudo ./run_iobench_tput_monitor.sh 2
# cat mon_result/sum.log
# mkdir -p mon_result/2
# cp mon_result/* mon_result/2

# sudo ./run_iobench_tput_monitor.sh 4
# cat mon_result/sum.log
# mkdir -p mon_result/4
# cp mon_result/* mon_result/4

sudo ./run_iobench_tput_monitor.sh 8
cat mon_result/sum.log
mkdir -p mon_result/8
cp mon_result/* mon_result/8

# sudo ./run_iobench_tput_monitor.sh 12
# cat mon_result/sum.log
# mkdir -p mon_result/12
# cp mon_result/* mon_result/12

# sudo ./run_iobench_tput_monitor.sh 16
# cat mon_result/sum.log
# mkdir -p mon_result/16
# cp mon_result/* mon_result/16
