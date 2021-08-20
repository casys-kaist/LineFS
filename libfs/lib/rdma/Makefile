SYS ?= $(shell gcc -dumpmachine)

#CC = gcc #-D_GNU_SOURCE # C compiler
CC = gcc -std=gnu11
#CFLAGS = -fPIC -Wall -Wextra -O2 -g # C flags
CFLAGS = -fPIC -O2 -g
# CFLAGS = -fPIC -O0 -g
CFLAGS += -I../rdma-core/build/include
LDFLAGS = -shared  # linking flags

RM = rm -f  # rm command
TARGET_LIB = librdma.so # target lib
SRCS = agent.c connection.c verbs.c mr.c messaging.c # source files
OBJS = $(SRCS:.c=.o)

# Flags for Bluefield NIC.
ifeq (aarch64-linux-gnu,$(SYS))
    NIC_OFFLOAD_FLAGS = -DNIC_OFFLOAD
else
    NIC_OFFLOAD_FLAGS =
endif

#CPPFLAGS += -DSANITY_CHECK
#CPPFLAGS += -DDEBUG
#CPPFLAGS += -DPRINT_ASYNC_EVENT	# To check CQ overflow. With this print, ASYNC event is printed.
CPPFLAGS += -DIBV_RATE_LIMITER
# CPPFLAGS += -DCHECK_SEQN_INCREASE_BY_ONE 	# To check sequence number always increases by one.
CPPFLAGS += -DCHECK_DUP_MSG
# CPPFLAGS += -DDIE_ON_DUP_MSG
CPPFLAGS += -DCHECK_INCORRECT_SEQN
# CPPFLAGS += -DDIE_ON_INCORRECT_SEQN
# CPPFLAGS += -DPRINT_PREV_MSG
CPPFLAGS += -DMSG_BUFFER_PROFILE
CPPFLAGS += -DIBV_WRAPPER_INLINE
CPPFLAGS += -D_GNU_SOURCE
CPPFLAGS += -fvisibility=default
CPPFLAGS += $(NIC_OFFLOAD_FLAGS)

LDFLAGS +=  -L../rdma-core/build/lib -Wl,-rpath=$(abspath ../rdma-core/build/lib)
LDLIBS = -lrdmacm -libverbs -lpthread

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
		$(CC) -g -o $@ $^ ${LDFLAGS} ${LDLIBS}

#$(SRCS:.c=.d):%.d:%.c
#		$(CC) $(CFLAGS) -MM $< >$@

#include $(SRCS:.c=.d)

.PHONY: clean
clean:
	${RM} ${TARGET_LIB} ${OBJS}

