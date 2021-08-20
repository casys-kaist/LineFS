CC = gcc -std=c99
#CC = c99
EXE = iotest file_basic small_io falloc_test ftrunc_test dir_test many_files_test fork_io readdir_test statdir_test append_test fwrite_fread partial_update_test iobench_lat iobench write_read
#$(info $(EXE))

CUR_DIR = $(shell pwd)
#GLIBC_DIR = $(CUR_DIR)/../../../shim/glibc-build/

LDFLAGS = -Wl,-rpath=$(abspath $(GLIBC_DIR)) \
		  -Wl,-rpath=/usr/lib \
		  -Wl,-rpath=/usr/local/lib \
		  -Wl,-rpath=/usr/lib/x86_64-linux-gnu/ \
		  -Wl,-rpath=/lib/x86_64-linux-gnu/ \
		  -lpthread -lrt -lm -lssl -lcrypto

all: $(EXE)

%.o: %.c
	$(CC) $(CFLAGS) -c -g $< -o $@

%.o: %.cc
	$(CXX) -std=c++11 $(CFLAGS) -c -g $< -o $@

time_stat.o: time_stat.c
	$(CC) time_stat.c -c -o time_stat.o -D_BSD_SOURCE

file_basic: file_basic.c
	$(CC) -g -o $@ $^ -DDMFS $(LDFLAGS)

small_io: small_io.c
	$(CC) -g -o $@ $^ -DDMFS $(LDFLAGS)

append_test: append_test.c
	$(CC) -g -o $@ $^ -DDMFS $(LDFLAGS)

falloc_test: falloc_test.c
	$(CC) -g -o $@ $^ -DDMFS $(LDFLAGS)

ftrunc_test: ftrunc_test.c
	$(CC) -g -o $@ $^ -DDMFS $(LDFLAGS)

readdir_test: readdir_test.c
	$(CC) -g -o $@ $^ -DDMFS $(LDFLAGS)

statdir_test: statdir_test.c
	$(CC) -g -o $@ $^ -DDMFS $(LDFLAGS)

write_read: write_read.c time_stat.o
	$(CC) -g -o $(CC) -g -o $@ $^ -DDMFS $(LDFLAGS)

iotest: iotest.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -g -Ofast -o $@ $^ -DDMFS $(LDFLAGS)

fwrite_fread: fwrite_fread.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -g -Ofast -o $@ $^ -DDMFS $(LDFLAGS)

dir_test: dir_test.c
	$(CC) -O0 -g -o $@ $^ -DDMFS $(LDFLAGS)

many_files_test: many_files_test.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -O2 -g -o $@ $^ -DDMFS $(LDFLAGS)

partial_update_test: partial_update_test.cc
	$(CXX) -std=c++11 -g -O0 -o $@ $^ -DDMFS $(LDFLAGS)
	#$(CXX) -std=c++11 -g -o $@ $^  -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -L$(LIBSPDK_DIR) -lspdk -DMLFS $(CFLAGS) $(LDFLAGS) 

fork_io: fork_io.cc time_stat.o 
	$(CXX) -std=c++11 -g -o $@ $^ -DDMFS $(LDFLAGS)

iobench_lat: iobench_lat.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -O2 -g -o $@ $^ -DDMFS $(LDFLAGS)

iobench: iobench.cc time_stat.o thread.cc
	$(CXX) -std=c++11 -O2 -g -o $@ $^ -DDMFS $(LDFLAGS)

clean:
	rm -rf *.o $(EXE) *.normal

