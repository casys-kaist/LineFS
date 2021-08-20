BINARIES=lock_then_write lock_then_write.mlfs produce_then_consume produce_then_consume.mlfs driver produce_then_consume.mlfs.pthread produce_then_consume.pthread
ncores=$(shell grep -c ^processor /proc/cpuinfo)

CUR_DIR = $(shell pwd)/..
GLIBC_DIR = $(CUR_DIR)/../../shim/glibc-build/
LIBFS_DIR = $(CUR_DIR)/../build/
NVML_DIR = $(LIBFS_DIR)/../lib/nvml/src/nondebug/
CUCKOO_DIR = $(LIBFS_DIR)/../lib/cuckoofilter
LIBFS=$(LIBFS_DIR)/libmlfs.a
LIBSPDK_DIR =$(CUR_DIR)/../src/storage/spdk/
LIBSPDK=$(CUR_DIR)/../src/storage/spdk/libspdk.so
LIBSPDK_INC=$(CUR_DIR)/../src/storage/spdk/

INCLUDES= $(LIBFS_DIR)/../src -I$(CUR_DIR)/../lib/nvml/src/include
DAX_OBJ = ../build/storage/storage_dax.o

DPDK_VER := 16.07.2
DPDK_DIR := $(abspath $(CUR_DIR)/../lib/dpdk-$(DPDK_VER)/x86_64-native-linuxapp-gcc)
SPDK_ROOT_DIR := $(abspath $(CUR_DIR)/../lib/spdk)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
SPDK_LIBS += $(SPDK_ROOT_DIR)/build/lib/libspdk_nvme.a \
			 $(SPDK_ROOT_DIR)/build/lib/libspdk_util.a \
			 $(SPDK_ROOT_DIR)/build/lib/libspdk_log.a \
			 $(ENV_LIBS)

CFLAGS=-I $(LIBSPDK_INC) -I $(DPDK_DIR)/include -I$(INCLUDES)

LDFLAGS = -Wl,-rpath=$(abspath $(GLIBC_DIR)) \
		  -Wl,-rpath=$(abspath $(GLIBC_DIR))/rt \
		  -Wl,-rpath=$(abspath $(LIBFS_DIR)) \
		  -Wl,-rpath=$(abspath $(NVML_DIR)) \
		  -Wl,-rpath=$(abspath $(LIBSPDK_DIR)) \
		  -Wl,-rpath=/usr/local/lib \
		  -Wl,-rpath=/usr/lib/x86_64-linux-gnu/ \
		  -Wl,-rpath=/lib/x86_64-linux-gnu/ \
		  -Wl,-dynamic-linker=$(abspath $(GLIBC_DIR))/ld-linux-x86-64.so.2 \
		  -lpthread -lrt -lm -lssl -lcrypto

all: $(BINARIES)

%.mlfs: %.c sync_up.h
	gcc -g -Ofast -o $@ $<  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs \
		-L$(LIBSPDK_DIR)-lspdk -DMLFS $(CFLAGS) $(LDFLAGS) -l boost_system

%.mlfs.pthread: %.c sync_up.h
	gcc -g -Ofast -o $@ $<  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DUSE_PTHREAD\
		-L$(LIBSPDK_DIR)-lspdk -DMLFS $(CFLAGS) $(LDFLAGS) -l boost_system -pthread

%.pthread: %.c sync_up.h
	gcc -g -Ofast -o $@ $< -pthread -DUSE_PTHREAD

%: %.c sync_up.h
	gcc -g -Ofast -o $@ $< -pthread

driver: driver.c sync_up.h
	gcc -DN_CORES=$(ncores) -o $@ $< -pthread

clean:
	rm -f $(BINARIES) *.o
