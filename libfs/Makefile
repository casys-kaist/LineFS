########
#
#   Simplified Makefile, for each source we call a function (make-goal) to
#	compile, this is done in the last line of this file.
#
########
SYS ?= $(shell gcc -dumpmachine)
export SYS

####### Flags ##################################################################
### Print Flags
PRINT_FLAGS += -DPRINT_COLOR	# enable color scheme.
# PRINT_FLAGS += -DPRINT_POSIX
# PRINT_FLAGS += -DPRINT_REPLICATE
# PRINT_FLAGS += -DPRINT_LOG_PREFETCH
# PRINT_FLAGS += -DPRINT_DIGEST
# PRINT_FLAGS += -DPRINT_RPC
# PRINT_FLAGS += -DPRINT_REP_COALESCE
# PRINT_FLAGS += -DPRINT_SETUP
# PRINT_FLAGS += -DPRINT_DRAM_ALLOC
# PRINT_FLAGS += -DPRINT_RDMA
# PRINT_FLAGS += -DPRINT_META
# PRINT_FLAGS += -DMLFS_INFO
# PRINT_FLAGS += -DMLFS_DEBUG

### Profile flags.
# PROFILE_FLAGS = -DPROFILE_THPOOL
# PROFILE_FLAGS += -DPROFILE_LOG_COPY_BW

### Flags for experiments
# EXP_FLAGS = -DRECORD_OPS_FOR_HYPERLOOP_EXE # Disable HYPERLOOP flag for recording.
# EXP_FLAGS += -DNO_BUSY_WAIT # No busy waiting to measure cpu utilization.

### Common flags
COMMON_FLAGS = -DLIBFS
COMMON_FLAGS += -DDISTRIBUTED
COMMON_FLAGS += -DMASTER
COMMON_FLAGS += -DSEED
COMMON_FLAGS += -DLAZY_SURRENDER
COMMON_FLAGS += -DINVALIDATION
COMMON_FLAGS += -DKLIB_HASH
COMMON_FLAGS += -DNIC_SLAB_NCX
# COMMON_FLAGS += -DHYPERLOOP 	# Delegates replication to Hyperloop.
COMMON_FLAGS += $(PRINT_FLAGS)
COMMON_FLAGS += $(EXP_FLAGS)
COMMON_FLAGS += $(PROFILE_FLAGS)

### With NIC-offloading.
WITH_SNIC_FLAGS = $(COMMON_FLAGS)
WITH_SNIC_FLAGS += -DNIC_OFFLOAD
WITH_SNIC_FLAGS += -DLIMIT_PREFETCH 	  # Not working correctly if THREAD_NUM_LOG_PREFETCH is greater than 1.
WITH_SNIC_FLAGS += -DPIPELINE_RATE_LIMIT  # Not working correctly if THREAD_NUM_LOG_PREFETCH is greater than 1.
WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST

###############################################################################
#  Uncomment one of the followings.                                           #
###############################################################################
# MLFS_FLAGS = $(COMMON_FLAGS)  # Disable NIC-offloading (hostonly)
MLFS_FLAGS = $(WITH_SNIC_FLAGS) # Enable NIC-offloading
###############################################################################

# DEBUG = -g3 -O0
#DEBUG = -O0 -gdwarf-2 -g3 -rdynamic
# DEBUG = -g -O2
#DEBUG = -g -Ofast
DEBUG = -O3
################################################################################

DISTRO := $(shell lsb_release -si)

#CC=clang-3.8
#CC=gcc
CXX=g++ --std=c++11
CC=gcc -std=gnu11

########
#  List of all modules' directories
########
ifeq (,$(filter $(SYS), aarch64-redhat-linux))
# x86
MODULES   := io concurrency config filesystem global ds log mkfs \
	mlfs shim posix storage distributed experimental hyperloop
else
# aarch64
MODULES   := io concurrency config filesystem global ds log mkfs \
	mlfs posix storage distributed experimental
endif

########
#  List of all modules' directories
########

########
#  All modules' directories in src and build
########
SRC_DIR   := $(addprefix src/,$(MODULES))
BUILD_DIR := $(addprefix build/,$(MODULES))

########
#  Source and Object files in their  module directories
########
SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
SRCCPP    := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
SRC				:= $(filter-out src/mkfs/mkfs.c, $(SRC))
OBJ       := $(patsubst src/%.c,build/%.o,$(SRC))
OBJCPP    := $(patsubst src/%.cpp,build/%.o,$(SRCCPP))

########
#  Lib directories
########
NVML_DIR := $(abspath $(CURDIR)/lib/nvml/src)
JEMALLOC_DIR := $(abspath $(CURDIR)/lib/jemalloc-4.5.0)
RDMA_DIR := $(abspath $(CURDIR)/lib/rdma)
RDMA_CORE_DIR := $(abspath $(CURDIR)/lib/rdma-core)
HYPERLOOP_DIR := $(abspath $(CURDIR)/lib/hyperloop)

SYSCALL_INTERCEPT_DIR := $(abspath $(CURDIR)/lib/syscall_intercept/install)
ifeq (,$(filter $(DISTRO), Ubuntu))
    SYSCALL_INTERCEPT_LIB_DIR := $(SYSCALL_INTERCEPT_DIR)/lib64
else
    SYSCALL_INTERCEPT_LIB_DIR := $(SYSCALL_INTERCEPT_DIR)/lib
endif

INCLUDES  := $(addprefix -I,src/ $(RDMA_CORE_DIR)/build/include $(RDMA_DIR) $(NVML_DIR)/include $(SYSCALL_INTERCEPT_DIR)/include $(HYPERLOOP_DIR))

ifneq (,$(findstring -DHYPERLOOP,$(MLFS_FLAGS)))
    # With hyperloop
    $(info With Hyperloop library.)
    LD_FLAGS = -lpthread -laio -lstdc++ -lm -lnuma -L$(NVML_DIR)/nondebug/ -lpmem -lrt -L$(RDMA_CORE_DIR)/build/lib -Wl,-rpath=$(RDMA_CORE_DIR)/build/lib -L$(HYPERLOOP_DIR)/build/libhyperclient -Wl,-rpath=$(HYPERLOOP_DIR)/build/libhyperclient -lhyperclient #-Wl,-fuse-ld=gold
else
    # Without hyperloop
    $(info Without Hyperloop library.)
    LD_FLAGS = -lpthread -laio -lstdc++ -lm -lnuma -L$(NVML_DIR)/nondebug/ -lpmem -lrt -L$(RDMA_CORE_DIR)/build/lib -Wl,-rpath=$(RDMA_CORE_DIR)/build/lib #-Wl,-fuse-ld=gold
endif

# ############## No effect setting flags here. ##################################
# #MLFS_FLAGS = -DUSE_SLAB
# #MLFS_FLAGS += -DCONCURRENT
# #MLFS_FLAGS += -DLOG_OPT	// Not working.
# #MLFS_FLAGS += -DENABLE_PERF_MODEL
# #MLFS_FLAGS += -DUSE_LEASE
# #MLFS_FLAGS += -DLEASE_OPT
# #MLFS_FLAGS += -DNVM_READ_REDIRECT
# #MLFS_FLAGS += -DSSD_READ_REDIRECT
# #MLFS_FLAGS += -DHDD_READ_REDIRECT
# MLFS_FLAGS += -DCOALESCING
# #MLFS_FLAGS += -DUSE_SSD
# #MLFS_FLAGS += -DUSE_HDD
# #MLFS_FLAGS += -DMLFS_LOG
# ################################################################################

################################################################################
# SmartNIC related flags. DISTRIBUTED should be set.
#
# Flags:
# NIC_OFFLOAD : Enable KernFS offloading to SmartNIC.
#
# Flags not tested or not used:
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

define make-goal
$1/%.o: %.c
	$(CC) $(INCLUDES) $(MLFS_FLAGS) $(DEBUG) -fPIC -c $$< -o $$@ -DLIBFS
$1/%.o: %.cpp
	$(CXX) $(INCLUDES) $(MLFS_FLAGS) $(DEBUG) -fPIC -c $$< -o $$@ -DLIBFS
endef

########
#  Phony targets
########
.PHONY: all checkdirs clean mkfs buildarmlib

all: checkdirs libmlfs mkfs
checkdirs: $(BUILD_DIR)
	@mkdir -p bin

clean:
	@rm -rf $(BUILD_DIR)

########
#  Create dirs recursively
########
$(BUILD_DIR):
	@mkdir -p $@

########
#  Targets
########
ifeq (,$(filter $(SYS), aarch64-redhat-linux))
#build/libmlfs.a: $(OBJ)
libmlfs: $(OBJ) $(OBJCPP)
	ar cr build/libmlfs.a $(OBJ) $(OBJCPP)
	$(CC) -shared $(DEBUG) -o build/libmlfs.so $(OBJ) $(OBJCPP) $(LD_FLAGS) -DLIBFS $(MLFS_FLAGS) -L$(JEMALLOC_DIR)/lib -L$(RDMA_DIR) -Wl,-rpath=$(abspath $(RDMA_DIR)) -lrdma -Wl,-rpath=$(abspath $(JEMALLOC_DIR)/lib) -ljemalloc -lrdmacm -libverbs -lpthread -L$(SYSCALL_INTERCEPT_LIB_DIR) -Wl,-rpath=$(SYSCALL_INTERCEPT_LIB_DIR) -lsyscall_intercept -Wl,-rpath=$(abspath $(NVML_DIR)/nondebug)
else
#build/libmlfs.a: $(OBJ)
libmlfs: buildarmlib $(OBJ) $(OBJCPP)
	ar cr build/libmlfs.a $(OBJ) src/ds/lib/bitops_arch_arm64.o src/ds/lib/hweight.o $(OBJCPP)
	$(CC) -shared $(DEBUG) -o build/libmlfs.so $(OBJ) src/ds/lib/bitops_arch_arm64.o src/ds/lib/hweight.o $(OBJCPP) $(LD_FLAGS) -DLIBFS $(MLFS_FLAGS) -L$(JEMALLOC_DIR)/lib -L$(RDMA_DIR) -Wl,-rpath=$(abspath $(RDMA_DIR)) -lrdma -Wl,-rpath=$(abspath $(JEMALLOC_DIR)/lib) -ljemalloc -lrdmacm -libverbs -lpthread -L$(SYSCALL_INTERCEPT_LIB_DIR)
endif

mkfs: $(OBJ) libmlfs mkfs.c
	$(CC) -g -o build/mkfs.o -c src/mkfs/mkfs.c $(INCLUDES) $(MLFS_FLAGS) -lrdmacm -libverbs -lpthread
	$(CC) -g -o bin/mkfs.mlfs build/mkfs.o build/libmlfs.a $(INCLUDES) -L$(RDMA_DIR) $(LD_FLAGS) -DLIBFS -Wl,-rpath=$(abspath $(NVML_DIR)/nondebug) -Wl,-rpath=$(abspath $(RDMA_DIR)) -lrdma -lrdmacm -libverbs -lpthread


########
#  Compile each source into an object
########
$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))

buildarmlib:
	$(CC) -c src/ds/lib/bitops_arch_arm64.S -o src/ds/lib/bitops_arch_arm64.o
	$(CC) -Isrc -c src/ds/lib/hweight.c -o src/ds/lib/hweight.o
