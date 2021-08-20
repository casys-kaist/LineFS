#!/bin/bash
# This scirpts formats device (mkfs) and runs kernfs.
# You can set delay by creating ../local_scripts/sleep_mkfs.sh and adding some sleep time as below.
# In ../local_scripts/sleep_mkfs.sh file,
# #!/bin/bash
# sleep 10
#
DEBUG=0

print_usage()
{
    echo "Format device (mkfs) and run kernfs.
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

if [ -f "local_scripts/sleep_mkfs.sh" ]; then
    echo Sleep for a while.
    ./local_scripts/sleep_mkfs.sh
fi

if [ "$(gcc -dumpmachine)" != "aarch64-linux-gnu" ]; then
    make mkfs
fi

# Run kernfs.
if [ $DEBUG = "1" ]; then
    scripts/run_kernfs.sh -d
else
    scripts/run_kernfs.sh
fi