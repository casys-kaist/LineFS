CC = gcc -std=gnu99
#CC = c99
EXE = iotest file_basic small_io falloc_test rsync_test lock_test lock_perf rbtree_test dir_test many_files_test fork_io readdir_test statdir_test append_test fwrite_fread slave_test partial_update_test ftrunc_test mkdir_test lease_test maildir_test mkdir_user iobench
#$(info $(EXE))

CUR_DIR = $(shell pwd)
LIBFS_DIR = $(CUR_DIR)/../build/
NVML_DIR = $(LIBFS_DIR)/../lib/nvml/src/nondebug/
CUCKOO_DIR = $(LIBFS_DIR)/../lib/cuckoofilter
RDMA_DIR = $(CURDIR)/../lib/rdma
RDMA_CORE_DIR := $(CURDIR)/../lib/rdma-core
LIBFS=$(LIBFS_DIR)/libmlfs.a

INCLUDES= $(LIBFS_DIR)/../src -I$(CUR_DIR)/../lib/nvml/src/include -I$(RDMA_CORE_DIR)/build/include -I$(RDMA_DIR)

DPDK_VER := 16.07.2
DPDK_DIR := $(abspath $(CURDIR)/../lib/dpdk-$(DPDK_VER)/x86_64-native-linuxapp-gcc)

CFLAGS=-I $(DPDK_DIR)/include -I$(INCLUDES)

LDFLAGS = -Wl,-rpath=$(abspath $(LIBFS_DIR)) \
			-Wl,-rpath=$(abspath $(NVML_DIR)) \
			-Wl,-rpath=$(abspath $(RDMA_CORE_DIR)) \
			-Wl,-rpath=$(abspath $(RDMA_DIR)) \
			-lpthread -lrt -lm -lssl -lcrypto

all: $(EXE)

%.o: %.c
	$(CC) $(CFLAGS) -c -g $< -o $@

%.o: %.cc
	$(CXX) -std=c++11 $(CFLAGS) -c -g $< -o $@

time_stat.o: time_stat.c
	$(CC) time_stat.c -c -o time_stat.o -D_BSD_SOURCE

file_basic: file_basic.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

small_io: small_io.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

append_test: append_test.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

mkdir_user: mkdir_user.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

signal_test: signal_test.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

falloc_test: falloc_test.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

rsync_test: rsync_test.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

ftrunc_test: ftrunc_test.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

readdir_test: readdir_test.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

statdir_test: statdir_test.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -pthread $(LDFLAGS)

slave_test: slave_test.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

slave_random_kill: slave_random_kill.c time_stat.o
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

mkdir_user: mkdir_user.c time_stat.o
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

maildir_test: maildir_test.c time_stat.o
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

mkdir_test: mkdir_test.c time_stat.o
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)

iotest: iotest.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -g -Ofast -o $@ $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs  -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -DMLFS $(CFLAGS) $(LDFLAGS)
	$(CXX) -std=c++11 -g -Ofast -o $(addsuffix .normal, $@) $^ $(CFLAGS) -L$(NVML_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR))

fwrite_fread: fwrite_fread.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -g -Ofast -o $@ $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(RDMA_CORE_DIR) -L$(RDMA_DIR)  -DMLFS $(CFLAGS) $(LDFLAGS) 

lock_test: lock_test.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) ../build/concurrency/locks.o ../build/concurrency/condvar.o -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -pthread -lrt

lock_perf: lock_perf.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) ../build/concurrency/locks.o ../build/concurrency/condvar.o -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -pthread -lrt -DPERF_mlfs

rbtree_test: rbtree_test.cc
	$(CXX) -std=c++11 -g -o $@ $^ -I$(INCLUDES) ../build/ds/rbtree.o -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -pthread -lrt 

dir_test: dir_test.c
	$(CC) -O0 -g -o $@ $^ $(CFLAGS) -L$(LIBFS_DIR) -lmlfs -lm -lrt  -L$(NVML_DIR) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR)) -I$(INCLUDES) $(LDFLAGS)

many_files_test: many_files_test.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -O2 -g -o $@ $^ $(CFLAGS) -L$(LIBFS_DIR) -lmlfs -lm -lrt  -L$(NVML_DIR) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR)) -I$(INCLUDES) $(LDFLAGS)

file_create_test: file_create_test.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -O2 -g -o $@ $^ $(CFLAGS) -L$(LIBFS_DIR) -lmlfs -lm -lrt  -L$(NVML_DIR) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR)) -I$(INCLUDES) $(LDFLAGS)

lease_test: lease_test.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -O2 -g -o $@ $^ $(CFLAGS) -L$(LIBFS_DIR) -lmlfs -DMLFS -lm -lrt  -L$(NVML_DIR) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR)) -I$(INCLUDES) $(LDFLAGS)
	$(CXX) -std=c++11 -O2 -g -o $(addsuffix .normal, $@) $^ $(CFLAGS) -L$(LIBFS_DIR) -lmlfs -DCEPH -lm -lrt  -L$(NVML_DIR) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR)) -I$(INCLUDES) $(LDFLAGS)

partial_update_test: partial_update_test.cc
	$(CXX) -std=c++11 -g -O0 -o $@ $^ $(CFLAGS) -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) -lm -lrt -lpthread $(LDFLAGS) -fopenmp
	#$(CXX) -std=c++11 -g -o $@ $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs  -DMLFS $(CFLAGS) $(LDFLAGS) 

fork_io: fork_io.cc time_stat.o 
	$(CXX) -std=c++11 -g -o $@ $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(RDMA_CORE_DIR) -L$(RDMA_DIR)  -DMLFS $(CFLAGS) $(LDFLAGS) 

iobench: iobench.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -g -Ofast -o $@ $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_DIR) -lrdma -lrdmacm -libverbs $(LDFLAGS)
	$(CXX) -std=c++11 -g -Ofast -o $(addsuffix .normal, $@) $^ $(CFLAGS) $(DAX_OBJ) -L$(NVML_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR))

clean:
	rm -rf *.o $(EXE) *.normal

