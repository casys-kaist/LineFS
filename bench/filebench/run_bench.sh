#! /usr/bin/sudo /bin/bash
PROJ_ROOT=../../
MICROBENCH_DIR=${PROJ_ROOT}/bench/micro

CPU_JOB=0
FILE_SERVER=1
VARMAIL=1

print_usage() {
	echo "Run Filebench.
Usage: $0 [ -t hostonly|nic ] [ -c ] [ -f ] [ -v ]
    -c : Run with a cpu-intensive job.
    -f : Run only fileserver workload. (Default = run all)
    -v : Run only varmail workload. (Default = run all)
    -t : System type. <hostonly|nic> (hostonly is default)"
}

while getopts "cft:v?h" opt; do
	case $opt in
	c)
		CPU_JOB=1
		;;
	f)
		FILE_SERVER=1
		VARMAIL=0
		;;
	v)
		FILE_SERVER=0
		VARMAIL=1
		;;
	t)
		SYSTEM=$OPTARG
		;;
	h | ?)
		print_usage
		exit 2
		;;
	esac
done

run_parsec() {
	(
		cd $MICROBENCH_DIR || exit
		./scripts/parsec_ctl.sh -r
	) # parsec
	# (cd $MICROBENCH_DIR; ./scripts/parsec_ctl.sh -n) # stress-ng
}

quit_parsec() {
	(
		cd $MICROBENCH_DIR || exit
		./scripts/parsec_ctl.sh -k
	) # parsec
	# (cd $MICROBENCH_DIR; ./scripts/parsec_ctl.sh -q) # stress-ng
}

if [ "$SYSTEM" != "hostonly" ] && [ "$SYSTEM" != "nic" ]; then
	print_usage
	exit 2
fi

run_varmail() {
	########### Run varmail ##############
	# Reset kernfs.
	(
		cd $MICROBENCH_DIR || exit
		./scripts/reset_kernfs.sh -t "$SYSTEM"
	)

	OUTPUT="output.varmail"
	if [ $CPU_JOB = "1" ]; then
		OUTPUT="${OUTPUT}_cpu"
		run_parsec
	fi

	echo "**************************** RUN BENCH ****************************"
	echo "Workload       : Varmail"
	echo "System         : $SYSTEM"
	if [ $CPU_JOB = "1" ]; then
		echo "Co-running app : Streamcluster"
	else
		echo "Co-running app : N/A"
	fi
	echo "*******************************************************************"
	./run_varmail.sh | tee $OUTPUT

	quit_parsec

}

run_fileserver() {
	########### Run fileserver ##############
	# Reset kernfs.
	(
		cd $MICROBENCH_DIR || exit
		./scripts/reset_kernfs.sh -t "$SYSTEM"
	)

	OUTPUT="output.fileserver"
	if [ $CPU_JOB = "1" ]; then
		OUTPUT="${OUTPUT}_cpu"
		run_parsec
	fi

	echo "**************************** RUN BENCH ****************************"
	echo "Workload       : Fileserver"
	echo "System         : $SYSTEM"
	if [ $CPU_JOB = "1" ]; then
		echo "Co-running app : Streamcluster"
	else
		echo "Co-running app : N/A"
	fi
	echo "*******************************************************************"
	./run_fileserver.sh | tee $OUTPUT

	quit_parsec

}

# Quit on begin.
quit_parsec

if [ "$VARMAIL" = "1" ]; then
	run_varmail
fi

if [ "$FILE_SERVER" = "1" ]; then
	run_fileserver
fi

# Terminate NICFSes and kernel workers of LineFS or SharedFS of Assise.
(
	cd $MICROBENCH_DIR || exit
	./scripts/reset_kernfs.sh -t "$SYSTEM" -k
)
