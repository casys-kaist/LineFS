#!/bin/bash
OUTPUT=./lease_results
NPROC=2 # The number of processes.
START=1
END=$(expr "$START" + "$NPROC" - 1);
mkdir -p $OUTPUT

#./run.sh mkdir_user
./mkdir_user

sleep 1

for i in `seq $START $END`
do
	#./run.sh lease_test c 1000 1 aaa &
	#echo "${userlist[i]}"
	#./run.sh lease_test c 1000 1 "${userlist[i]}" &
         content=$(sed -n "$i p" userlist.dat)	
	#./run.sh lease_test n 1000 1 "${content}_old" "${content}_new" &
	./lease_test n 1000 1 "${content}_old" "${content}_new" > "output_$i.log" &
	sleep 2
done
echo "$NPROC processes were started. Send signal after 4 seconds."
sleep 4
./lease_test s  # Send signal automatically.

#./run.sh lease_test c 1 1 &
#./run.sh lease_test c 1 1 &
#./run.sh lease_test c 1 1 &

#./run.sh lease_test c 1 1 &>> lease_results/lease_test.out &
#sleep 1
#./run.sh lease_test c 1 1 &>> lease_results/lease_test.out &
#./run.sh lease_test c 1 1 &>> lease_results/leasetest.out
