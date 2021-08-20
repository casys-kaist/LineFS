CC = gcc -std=c99
#CC = c99
EXE = iobench membench iobench_lat iobench_monitor
#$(info $(EXE))

CUR_DIR = $(shell pwd)
PROJECT_ROOT = $(CUR_DIR)/../../
LIBFS_DIR = $(abspath $(PROJECT_ROOT)/libfs/build/)
NVML_DIR = $(abspath $(LIBFS_DIR)/../lib/nvml/src/nondebug/)
RDMA_DIR := $(abspath $(PROJECT_ROOT)/libfs/lib/rdma)
RDMA_CORE_DIR := $(abspath $(PROJECT_ROOT)/libfs/lib/rdma-core)
LIBFS = $(LIBFS_DIR)/libmlfs.a

INCLUDES= $(LIBFS_DIR)/../src -I$(CUR_DIR)/../lib/nvml/src/include -I$(RDMA_CORE_DIR)/build/include -I$(RDMA_DIR)

CFLAGS= -I$(INCLUDES)

LDFLAGS =	  -Wl,-rpath=$(abspath $(LIBFS_DIR)) \
		  -Wl,-rpath=$(abspath $(NVML_DIR)) \
		  -Wl,-rpath=$(abspath $(RDMA_DIR)) \
		  -Wl,-rpath=$(abspath $(RDMA_CORE_DIR)) \
  		  -Wl,-rpath=/usr/lib64 \
		  -Wl,-rpath=/usr/local/lib \
		  -Wl,-rpath=/usr/lib/x86_64-linux-gnu/ \
		  -Wl,-rpath=/lib/x86_64-linux-gnu/ \
		  -lpthread -lrt -lm -lssl -lcrypto -laio

all: $(EXE)

%.o: %.c
	$(CC) $(CFLAGS) -c -g $< -o $@

%.o: %.cc
	$(CXX) -std=c++11 $(CFLAGS) -c -g $< -o $@

time_stat.o: time_stat.c
	$(CC) time_stat.c -c -o time_stat.o -D_BSD_SOURCE

iobench: iobench.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -g -Ofast -o $@ $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)
	$(CXX) -std=c++11 -g -Ofast -o $(addsuffix .infinite, $@) $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS -DRUN_INF $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)
	$(CXX) -std=c++11 -g -Ofast -o $(addsuffix .ntimes, $@) $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS -DRUN_INF -DRUN_N_TIMES $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)
	$(CXX) -std=c++11 -g -Ofast -o $(addsuffix .normal, $@) $^ $(CFLAGS) -L$(NVML_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR))

iobench_monitor: iobench_monitor.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -g -Ofast -o $@ $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS)
	$(CXX) -std=c++11 -g -Ofast -o $(addsuffix .normal, $@) $^ $(CFLAGS) -L$(NVML_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR))

iobench_lat: iobench_lat.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -g -Ofast -o $@ $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) -L$(RDMA_CORE_DIR) -L$(RDMA_DIR) $(LDFLAGS) $(MISC_FLAGS) 
	$(CXX) -std=c++11 -g -Ofast -o $(addsuffix .normal, $@) $^ $(CFLAGS) -L$(NVML_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR))

membench: membench.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -Ofast -g -o $@ $^ $(CFLAGS) -L$(NVML_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR)) -I$(INCLUDES) $(LDFLAGS)

strawman: time_stat.c thread.cc
	gcc time_stat.c -c -std=c99 -D_BSD_SOURCE -O2
	g++ -g -c iobench.cc -std=c++11 -lpthread -O2 -DUSER_BLOCK_MIGRATION
	g++ thread.cc -c -std=c++11 -O2
	#gcc aligned_block_migration.c -c
	g++ batch_migration.cc -c -std=c++11 -O2
	g++ migration_manager.cc -c -std=c++11 -msse4.1 -mavx -O2 -g
	g++ iobench.o thread.o batch_migration.o time_stat.o migration_manager.o -lpthread -std=c++11 -g -O2

clean:
	rm -rf *.o $(EXE) *.normal iobench.ntimes iobench.infinite

