########
#
#   Simplified Makefile, for each source we call a function (make-goal) to
#	compile, this is done in the last line of this file.
#
########
SYS ?= $(shell gcc -dumpmachine)
export SYS

### Select distributed file system. 'assise' or 'linefs'
DFS = linefs

####### Flags ##################################################################
### Print flags
PRINT_FLAGS = -DPRINT_COLOR	# enable color scheme.
# PRINT_FLAGS += -DPRINT_PIPELINE
# PRINT_FLAGS += -DPRINT_REPLICATE
# PRINT_FLAGS += -DPRINT_LOG_PREFETCH
# PRINT_FLAGS += -DPRINT_LOGHDRS
# PRINT_FLAGS += -DPRINT_DIGEST
# PRINT_FLAGS += -DPRINT_RPC
# PRINT_FLAGS += -DPRINT_SETUP
# PRINT_FLAGS += -DPRINT_RDMA
# PRINT_FLAGS += -DPRINT_META
# PRINT_FLAGS += -DPRINT_DRAM_ALLOC
# PRINT_FLAGS += -DPRINT_CIRCBUF
# PRINT_FLAGS += -DTHPOOL_DEBUG
# PRINT_FLAGS += -DMLFS_INFO
# PRINT_FLAGS += -DMLFS_DEBUG

### Debug flags.
# DBG_FLAGS = -DVALIDATE_LOGHDRS
# DBG_FLAGS += -DDIGEST_MEMCPY_NOOP # Not working currently. We need to implement sending ack to libfs.
# DBG_FLAGS += -DDIGEST_MEMCPY_NO_COPY
# DBG_FLAGS += -DSIGNAL_HANDLER 	# Enable it to grab signal. Example usages: mutrace, PROFILE_THPOOL, ...
# DBG_FLAGS += -DCHECK_LOGHDR_LIST
# DBG_FLAGS += -DCHECK_SANITY_MEMCPY_LIST_BUF # Zeroing loghdr buffer after used.

### Profile flags.
# PROFILE_FLAGS = -DPROFILE_THPOOL # To use it enable SIGNAL_HANDLER.
# PROFILE_FLAGS += -DPROFILE_PIPELINE
# PROFILE_FLAGS += -DPROFILE_CIRCBUF
# PROFILE_FLAGS += -DPROFILE_REALTIME_NET_BW_USAGE
# PROFILE_FLAGS += -DPROFILE_REALTIME_MEMCPY_BW
# PROFILE_FLAGS += -DPROFILE_REALTIME_FETCH_LOG_FROM_PRIMARY
# PROFILE_FLAGS += -DPROFILE_REALTIME_FETCH_LOG_FROM_LOCAL
# PROFILE_FLAGS += -DPROFILE_REALTIME_COMPRESS_BW
# PROFILE_FLAGS += -DPROFILE_NIC_SLAB_STAT
# PROFILE_FLAGS += -DPROFILE_JOBQUEUE_LEN

### Flags for experiments
EXP_FLAGS = -DSIGNAL_WITH_NFS
# EXP_FLAGS += -DNO_BUSY_WAIT # No busy waiting to measure cpu utilization.
# EXP_FLAGS += -DNO_NON_TEMPORAL_WRITE # Use memcpy rather than non-temporal write(pmem_memmove_persist) (Host-only)

### Common flags.
COMMON_FLAGS = -DKERNFS
COMMON_FLAGS += -DDISTRIBUTED
# COMMON_FLAGS += -DENABLE_PERF_MODEL # related to lease?
# COMMON_FLAGS += -DUSE_LEASE
# COMMON_FLAGS += -DLEASE_DEBUG
# COMMON_FLAGS += -DLEASE_MIGRATION
COMMON_FLAGS += -DBALLOC
COMMON_FLAGS += -DDIGEST_OPT 	# coalescing.
COMMON_FLAGS += -DCONCURRENT
COMMON_FLAGS += -DNIC_SLAB_NCX
# COMMON_FLAGS += -DHYPERLOOP 	# Delegates replication to Hyperloop.
COMMON_FLAGS += $(PRINT_FLAGS)
COMMON_FLAGS += $(DBG_FLAGS)
COMMON_FLAGS += $(PROFILE_FLAGS)
COMMON_FLAGS += $(EXP_FLAGS)

### Host flags.
HOST_FLAGS = -DMASTER 	# Set on Host, unset on NIC.
# HOST_FLAGS += -DPROFILE_LOG_COPY_BW

### With NIC offloading. Flags for both host and NIC.
WITH_SNIC_COMMON_FLAGS = $(COMMON_FLAGS)
WITH_SNIC_COMMON_FLAGS += -DNIC_OFFLOAD
WITH_SNIC_COMMON_FLAGS += -DDIGEST_OPT_MERGE_WRITES
# WITH_SNIC_COMMON_FLAGS += -DCOMPRESS_LOG
WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST
# WITH_SNIC_COMMON_FLAGS += -DPREFETCH_FLOW_CONTROL # To prevent from running out of NIC memory. It limits prefetching bandwidth. Use this feature only for debugging or investigation because it drops throughput.
# WITH_SNIC_COMMON_FLAGS += -DNO_PIPELINING  # Naive offloading without pipelining
# WITH_SNIC_COMMON_FLAGS += -DNO_PIPELINING_BG_COPY  # Copy to local NVM in background.
WITH_SNIC_COMMON_FLAGS += -DPIPELINE_RATE_LIMIT  # Enable rate limiter in LineFS.
# WITH_SNIC_COMMON_FLAGS += -DBACKUP_RDMA_MEMCPY # Use RDMA memcpy on Replica1's host failure.

### With NIC offloading. Flags for host.
WITH_SNIC_HOST_FLAGS = $(WITH_SNIC_COMMON_FLAGS)
# WITH_SNIC_HOST_FLAGS += -DIOAT_INTERRUPT_KERNEL_MODULE
WITH_SNIC_HOST_FLAGS += $(HOST_FLAGS)

### Flags for Smart NIC
WITH_SNIC_NIC_FLAGS = $(WITH_SNIC_COMMON_FLAGS)
WITH_SNIC_NIC_FLAGS += -DNIC_SIDE
# WITH_SNIC_NIC_FLAGS += -DPER_LIBFS_REPLICATION_THREAD # Disable it when m_to_n_rep_thread is enabled. # Deprecated.
# WITH_SNIC_NIC_FLAGS += -DPER_LIBFS_PREFETCH_THREAD # Deprecated.
# WITH_SNIC_NIC_FLAGS += -DCOALESCING  # Not supported yet.

ifeq (,$(filter $(SYS), aarch64-linux-gnu))
# x86

ifeq (,$(filter $(DFS), assise))
# LineFS
    MLFS_FLAGS = $(WITH_SNIC_HOST_FLAGS) 	# Enable NIC-offloading
else
# Assise
    MLFS_FLAGS = $(COMMON_FLAGS) $(HOST_FLAGS)  # Disable NIC-offloading (hostonly)
endif

else
# ARM
    MLFS_FLAGS = $(WITH_SNIC_NIC_FLAGS) 	# It is NIC kernfs.
endif

### Debug options.
# DEBUG = -g -Og
# DEBUG = -g -O0 -DNO_COMPILETIME_ASSERT_IN_ARM  # Use it when -g -Og fails.
# DEBUG = -g3 -Ofast
DEBUG = -O3
################################################################################

ifeq (,$(filter $(SYS), aarch64-linux-gnu))
    BUILD_REL := build
    SUBDIR := io cache concurrency global ds mlfs storage filesystem distributed experimental hyperloop .
else
    BUILD_REL := buildarm
    SUBDIR := io cache concurrency global ds mlfs storage filesystem distributed experimental hyperloop nic .
endif

SRC_DIR   := $(addprefix , $(SUBDIR))
BUILD_DIR := $(addprefix $(BUILD_REL)/, $(SUBDIR))
LIBFS_DIR := $(CURDIR)/../libfs

########
#  Source and Object files in their  module directories
########
SRC	:= $(foreach sdir, $(SRC_DIR), $(wildcard $(sdir)/*.c))
SRCCPP	:= $(foreach sdir, $(SRC_DIR), $(wildcard $(sdir)/*.cpp))
SRC := $(filter-out %mkfs.c %kernf.c,$(SRC))
OBJ	:= $(patsubst %.c,$(BUILD_REL)/%.o,$(SRC))
OBJCPP	:= $(patsubst %.cpp,$(BUILD_REL)/%.o,$(SRCCPP))
HDER := $(foreach sdir, $(SRC_DIR), $(wildcard $(sdir)/*.h)) $(wildcard storage/aio/*.h)

########
#  Lib directories
########
NVML_DIR := $(abspath $(LIBFS_DIR)/lib/nvml/src)
DPDK_DIR := $(abspath $(LIBFS_DIR)/lib/dpdk-16.07/x86_64-native-linuxapp-gcc)
SPDK_ROOT_DIR := $(abspath $(CURDIR)/lib/spdk)
CUCKOO_FILTER_DIR := $(abspath $(LIBFS_DIR)/lib/cuckoofilter)
CUCKOO_DIR := $(abspath $(LIBFS_DIR)/lib/libcuckoo)
GLIB_DIR := $(abspath $(LIBFS_DIR)/lib/glib)
#JEMALLOC_DIR := $(abspath $(LIBFS_DIR)/lib/jemalloc-4.5.0)
RDMA_DIR := $(abspath $(LIBFS_DIR)/lib/rdma)
LZ4_DIR := $(abspath $(CURDIR)/lib/simple-lz4)

INCLUDES  := $(addprefix -I, . $(NVML_DIR)/include $(CUCKOO_DIR) $(CUCKOO_FILTER_DIR)/src $(GLIB_DIR)/ $(GLIB_DIR)/glib $(RDMA_DIR) )
LD_FLAGS = -lpthread -laio -lstdc++ -lm -L$(NVML_DIR)/nondebug/ -lrt -L$(RDMA_DIR) -Wl,-rpath=$(abspath $(RDMA_DIR)) -lrdma -ldl -luuid -lnuma

ifeq (,$(filter $(SYS), aarch64-linux-gnu))
	# x86
LD_FLAGS += -lpmem -L$(SPDK_ROOT_DIR) -Wl,-rpath=$(abspath $(SPDK_ROOT_DIR)) -lnuma
INCLUDES += -I$(SPDK_ROOT_DIR)/include

include $(SPDK_ROOT_DIR)/mk/spdk.common.mk

SPDK_LIBS += $(SPDK_ROOT_DIR)/build/lib/libspdk_ioat.a \
	     	 $(SPDK_ROOT_DIR)/build/lib/libspdk_util.a \
	    	 $(SPDK_ROOT_DIR)/build/lib/libspdk_log.a \
	    	 $(ENV_LIBS)

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk
else
	#ARM
LD_FLAGS += -llz4 -L$(LZ4_DIR)/lib
INCLUDES += -I$(LZ4_DIR)/lib
endif
############### No effect setting flags here. ##################################
##MLFS_FLAGS = -DUSE_SLAB
##MLFS_FLAGS += -DENABLE_PERF_MODEL
##MLFS_FLAGS += -DUSE_LEASE
##MLFS_FLAGS += -DLEASE_OPT
##MLFS_FLAGS += -DACTIVE
##MLFS_FLAGS += -DIOMERGE
##MLFS_FLAGS += -DFCONCURRENT
##MLFS_FLAGS += -DUSE_SSD
##MLFS_FLAGS += -DUSE_HDD
##MLFS_FLAGS += -DMIGRATION
##MLFS_FLAGS += -DEXPERIMENTAL
#################################################################################

################################################################################
# SmartNIC related flags. DISTRIBUTED should be set.
#
# Flags:
# NIC_OFFLOAD : Enable KernFS offloading to SmartNIC.
# NIC_SIDE : Indicates it is SmartNIC. If not set, it indicates host side. MASTER
# 	should be disabled if NIC_SIDE is set.
# DIGEST_OPT_MERGE_WRITES : Use RDMA read and write for host memcpy. DIGEST_OPT
# 	flag is required (Currently, it supports DIGEST_OPT case only.) With this
# 	flag, Assise builds rdma_entries for a bunch of RDMA memcpys and triggers
# 	them when the number of entries exceeds a threshold.
#
# Flags not tested or not used:
# NVM_WRITE_CONCURRENT : Use multiple threads for NVM write on Host. (Writes are
# 	triggered by RPC from Kernfs on NIC.) It doesn't affect when Assise does
# 	RDMA memcpy. Not tested.
#
# Flags for experiments:
# DIGEST_BREAKDOWN : For breakdown digest latency. It should be set on both LibFS
# 	and Kenrfs on NIC.
# REPLICATION_BREAKDOWN	: For breakdown replication latency. It should be set on
# 	both LibFS and Kernfs of replicas.
################################################################################

########
#  vpath and compile function for each file
########
vpath %.c $(SRC_DIR)
vpath %.cpp $(SRC_DIR)

#CC = clang-3.8
CC = gcc -std=gnu11
CXX = g++ -std=c++11

define make-goal
$1/%.o: %.c
	$(CC) $(INCLUDES) $(DEBUG) -fPIC -c $$< -o $$@ -DKERNFS $(MLFS_FLAGS)
$1/%.o: %.cpp
	$(CXX) $(INCLUDES) $(DEBUG) -fPIC -c $$< -o $$@ -DKERNFS $(MLFS_FLAGS)
endef

########
#  Phony targets
########
.PHONY: all checkdirs clean filesystem buildarmlib

all: $(BUILD_DIR) filesystem
ifeq (,$(filter $(SYS), aarch64-redhat-linux))
clean:
	@rm -rf ./$(BUILD_REL)
else
clean:
	@rm -rf ./$(BUILD_REL) ./ds/lib/bitops_arch_arm64.o ./ds/lib/hweight.o
endif

########
#  Create dirs recursively
########
$(BUILD_DIR):
	@mkdir -p $@

########
#  Targets
########
ifeq (,$(filter $(SYS), aarch64-linux-gnu))
filesystem: $(OBJ) $(OBJCPP) $(SRC) $(HDER)
	$(CC) -shared $(DEBUG) -o $(BUILD_REL)/libkernfs.so $(OBJ) $(OBJCPP) -Wl,--whole-archive $(SPDK_LIBS) -Wl,--no-whole-archive $(LD_FLAGS) $(MLFS_FLAGS)
else
filesystem: buildarmlib $(OBJ) $(OBJCPP) $(SRC) $(HDER)
	$(CC) -shared $(DEBUG) -o $(BUILD_REL)/libkernfs.so $(OBJ) ds/lib/bitops_arch_arm64.o ds/lib/hweight.o $(OBJCPP) -Wl,--whole-archive -Wl,--no-whole-archive $(LD_FLAGS) $(MLFS_FLAGS)
endif

########
#  Compile each source into an object
########
$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))

buildarmlib:
	$(CC) -c ds/lib/bitops_arch_arm64.S -o ds/lib/bitops_arch_arm64.o
	$(CC) -I$(LIBFS_DIR)/src -c ds/lib/hweight.c -o ds/lib/hweight.o
