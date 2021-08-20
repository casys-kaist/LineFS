#####################################################################################
# MLFS build setup
CUR_DIR = $(shell pwd)
SRC_ROOT = $(abspath ../../)
GLIBC_DIR = $(SRC_ROOT)/shim/glibc-build/
LIBFS_DIR = $(SRC_ROOT)/libfs/build/
NVML_DIR = $(SRC_ROOT)/libfs/lib/nvml/src/nondebug/
LIBSPDK_DIR = $(SRC_ROOT)/libfs/src/storage/spdk/
LIBSPDK_INC= $(SRC_ROOT)/libfs/src/storage/spdk/
MLFS_INCLUDES= $(LIBFS_DIR)/../src

DPDK_DIR := $(abspath $(SRC_ROOT)/libfs/lib/dpdk-16.07/x86_64-native-linuxapp-gcc)
SPDK_ROOT_DIR := $(abspath $(SRC_ROOT)/libfs/lib/spdk)
# This cause problems.
#include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
SPDK_LIBS += $(SPDK_ROOT_DIR)/build/lib/libspdk_nvme.a \
             $(SPDK_ROOT_DIR)/build/lib/libspdk_util.a \
             $(SPDK_ROOT_DIR)/build/lib/libspdk_log.a \
             $(ENV_LIBS)

CFLAGS += -g -O3 -I$(LIBSPDK_INC) -I$(MLFS_INCLUDES) -I$(DPDK_DIR)/include
#CXXFLAGS += -I $(LIBSPDK_INC) -I $(MLFS_INCLUDES) -I $(DPDK_DIR)/include

MLFS_LDFLAGS = -Wl,-rpath=$(abspath $(GLIBC_DIR)) \
               -Wl,-rpath=$(abspath $(GLIBC_DIR))/rt \
               -Wl,-rpath=$(abspath $(LIBFS_DIR)) \
               -Wl,-rpath=$(abspath $(NVML_DIR)) \
               -Wl,-rpath=$(abspath $(LIBSPDK_DIR)) \
               -Wl,-rpath=/usr/local/lib \
               -Wl,-rpath=/usr/lib \
               -Wl,-rpath=/usr/lib/x86_64-linux-gnu/ \
               -Wl,-rpath=/lib/x86_64-linux-gnu/ \
               -Wl,-dynamic-linker=$(abspath $(GLIBC_DIR))/ld-linux-x86-64.so.2 \
               -L$(LIBFS_DIR)  \
               -L$(LIBSPDK_DIR)  \
               -lmlfs -lpthread -lrt -lspdk -lm
#####################################################################################

all: rsocket rsocket.mlfs rstream

rsocket: rstream.c common.c
	gcc -g -O3 -o rsocket rstream.c common.c -lrdmacm -libverbs

rsocket.mlfs: rstream.c common.c
	gcc $(CFLAGS) -o rsocket.mlfs rstream.c common.c  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(LIBSPDK_DIR) -lspdk -DMLFS $(CFLAGS) $(MLFS_LDFLAGS) -lrdmacm -libverbs -DMLFS

rstream: riostream.c common.c
	gcc -g -O3 -o rio riostream.c common.c -lrdmacm -libverbs

clean:
	rm -f *.o
	rm -f rio
	rm -f rsocket rsocket.mlfs
