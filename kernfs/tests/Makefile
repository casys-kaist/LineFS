SYS ?= $(shell gcc -dumpmachine)
export SYS

CC=gcc
CXX=g++ -std=c++11

#CC = clang-3.8
#CXX = clang++-3.8

########
#  Lib directories
########
LIBFS_DIR := $(CURDIR)/../../libfs
KERNFS_DIR := $(CURDIR)/../
NVML_DIR := $(abspath $(LIBFS_DIR)/lib/nvml/src)
DPDK_DIR := $(abspath $(LIBFS_DIR)/lib/dpdk-16.07/x86_64-native-linuxapp-gcc)
SPDK_DIR := $(abspath $(KERNFS_DIR)/lib/spdk)
CUCKOO_DIR := $(abspath $(LIBFS_DIR)/lib/cuckoofilter)

INCLUDES  := $(addprefix -I, .. . $(NVML_DIR)/include $(CUCKOO_DIR)/src)

ifeq (,$(filter $(SYS), aarch64-linux-gnu))
    LD_FLAGS = -lpthread -lm -L$(NVML_DIR)/nondebug/ -lpmem -lrt
    BUILD_REL := build
else
    LD_FLAGS = -lpthread -lm -L$(NVML_DIR)/nondebug/ -lrt
    BUILD_REL := buildarm
endif

LD_FLAGS_CXX = -lpthread -L$(NVML_DIR)/nondebug/ -lpmem -lrt
#DEBUG = -g -O0
# DEBUG = -g -Ofast
DEBUG = -O3

########
#  Phony targets
########
.PHONY: kernfs

BIN := kernfs fifo_cli

all: $(BIN)

clean:
	rm -rf *.o $(BIN)

kernfs: kernfs.c
	$(CC) $^ $(DEBUG) -o $@ $(INCLUDES) -L../$(BUILD_REL) -lkernfs $(LD_FLAGS) -Wl,-rpath=$(abspath $(NVML_DIR)/nondebug) -Wl,--whole-archive $(SPDK_LIBS) -Wl,--no-whole-archive -luuid -lnuma 

extent_test: extent_test.cc
	$(CXX) -std=c++11 $(DEBUG) $^ -o $@ $(INCLUDES) -L../$(BUILD_REL) -lkernfs $(LD_FLAGS_CXX) -DUSE_SLAB -Wl,-rpath=$(abspath ../$(BUILD_REL)) -Wl,-rpath=$(abspath $(NVML_DIR)/nondebug)

fifo_cli: fifo_cli.c
	$(CC) -o $@ $^
