CC = gcc -std=c99

#CC = c99
EXE = dmabench
#$(info $(EXE))

CUR_DIR = $(shell pwd)
PROJECT_ROOT = $(CUR_DIR)/../../../

#DPDK_VER := 16.07.2
#DPDK_DIR := $(abspath $(PROJECT_ROOT)/libfs/lib/spdk/dpdk/)
#SPDK_ROOT_DIR := $(abspath $(PROJECT_ROOT)/libfs/lib/spdk)
#SPDK_ROOT_INC := $(abspath $(PROJECT_ROOT)/libfs/lib/spdk/include/)



#PDK_DIR := $(abspath $(CUR_DIR)/spdk/dpdk/)
LIBFS_DIR = $(abspath $(PROJECT_ROOT)/libfs/build/)
KERNFS_DIR = $(abspath $(PROJECT_ROOT)/kernfs/)
SPDK_ROOT_DIR := $(abspath $(KERNFS_DIR)/lib/spdk)
SPDK_ROOT_INC := $(abspath $(KERNFS_DIR)/lib/spdk/include/)

include $(SPDK_ROOT_DIR)/mk/spdk.common.mk

SPDK_LIBS += $(SPDK_ROOT_DIR)/build/lib/libspdk_ioat.a \
			 $(SPDK_ROOT_DIR)/build/lib/libspdk_util.a \
			 $(SPDK_ROOT_DIR)/build/lib/libspdk_log.a \
			 $(ENV_LIBS)

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk

#DPDK_LIBS += $(DPDK_DIR)/build/lib/librte_pci.a \
	     $(DPDK_DIR)/build/lib/librte_eal.a

INCLUDES += $(SPDK_ROOT_INC)

CFLAGS= -I$(INCLUDES)

LDFLAGS = -Wl,-rpath=$(abspath $(LIBFS_DIR)) \
	  -Wl,-rpath=$(abspath $(SPDK_ROOT_DIR)) \
	  -Wl,-rpath=/usr/lib64 \
	  -Wl,-rpath=/usr/local/lib \
	  -Wl,-rpath=/usr/lib/x86_64-linux-gnu/ \
	  -Wl,-rpath=/lib/x86_64-linux-gnu/ \
	  -lpthread -lrt -lm -lssl -lcrypto

MLFS_LDFLAGS = -Wl,-rpath=$(abspath $(LIBFS_DIR)) \
		 -Wl,-rpath=$(abspath $(SPDK_ROOT_DIR)) \
		 -Wl,-rpath=$(abspath $(LIBSPDK_DIR)) \
		 -Wl,-rpath=/usr/lib64 \
		 -Wl,-rpath=/usr/local/lib \
		 -Wl,-rpath=/usr/lib/x86_64-linux-gnu/ \
		 -Wl,-rpath=/lib/x86_64-linux-gnu/ \
		 -lpthread -lrt -lm -lssl -lcrypto

#LDFLAGS = -Wl,-rpath=$(abspath $(GLIBC_DIR)) \
		  -Wl,-rpath=$(abspath $(GLIBC_DIR))/rt \
		  -Wl,-rpath=$(abspath $(LIBFS_DIR)) \
		  -Wl,-rpath=$(abspath $(NVML_DIR)) \
		  -Wl,-rpath=$(abspath $(LIBSPDK_DIR)) \
  		  -Wl,-rpath=/usr/lib \
		  -Wl,-rpath=/usr/local/lib \
		  -Wl,-rpath=/usr/lib/x86_64-linux-gnu/ \
		  -Wl,-rpath=/lib/x86_64-linux-gnu/ \
		  -Wl,-dynamic-linker=$(abspath $(GLIBC_DIR))/ld-linux-x86-64.so.2 \
		  -lpthread -lrt -lm -lssl -lcrypto

all: $(EXE)

%.o: %.c
	$(CC) $(CFLAGS) -c -g $< -o $@

%.o: %.cc
	$(CXX) -std=c++11 $(CFLAGS) -c -g $< -o $@

time_stat.o: time_stat.c
	$(CC) time_stat.c -c -o time_stat.o -D_BSD_SOURCE

dmabench: dmabench.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -g -o $(addsuffix .normal, $@) $^ $(CFLAGS) -Wl,--whole-archive $(SPDK_LIBS) -Wl,--no-whole-archive -ldl -luuid -lpmem -lnuma -lpthread -lm -lrt $(LDFLAGS)
	$(CXX) -std=c++11 -g -o $(addsuffix _lat.normal, $@) $^ $(CFLAGS) -Wl,--whole-archive $(SPDK_LIBS) -Wl,--no-whole-archive -ldl -luuid -lpmem -lnuma -lpthread -lm -lrt $(LDFLAGS) -DLAT_BENCH
	#$(CXX) -std=c++11 -g -o $@ $^ $(CFLAGS) -ldl -lpmem -lnuma -lpthread -lm -lrt -L$(LIBFS_DIR) -lmlfs -I$(LIBFS_DIR)/../src -L$(LIBSPDK_DIR) -lspdk $(MLFS_LDFLAGS)
	#$(CXX) -std=c++11 -g -o $(addsuffix _lat, $@) $^ $(CFLAGS) -ldl -lpmem -lnuma -lpthread -lm -lrt -L$(LIBFS_DIR) -lmlfs -I$(LIBFS_DIR)/../src -L$(LIBSPDK_DIR) -lspdk $(MLFS_LDFLAGS) -DLAT_BENCH

	#$(CXX) -std=c++11 -g -Ofast -o $@ $^ $(CFLAGS) -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(LIBSPDK_DIR) -lspdk -DMLFS $(CFLAGS) -L$(RDMA_DIR) -lrdma -lrdmacm -libverbs $(LDFLAGS) 


	#$(CXX) -std=c++11 -g -Ofast -o $(addsuffix .normal, $@) $^ -L$(LIBSPDK_DIR) $(CFLAGS) $(DAX_OBJ) -L$(NVML_DIR) -lpmem -lpthread -lm -lrt -Wl,-rpath=$(abspath $(NVML_DIR))

#dmabench_lat: dmabench_lat.cc time_stat.o thread.cc
#	$(CXX) -std=c++11 -Ofast -g -o $@ $^ $(CFLAGS) -Wl,--whole-archive $(SPDK_LIBS) -Wl,--no-whole-archive -ldl -luuid -lpmem -lnuma -lpthread -lm -lrt $(LDFLAGS)

clean:
	rm -rf *.o $(EXE) *.normal

