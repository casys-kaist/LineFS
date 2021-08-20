#! /bin/bash
SERVER=13.13.13.2

#./redis-benchmark -t set -n 1000000 -r 10000000 -d 128 -c 50 -p 7379 -h $SERVER
#./redis-benchmark -t set -n 1000000 -r 10000000 -d 1024 -c 50 -p 7379 -h $SERVER
#./redis-benchmark -t set -n 1000000 -r 10000000 -d 4096 -c 50 -p 7379 -h $SERVER
#./redis-benchmark -t set -n 500000 -r 10000000 -d 8192 -c 50 -p 7379 -h $SERVER

#synchronous 
#./redis-benchmark -t set -n 1000000 -r 10000000 -d 128 -c 50 -p 7379 --sync_rep -h $SERVER
#./redis-benchmark -t set -n 1000000 -r 10000000 -d 1024 -c 50 -p 7379 --sync_rep -h $SERVER
#./redis-benchmark -t set -n 1000000 -r 10000000 -d 4096 -c 50 -p 7379 --sync_rep -h $SERVER
#./redis-benchmark -t set -n 500000 -r 10000000 -d 8192 -c 50 -p 7379 --sync_rep -h $SERVER

#AOF mode
#./redis-benchmark -t set -n 1000 -r 1000 -d 1024 -c 50
#./redis-benchmark -t set -n 2000000 -r 2000000 -d 256
./redis-benchmark -t set -n 1000000 -r 1000000 -d 1024 -c 50
#./redis-benchmark -t set -n 450000 -r 5000000 -d 4096 -c 200
#./redis-benchmark -t set -n 100000 -r 5000000 -d 4096 -c 200
#./redis-benchmark -t set -n 250000 -r 3000000 -d 8192 -c 200

#RDB mode
#./redis-benchmark -t set -n 600000 -r 5000000 -d 4096 -c 200
