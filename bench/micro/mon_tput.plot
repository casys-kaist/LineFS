# Plot tput_monitor result. Run mon_get_aggr_tput.sh first to get sum.log.

set term dumb
# set autoscale
set yrange [0:*]
plot 'mon_result/sum.log' using 1:2 with line notitle
#plot 'error.1.log' using 1:2 title "proc1" with line
#         'error.2.log' using 1:2 title "proc2", \
#         'error.3.log' using 1:2 title "proc3", \
#         'error.4.log' using 1:2 title "proc4"
#pause 1
#reread
