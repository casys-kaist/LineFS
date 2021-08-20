#! /bin/bash

# $1 should be strace file.

cat $1 | awk -F "(" '{print $1}' |  awk '!($1 in a){a[$1];print}'
