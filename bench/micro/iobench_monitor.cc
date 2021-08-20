#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <random>
#include <memory>
#include <fstream>

#if 0
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/math/distributions/pareto.hpp>
#include <boost/random.hpp>
#endif

#ifdef MLFS
#include <mlfs/mlfs_interface.h>
#endif


#include "time_stat.h"
#include "thread.h"

#ifdef MLFS
const char *test_dir_prefix = "/mlfs/";
#else
const char *test_dir_prefix = "./pmem";
//const char test_dir_prefix[] = "./ssd";
#endif

char *test_file_name = "testfile";

//uncomment below to run strawman
//#define USER_BLOCK_MIGRATION 1

#ifdef USER_BLOCK_MIGRATION
#include "batch_migration.h"
#define open(x,y,z) bb_open(x,y,z)
//#define open(x,y) bb_open(x,y)
#define read(x,y,z) bb_read(x,y,z)
#define write(x,y,z) bb_write(x,y,z)
#define pread(x,y,z,w) bb_pread(x,y,z,w)
#define fsync(x) bb_fsync(x)
#endif

#define ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define ALIGN_MASK_FLOOR(x, mask) (((x)) & ~(mask))
#define ALIGN(x, a)  ALIGN_MASK((x), ((__typeof__(x))(a) - 1))
#define ALIGN_FLOOR(x, a)  ALIGN_MASK_FLOOR((x), ((__typeof__(x))(a) - 1))
#define BUF_SIZE (2 << 20)

//#define ODIRECT
#undef ODIRECT
//#define VERIFY

typedef enum {SEQ_WRITE, SEQ_READ, SEQ_WRITE_READ, RAND_WRITE, RAND_READ,
	ZIPF_WRITE, ZIPF_READ, ZIPF_MIX, NONE} test_t;

typedef enum {FS} test_mode_t;

typedef unsigned long addr_t;

static pthread_barrier_t tsync;
uint8_t dev_id;
unsigned long ops_cap;
int psync;   //process barrier
int quit_sync;   //process barrier
static unsigned int *shm_proc_sync;
static unsigned int *shm_proc_inf;  // Keep running infinitely.
static unsigned int *shm_proc_epoch;
int wait_signal;
int run_inf; // infinite mode.
int do_fsync;

// For monitoring.
int proc_id;    // process id.
int timer_mode; // Timer process. It will set next_epoch bits periodically.
#define MON_EPOCH_IN_MILLISECOND 1000

void start_mon_timer(void)
{
    int epoch_cnt = 0;
    cerr << "[iob_mon] Timer thread begin." << endl;
    while (1) {
        usleep(MON_EPOCH_IN_MILLISECOND * 1000);
        cerr << "[iob_mon timer] "<< epoch_cnt <<" *shm_proc_epoch:" << hex << *shm_proc_epoch << dec << " -> ";
        *shm_proc_epoch = ~(0UL); // Set all bit to 1.
        cerr << hex << *shm_proc_epoch << dec << endl;
	epoch_cnt++;
    }
    cerr << "[iob_mon] Timer thread end." << endl;
}

class io_bench : public CThread
{
	public:
		io_bench(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
				test_t _test_type, string _zipf_file);

		int id, fd, per_thread_stats;
		unsigned long file_size_bytes;
		unsigned int io_size;
		test_t test_type;
		string test_file;
		string zipf_file;
		char *buf;
		struct time_stats stats;

                std::list<uint64_t> io_list;
                std::list<uint8_t> op_list;

		pthread_cond_t cv;
		pthread_mutex_t cv_mutex;

		void prepare(void);
		void cleanup(void);

		void do_read(void);
		void do_write(void);

		// Thread entry point.
		void Run(void);

		// util methods
		static unsigned long str_to_size(char* str);
		static test_t get_test_type(char *);
		static test_mode_t get_test_mode(char *);
		static void hexdump(void *mem, unsigned int len);
		static void show_usage(const char *prog);
};

io_bench::io_bench(int _id, unsigned long _file_size_bytes,
		unsigned int _io_size, test_t _test_type, string _zipf_file)
	: id(_id), file_size_bytes(_file_size_bytes), io_size(_io_size),
	test_type(_test_type), zipf_file(_zipf_file)
{
	test_file.assign(test_dir_prefix);
	//test_file += "/file" + std::to_string(dev_id) + "-" + std::to_string(id) + std::to_string(getpid());

	test_file += "/" + std::string(test_file_name) + std::to_string(0) + "-" + std::to_string(dev_id);
	per_thread_stats = 0;
}

#define handle_error_en(en, msg) \
	do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define SHM_PATH_SYNC "/iobench_shm_sync"
#define SHM_PATH_EPOCH "/iobench_shm_epoch"
#define SHM_F_SIZE 128

void* create_shm(int &fd, int &res, char* path) {
	void * addr;
	// fd = shm_open(SHM_PATH, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	fd = shm_open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		exit(-1);
	}

	res = ftruncate(fd, SHM_F_SIZE);
	if (res < 0)
	{
		exit(-1);
	}

	addr = mmap(NULL, SHM_F_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED){
		exit(-1);
	}

	return addr;
}

void destroy_shm(void *addr, char* path) {
	int ret, fd;
	ret = munmap(addr, SHM_F_SIZE);
	if (ret < 0)
	{
		exit(-1);
	}

	// fd = shm_unlink(SHM_PATH);
	fd = shm_unlink(path);
	if (fd < 0) {
		exit(-1);
	}
}

unsigned int next_epoch_started (void)
{
    unsigned int ret = (((unsigned int)*shm_proc_epoch) >> proc_id) & 1UL;
    // if (ret)
        // cerr <<"[iob_mon] ("<< __func__<< ") *shm_proc_epoch(" << hex << shm_proc_epoch << "): " <<*shm_proc_epoch<< dec << ", get_bit: " << ret << endl;
        // usleep(500*1000);
    return ret;
}

void clear_next_epoch_bit (void)
{
    // cerr <<"[iob_mon] ("<< __func__<< ") *shm_proc_epoch(" << hex << shm_proc_epoch << "): "<<*shm_proc_epoch<< dec << endl;
    *shm_proc_epoch &= ~(1UL << proc_id);
    // cerr <<"[iob_mon] ("<< __func__<< ") *shm_proc_epoch: "<< hex <<*shm_proc_epoch<< dec << endl;
}

void io_bench::prepare(void)
{
	int ret, s;
	cpu_set_t cpuset;

#ifdef MLFS
	init_fs();
#endif

	pthread_mutex_init(&cv_mutex, NULL);
	pthread_cond_init(&cv, NULL);

#if 0
	CPU_ZERO(&cpuset);

	/*
	if (id < 8) {
		for (int j = 0; j < 8; j++)
			CPU_SET(j, &cpuset);
	} else {
		for (int j = 8; j < 15; j++)
			CPU_SET(j, &cpuset);
	}
	*/

	CPU_SET(id, &cpuset);

	s = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (s != 0)
		handle_error_en(s, "pthread_setaffinity_np");

	printf("Thread %d assigned to core %d\n", id, id);
#endif

	ret = mkdir(test_dir_prefix, 0777);

	if (ret < 0 && errno != EEXIST) {
		perror("mkdir\n");
		exit(-1);
	}

#ifdef ODIRECT
	ret = posix_memalign((void **)&buf, 4096, BUF_SIZE);
	if (ret != 0)
		err(1, "posix_memalign");
#else
	buf = new char[(4 << 20)];
#endif


	if (test_type == SEQ_READ || test_type == RAND_READ) {
		for(unsigned long i = 0; i < BUF_SIZE; i++)
			buf[i] = 1;

#ifdef ODIRECT
		if ((fd = open(test_file.c_str(), O_RDWR| O_DIRECT, 0666)) < 0)
#else
		if ((fd = open(test_file.c_str(), O_RDWR, 0666)) < 0)
#endif
			err(1, "open");
	} else {
		for (unsigned long i = 0; i < BUF_SIZE; i++)
			buf[i] = '0' + (i % 10);

#ifdef ODIRECT
		fd = open(test_file.c_str(), O_RDWR | O_CREAT| O_TRUNC | O_DIRECT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#else
		fd = open(test_file.c_str(), O_RDWR | O_CREAT| O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif
		if (fd < 0) {
			err(1, "open");
		}
	}

	/**
	 * If its random write and FS, we preallocate the file so we can do
	 * random writes
	 */
	/*
	if (test_type == RAND_WRITE || test_type == ZIPF_WRITE) {
		//fallocate(fd, 0, 0, file_size_bytes);
		cout << "allocate file" << endl;

		test_t test_type_back = test_type;

		test_type = SEQ_WRITE;
		this->do_write();

		test_type = test_type_back;

		lseek(fd, 0, SEEK_SET);
	}
	*/

	if (test_type == RAND_WRITE || test_type == RAND_READ) {
		std::random_device rd;
		std::mt19937 mt(rd());
		//std::mt19937 mt;
		std::uniform_int_distribution<uint64_t> dist(0, file_size_bytes - 4096);

		for (uint64_t i = 0; i < ops_cap; i++) {
			//io_list.push_back(dist(mt));
			io_list.push_back(ALIGN_FLOOR(dist(mt), io_size));
		}
	} else if (test_type == ZIPF_WRITE || test_type == ZIPF_READ || test_type == ZIPF_MIX) {
		std::ifstream infile(zipf_file);
		unsigned long offset;
		std::random_device rd;
		std::mt19937 mt(rd());
		std::uniform_int_distribution<uint64_t> dist(0, 100);

		while (infile >> offset) {
			io_list.push_back(ALIGN_FLOOR(offset, io_size));

			if (dist(mt) < 80)
				// read
                op_list.push_back(0);
			else
                op_list.push_back(1);
		}
#if 0
		// This is not correct. Fix it later
		boost::mt19937 rg;
		boost::math::pareto_distribution<> dist;
		boost::random::uniform_real_distribution<> uniformReal(1.0, 10.0);
		//rg.seed(time(NULL));
		boost::variate_generator<boost::mt19937&,
			boost::random::uniform_real_distribution<> > generator(rg, uniformReal);

		double value;
		for (uint64_t i = 0; i < file_size_bytes / io_size; i++)  {
			//cout << (int)(file_size_bytes * boost::math::pdf(dist, uniformReal(rg))) << endl;
			value = boost::math::pdf(dist,generator());
			io_list.push_back(ALIGN_FLOOR((log)(file_size_bytes * value), io_size));
		}
#endif
	}

	/*
	for(auto it : io_list)
		cout << it << endl;
	*/
}

void io_bench::do_write(void)
{
	int bytes_written;
	unsigned long random_range;
	unsigned long  count = 0;

	random_range = file_size_bytes / io_size;

        pthread_barrier_wait(&tsync);

	if (per_thread_stats) {
		time_stats_init(&stats, 1);
		time_stats_start(&stats);
	}

	cout << "# of ops: " << ops_cap << endl;

        // Used for monitor.
        unsigned int mon_count = 0;
        unsigned long prev_written_size = 0;
	unsigned long accum_written_size = 0;

	if (test_type == SEQ_WRITE) {
		unsigned int _io_size = io_size;

		while (*shm_proc_inf > 0){
		    // reset offset.
		    lseek(fd, 0, SEEK_SET);
		    count = 0;

		    for (unsigned long i = 0; i < file_size_bytes; i += io_size) {
			count++;
			accum_written_size += io_size;

			// if we are in the next epoch, print size of data written.
			if (next_epoch_started()) {
			    mon_count++;
			    cerr << mon_count << " " << (accum_written_size - prev_written_size) / (1024*1024) << endl;
			    prev_written_size = accum_written_size;
			    clear_next_epoch_bit();
			}

			if (i + io_size > file_size_bytes)
			    _io_size = file_size_bytes - i;
			else
			    _io_size = io_size;

#ifdef VERIFY
			for (int j = 0; j < _io_size; j++)
			    buf[j] = '0' + (i % 10);
#endif
			bytes_written = write(fd, buf, _io_size);
			assert (bytes_written >= 0);

#if 0
			if (do_fsync) {
			    fsync(fd);
			}
#endif

			if (bytes_written != io_size) {
			    printf("write request %u received len %d\n",
				    _io_size, bytes_written);
			    errx(1, "write");
			}
			if(count >= ops_cap)
			    break;
		    }
		}
                cerr << "WRITE DONE" << endl;
	} else if (test_type == RAND_WRITE || test_type == ZIPF_WRITE) {
		unsigned int _io_size = io_size;
		while (*shm_proc_inf > 0){
		    // reset offset.
		    lseek(fd, 0, SEEK_SET);
		    count = 0;

		    for (auto it : io_list) {
			accum_written_size += io_size;

			// if we are in the next epoch, print size of data written.
			if (next_epoch_started()) {
			    mon_count++;
			    cerr << mon_count << " " << (accum_written_size - prev_written_size) / (1024*1024) << endl;
			    prev_written_size = accum_written_size;
			    clear_next_epoch_bit();
			}

			count++;
			/*
			   if (it + io_size > file_size_bytes) {
			   _io_size = file_size_bytes - it;
			   cout << _io_size << endl;
			   } else
			   _io_size = io_size;
			   */

			lseek(fd, it, SEEK_SET);
			bytes_written = write(fd, buf, _io_size);
			if (bytes_written != _io_size) {
			    printf("write request %u received len %d\n",
				    _io_size, bytes_written);
			    errx(1, "write");
			}
			if(count >= ops_cap)
			    break;
		    }
		}
                cerr << "WRITE DONE" << endl;
	}
    else if (test_type == ZIPF_MIX) {
		unsigned int _io_size = io_size;
        std::list<uint8_t>::iterator op_it = op_list.begin();
		for (auto it : io_list) {
			count++;
			lseek(fd, it, SEEK_SET);

            //read
            if (*op_it == 0) {
                read(fd, buf, io_size);
            }
            //write
            else {
                bytes_written = write(fd, buf, _io_size);
    			if (bytes_written != _io_size) {
    				printf("write request %u received len %d\n",
    						_io_size, bytes_written);
    				errx(1, "write");
    			}
            }
		    ++op_it;
		    if(count >= ops_cap)
		    	break;
		}
    }

	if (do_fsync) {
		fsync(fd);
	}

	if (per_thread_stats) {
		time_stats_stop(&stats);

		//time_stats_print(&stats, (char *)"---------------");

		printf("Throughput: %3.3f MB\n",(float)(file_size_bytes)
				/ (1024.0 * 1024.0 * (float) time_stats_get_avg(&stats)));
	}

	return ;
}

void io_bench::do_read(void)
{
	int ret;
	unsigned long count = 0;

        pthread_barrier_wait(&tsync);

	if (per_thread_stats) {
		time_stats_init(&stats, 1);
		time_stats_start(&stats);
	}

	cout << "# of ops: " << ops_cap << endl;

        unsigned int mon_count = 0;
        unsigned long prev_read_size = 0;
        unsigned long accum_read_size = 0;

	if (test_type == SEQ_READ) {
	    while (*shm_proc_inf > 0) {
		// reset offset.
		lseek(fd, 0, SEEK_SET);
		count = 0;

		for (unsigned long i = 0; i < file_size_bytes ; i += io_size) {
		    accum_read_size += io_size;
		    // if we are in the next epoch, print size of data read.
		    if (next_epoch_started()) {
			mon_count++;
			cerr << mon_count << " " << (accum_read_size-prev_read_size) / (1024*1024) << endl;
			prev_read_size = accum_read_size;
			clear_next_epoch_bit();
		    }

		    count++;
		    if (i + io_size > file_size_bytes)
			io_size = file_size_bytes - i;
		    else
			io_size = io_size;
#ifdef VERIFY
		    memset(buf, 0, io_size);

#endif
		    ret = read(fd, buf, io_size);
#if 0
		    if (ret != io_size) {
			printf("read size mismatch: return %d, request %lu\n",
				ret, io_size);
		    }
#endif
#ifdef VERIFY
		    // verify buffer
		    for (int j = 0; j < io_size; j++) {
			if (buf[j] != '0' + (i % 10)) {
			    //hexdump(buf + j, 256);
			    printf("read data mismatch at %lu\n", i);
			    printf("expected %c read %c\n", (int)('0' + (i % 10)), buf[j]);
			    //exit(-1);
			    break;
			}
		    }
#endif
		    if(count >= ops_cap)
			break;
		}
	    }
	    cerr << "READ DONE" << endl;
	} else if (test_type == RAND_READ || test_type == ZIPF_READ) {
	    while (*shm_proc_inf > 0) {
		// reset offset.
		lseek(fd, 0, SEEK_SET);
		count = 0;

		for (auto it : io_list) {
		    accum_read_size += io_size;
                        // if we are in the next epoch, print size of data written.
                        if (next_epoch_started()) {
                            mon_count++;
                            cerr << mon_count << " " << (accum_read_size - prev_read_size) / (1024*1024) << endl;
                            prev_read_size = accum_read_size;
                            clear_next_epoch_bit();
                        }

			count++;
		/*
			if (it + io_size > file_size_bytes)
				io_size = file_size_bytes - it;
			else
				io_size = io_size;
		*/

			ret = pread(fd, buf, io_size, it);
			if(count >= ops_cap)
				break;
		}
	    }
	    cerr << "READ DONE" << endl;
	}

#if 0
	for (unsigned long i = 0; i < file_size_bytes; i++) {
		int bytes_read = read(fd, buf+i, io_size + 100);

		if (bytes_read != io_size) {
			printf("read too far: length %d\n", bytes_read);
		}
	}
#endif

	if (per_thread_stats)  {
		time_stats_stop(&stats);
		//time_stats_print(&stats, (char *)"---------------");

		//printf("%f\n", (float) time_stats_get_avg(&stats));

		printf("Throughput: %3.3f MB\n",(float)((file_size_bytes) >> 20)
				/ (float) time_stats_get_avg(&stats));
	}

	return ;
}

void io_bench::Run(void)
{
	cout << "thread " << id << " start - " << endl;
	cout << "file: " << test_file << endl;

	if (test_type == SEQ_READ || test_type == RAND_READ || test_type == ZIPF_READ)
		this->do_read();
	else {
		this->do_write();
	}

	if (test_type == SEQ_WRITE_READ)
		this->do_read();

	//pthread_mutex_lock(&cv_mutex);
	//pthread_cond_signal(&cv);
	pthread_mutex_unlock(&cv_mutex);

	return;
}

void io_bench::cleanup(void)
{
	close(fd);

#if 0
	if (test_type == SEQ_READ || test_type == RAND_READ) {
		// Read data integrity check.
		for (unsigned long i = 0; i < file_size_bytes; i++) {
			if (buf[i] != '0' + (i % 10)) {
				hexdump(buf + i, 256);
				printf("read data mismatch at %lu\n", i);
				printf("expected %c read %c\n", (int)('0' + (i % 10)), buf[i]);
				exit(-1);
			}
		}

		printf("Read data matches\n");
	}
#endif

#ifdef ODIRECT
	free(buf);
#else
	delete buf;
#endif
}

unsigned long io_bench::str_to_size(char* str)
{
	/* magnitude is last character of size */
	char size_magnitude = str[strlen(str)-1];
	/* erase magnitude char */
	str[strlen(str)-1] = 0;
	unsigned long file_size_bytes = strtoull(str, NULL, 0);
	switch(size_magnitude) {
		case 'g':
		case 'G':
			file_size_bytes *= 1024;
		case 'm':
		case 'M':
			file_size_bytes *= 1024;
		case '\0':
		case 'k':
		case 'K':
			file_size_bytes *= 1024;
			break;
		case 'p':
		case 'P':
			file_size_bytes *= 4;
			break;
		case 'b':
		case 'B':
         break;
		default:
			std::cout << "incorrect size format " << str << endl;
			break;
	}
	return file_size_bytes;
}

test_t io_bench::get_test_type(char *test_type)
{
	/**
	 * Check the mode to bench: read or write and type
	 */
	if (!strcmp(test_type, "sr")){
		return SEQ_READ;
	}
	else if (!strcmp(test_type, "sw")) {
		return SEQ_WRITE;
	}
	else if (!strcmp(test_type, "wr")) {
		return SEQ_WRITE_READ;
	}
	else if (!strcmp(test_type, "rw")) {
		return RAND_WRITE;
	}
	else if (!strcmp(test_type, "rr")) {
		return RAND_READ;
	}
	else if (!strcmp(test_type, "zw")) {
		return ZIPF_WRITE;
	}
	else if (!strcmp(test_type, "zr")) {
		return ZIPF_READ;
	}
    else if (!strcmp(test_type, "zm")) {
        return ZIPF_MIX;
    }
	else {
		show_usage("iobench");
		cerr << "unsupported test type" << test_type << endl;
		exit(-1);
	}
}

#define HEXDUMP_COLS 8
void io_bench::hexdump(void *mem, unsigned int len)
{
	unsigned int i, j;

	for(i = 0; i < len + ((len % HEXDUMP_COLS) ?
				(HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++) {
		/* print offset */
		if(i % HEXDUMP_COLS == 0) {
			printf("0x%06x: ", i);
		}

		/* print hex data */
		if(i < len) {
			printf("%02x ", 0xFF & ((char*)mem)[i]);
		} else {/* end of block, just aligning for ASCII dump */
			printf("	");
		}

		/* print ASCII dump */
		if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1)) {
			for(j = i - (HEXDUMP_COLS - 1); j <= i; j++) {
				if(j >= len) { /* end of block, not really printing */
					printf(" ");
				} else if(isprint(((char*)mem)[j])) { /* printable char */
					printf("%c",(0xFF & ((char*)mem)[j]));
				} else {/* other char */
					printf(".");
				}
			}
			printf("\n");
		}
	}
}

void io_bench::show_usage(const char *prog)
{
    std::cerr << "usage: " << prog
        << " <wr/sr/sw/rr/rw/zr/zw/zm>"
        << " <size: X{G,M,K,P}, eg: 100M> <IO size, e.g.: 4K> <# of thread>"
        << " [-d <directory>] [-f <file-prefix>] [-n <# of ops>]"
        << " [-s 'fsync']"
        << " [-i <id of a process>]"
        << " [-p 'Make waiting processes run.']"
        << " [-w 'Wait before benchmark start and shutdown_fs() call.']"
        << " [-r 'Infinite mode. Keep running.']"
        << " [-q 'Quit benchmark (Used for a run with -r option.)']"
        << endl;
}

/* Returns new argc */
static int adjust_args(int i, char *argv[], int argc, unsigned del)
{
   if (i >= 0) {
      for (int j = i + del; j < argc; j++, i++)
         argv[i] = argv[j];
      argv[i] = NULL;
      return argc - del;
   }
   return argc;
}

#if 0
int process_opt_args(int argc, char *argv[])
{
   int dash_d = -1;

   for (int i = 0; i < argc; i++) {
      if (strncmp("-d", argv[i], 2) == 0) {
         test_dir_prefix = argv[i+1];
         dash_d = i;
      }
   }

   return adjust_args(dash_d, argv, argc, 2);
}
#endif

int process_opt_args(int argc, char *argv[])
{
   int dash_d = -1;
restart:
   for (int i = 0; i < argc; i++) {
      //printf("argv[%d] = %s\n", i, argv[i]);
      if (strncmp("-d", argv[i], 2) == 0) {
         test_dir_prefix = argv[i+1];
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 2);
	 goto restart;
      }
      else if (strncmp("-n", argv[i], 2) == 0) {
	 ops_cap = strtoull(argv[i+1], NULL, 0);
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 2);
	 goto restart;
      }
      else if (strncmp("-f", argv[i], 2) == 0) {
	 test_file_name = argv[i+1];
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-s", argv[i], 2) == 0) {
	 do_fsync = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-p", argv[i], 2) == 0) {
	 psync = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-i", argv[i], 2) == 0) {
	 proc_id = (int)strtoul(argv[i+1], NULL, 0);
         printf("PROC ID: %d\n", proc_id);
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-t", argv[i], 2) == 0) {
         timer_mode = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-w", argv[i], 2) == 0) {
	 wait_signal = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-r", argv[i], 2) == 0) {
	 run_inf = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-q", argv[i], 2) == 0) {
	 quit_sync = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
   }

   return argc;
}

int main(int argc, char *argv[])
{
	int n_threads, i;
	std::vector<io_bench *> io_workers;
	unsigned long file_size_bytes;
	unsigned int io_size = 0;
	struct time_stats main_stats, total_stats;
	const char *device_id;

        /* Shared memory.
         * Bit 0 is used for process barrier.
         * Bit i(other than 0) is used to notify 'process n+1'
         * that the next epoch has begun.
         */
	// int *shm_proc;
	int fd, res;
	char zipf_file_name[100];

	device_id = getenv("FILE_ID");
	ops_cap = 0;
	do_fsync = 0;
	psync = 0;
	wait_signal = 0;
	run_inf = 0;
	quit_sync = 0;

	if (device_id)
		dev_id = atoi(device_id);
	else
		dev_id = 0;

	shm_proc_sync = (unsigned int*)create_shm(fd, res, SHM_PATH_SYNC);
	shm_proc_inf = shm_proc_sync;
	shm_proc_inf++;    // shm_proc_inf uses the next 32bits of shm_proc_sync.
	shm_proc_epoch = (unsigned int*)create_shm(fd, res, SHM_PATH_EPOCH);

	argc = process_opt_args(argc, argv);
	if (argc < 5) {
		if(psync) {
			printf("Setting shm_proc_sync(%d) to zero\n", *shm_proc_sync);
			*shm_proc_sync = 0;
			exit(0);
		} else if (timer_mode){
			start_mon_timer();
			exit(0);
                } else if (quit_sync) {
			printf("Setting shm_proc_inf(%d) to zero\n", *shm_proc_inf);
			*shm_proc_inf = 0;
			exit(0);
		} else {
			io_bench::show_usage(argv[0]);
		}
		exit(-1);
	}

	n_threads = std::stoi(argv[4]);

	file_size_bytes = io_bench::str_to_size(argv[2]);
	io_size = io_bench::str_to_size(argv[3]);

	if (io_bench::get_test_type(argv[1]) == ZIPF_WRITE ||
			io_bench::get_test_type(argv[1]) == ZIPF_READ
            || io_bench::get_test_type(argv[1]) == ZIPF_MIX) {
		if (argc != 6) {
			cout << "must supply zipf file" << endl;
			exit(-1);
		}

		strncpy(zipf_file_name, argv[5], 100);
	} else
		strncpy(zipf_file_name, "none", 4);

	//std::cout << "Total file size: " << file_size_bytes << "B" << endl
	//	<< "io size: " << io_size << "B" << endl
	//	<< "# of thread: " << n_threads << endl;

	//if(do_fsync)
	//	std::cout << "Sync mode" << endl;

	if(!ops_cap)
		ops_cap = file_size_bytes / io_size;
	else
		ops_cap = min(file_size_bytes / io_size, ops_cap);

	for (i = 0; i < n_threads; i++) {
		io_workers.push_back(new io_bench(i,
					file_size_bytes,
					io_size,
					io_bench::get_test_type(argv[1]),
					zipf_file_name
					));
	}

        pthread_barrier_init(&tsync, NULL, n_threads);
	time_stats_init(&main_stats, 1);
	//time_stats_init(&total_stats, 1);

	//time_stats_start(&total_stats);

	for (auto it : io_workers) {
		it->prepare();
		pthread_mutex_lock(&it->cv_mutex);
		it->per_thread_stats = 1;
	}

	if (run_inf) {
	    printf("Running in infinite mode. -q will stop its running.\n");
	    *shm_proc_inf += 1;
	}

	if (wait_signal) {
	    printf("Waiting for start signal\n");
	    *shm_proc_sync += 1;
	    while (*shm_proc_sync > 0){
		usleep(100);
	    }
	}

	printf("Starting benchmark ...\n");
	time_stats_start(&main_stats);

	for (auto it : io_workers) {
		it->Start();
		//pthread_mutex_lock(&it->cv_mutex);
	}

	/*
	for (auto it : io_workers)
		pthread_cond_wait(&it->cv, &it->cv_mutex);
	*/

	for (auto it : io_workers)
		pthread_mutex_lock(&it->cv_mutex);

	for (auto it : io_workers)
		it->cleanup();

	for (auto it : io_workers)
		it->Join();

	time_stats_stop(&main_stats);

#ifdef MLFS
	// Do not execute shutdown_fs() for multi-process throughput measurement.
	// Digestion by shutdown_fs() spoils throughput microbench results.
	if (wait_signal) {
	    printf("Waiting for shutdown signal\n");
	    *shm_proc_sync += 1;
	    while (*shm_proc_sync > 0){
		usleep(1000000);
	    }
	    shutdown_fs();
	} else {
	    shutdown_fs();
	}
#endif

	//time_stats_stop(&total_stats);

	time_stats_print(&main_stats, (char *)"--------------- stats");

	printf("Aggregated throughput: %3.3f MB\n",
			((float)n_threads * (float)((file_size_bytes) >> 20))
			/ (float) time_stats_get_avg(&main_stats));
	//printf("--------------------------------------------\n");

	//time_stats_print(&total_stats, (char *)"----------- total stats");

	fflush(stdout);
	fflush(stderr);

	return 0;
}
