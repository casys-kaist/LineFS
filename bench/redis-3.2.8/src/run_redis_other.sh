#! /bin/bash

rm -rf ./pmem/*
drop_caches
./redis-server ../redis_ext4.conf
