#!/bin/bash
# This script runs kernfs.
# You can set delay by creating ../local_scripts/sleep.sh and adding some sleep time as below.
# In ../local_scripts/sleep.sh file,
# #!/bin/bash
# sleep 10
#
DEBUG=0

print_usage()
{
    echo "Run kernfs.
Usage: $0 [ -d ]
    -d : Run gdb."
}

while getopts "d?h" opt
do
case $opt in
    d)
        DEBUG=1
        ;;
    h|?)
        print_usage
        exit 2
        ;;
esac
done

if [ -f "local_scripts/sleep.sh" ]; then
    echo Sleep for a while.
    ./local_scripts/sleep.sh
fi

if [ $DEBUG = "1" ]; then
    (cd kernfs/tests && sudo gdb -ex run kernfs)
    # (cd kernfs/tests && sudo gdb --tty=/dev/pts/0 -ex run kernfs)
else
    (cd kernfs/tests && sudo nice -n -20 ./run.sh kernfs)
    # (cd kernfs/tests && sudo ./run.sh kernfs) # No priority.
fi
