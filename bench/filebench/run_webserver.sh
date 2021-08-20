#! /bin/bash

rm -rf a.strace
./run.sh ./filebench.mlfs -f webserver_mlfs.f
#./run.sh strace -ff ./filebench.mlfs -f varmail_mlfs.f 2> a.strace
