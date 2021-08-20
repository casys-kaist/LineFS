#!/bin/bash
sleepAndCountdown() {
    secs=$1
    while [ $secs -gt 0 ]; do
	echo -ne "$secs\033[0K\r"
	sleep 1
	: $((secs--))
    done
}

print_usage()
{
    echo "Usage: $0 <seconds_to_sleep>"
}

printCmd() {
    :
    # cmd=$1
    # echo "$cmd"
}

if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then # script is sourced.
    unset -f print_usage
else # script is executed directly.
    if [ -z "$1" ]; then
	print_usage
	exit 1
    fi
    sleepAndCountdown $1
fi
