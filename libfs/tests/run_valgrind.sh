#! /bin/bash

#valgrind --trace-children=yes --tool=helgrind  --read-var-info=yes ./run.sh iobench wr ext4 20M 3K 5 |& tee val.txt
valgrind -v --tool=drd --trace-children=yes --read-var-info=yes ./run.sh iobench sw 200M 64K 4 |& tee val.txt
