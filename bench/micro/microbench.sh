#! /bin/bash

echo "-------- 1 thread - 30 GB"
FILESIZE=30G
THREAD=1
TYPE=sw
DIR="-d ./pmem"

echo "Sequential write"
for i in `seq 1 1`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=rw
echo "Random write"
for i in `seq 1 1`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=sr
echo "Sequential read"
for i in `seq 1 5`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=rr
echo "Random read"
for i in `seq 1 5`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

sudo rm -rf ./pmem/*
echo "-------- 2 thread - 15 GB"
FILESIZE=15G
THREAD=2
TYPE=sw
echo "Sequential write"

for i in `seq 1 1`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done
TYPE=rw
echo "Random write"
for i in `seq 1 1`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=sr
echo "Sequential read"
for i in `seq 1 5`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=rr
echo "Random read"
for i in `seq 1 5`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

sudo rm -rf ./pmem/*
echo "-------- 4 thread - 7.5 GB"
FILESIZE=7500M
THREAD=4
TYPE=sw
echo "Sequential write"
for i in `seq 1 1`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=rw
echo "Random write"
for i in `seq 1 1`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=sr
echo "Sequential read"
for i in `seq 1 5`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=rr
echo "Random read"
for i in `seq 1 5`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

sudo rm -rf ./pmem/*
echo "-------- 8 thread - 3.75 GB"
FILESIZE=3750M
THREAD=8
TYPE=sw
echo "Sequential write"
for i in `seq 1 1`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=rw
echo "Random write"
for i in `seq 1 1`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=sr
echo "Sequential read"
for i in `seq 1 5`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done

TYPE=rr
echo "Random read"
for i in `seq 1 5`;
do
./iobench.normal $DIR $TYPE $FILESIZE 4K $THREAD
done
