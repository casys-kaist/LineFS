# Makefile template for shared library

CC = gcc -std=gnu11 -g -D_GNU_SOURCE # C compiler
RDMA_DIR := $(abspath $(CURDIR)/..)
NVML_DIR := $(abspath $(CURDIR)/../../nvml/src)
INCLUDES  := -I$(RDMA_DIR) -I.

LIBS = -L$(RDMA_DIR) -lrdma -Wl,-rpath=$(abspath $(RDMA_DIR)) -L$(NVML_DIR)/nondebug -lpmem -Wl,-rpath=$(abspath $(NVML_DIR)/nondebug) -lm
INCLUDES  := $(addprefix -I,src/ $(RDMA_CORE_DIR)/build/include $(RDMA_DIR) $(NVML_DIR)/include)

RM = rm -f  # rm command
#SRCS = master_sync.c slave_sync.c master_passive.c slave_passive.c # source files
DEPS = time_stat.c
#SRCS = master_sync.c slave_passive.c rpc_client.c rpc_server.c # FIXME: tempoarily removing some srcs
SRCS = rpc_client.c rpc_server.c write_bw.c write_lat.c read_lat.c read_bw.c # FIXME: tempoarily removing some srcs
OBJS = $(SRCS:.c=.o)
OBJS += $(DEPS:.c=.o)
BIN = $(SRCS:.c=)

.PHONY: clean

all: $(BIN)

$(BIN):$(OBJS)
		$(CC) $@.o time_stat.o ${LDFLAGS} ${LIBS} -o $@

%.o: %.c
		$(CC) $(INCLUDES) -c $(CFLAGS) -o $@ $<

clean:
		rm -f $(OBJS) $(BIN)
