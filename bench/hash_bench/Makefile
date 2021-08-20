CC = gcc -std=c99
EXE = hash_bench
#$(info $(EXE))

CUR_DIR = $(shell pwd)
PROJECT_ROOT =$(abspath $(CUR_DIR)../../../)
GLIBC_DIR = $(abspath $(PROJECT_ROOT)/shim/glibc-build/)
LIBFS_DIR = $(abspath $(PROJECT_ROOT)/libfs/build/)
NVML_DIR = $(abspath $(PROJECT_ROOT)/libfs/lib/nvml/src/nondebug/)
LIBCUCKOO_DIR = $(abspath $(CUR_DIR)/libcuckoo)
CITYHASH_DIR = $(abspath $(CUR_DIR)/cityhash/src)
JUDY_DIR = $(abspath $(CUR_DIR)/judy-1.0.5/src)
GLIB_DIR = $(abspath $(CUR_DIR)/glib)

INCLUDES = $(addprefix -I, $(LIBCUCKOO_DIR) $(GLIB_DIR) $(GLIB_DIR)/glib $(CITYHASH_DIR) $(JUDY_DIR))

all: $(EXE)

%.o: %.c
	$(CC) $(CFLAGS) -c -g $< -o $@

%.o: %.cc
	$(CXX) -std=c++11 $(CFLAGS) -c -g $< -o $@ $(INCLUDES)

time_stat.o: time_stat.c
	$(CC) time_stat.c -c -o time_stat.o -D_BSD_SOURCE

hash_bench: hash_bench.o time_stat.o xxhash.o
	$(CXX) -std=c++11 $(INCLUDES) -g -o $@ $^ -lrt -lpthread  -Wl,-rpath=$(abspath $(GLIB_DIR)/glib/.libs) -L$(abspath $(GLIB_DIR)/glib/.libs) -lglib-2.0 -Wl,-rpath=$(abspath $(CITYHASH_DIR)/.libs) -L$(abspath $(CITYHASH_DIR)/.libs) -lcityhash  -L$(abspath $(JUDY_DIR)/obj/.libs) -lJudy -Wl,-rpath=$(abspath $(JUDY_DIR)/obj/.libs)

clean:
	rm -rf $(EXE) *.o
