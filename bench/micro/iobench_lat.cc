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

#ifdef MLFS
#include <mlfs/mlfs_interface.h>	
#endif

#include "time_stat.h"
#include "thread.h"

//#define VERIFY

#ifdef MLFS
const char *test_dir_prefix = "/mlfs/";
#else
const char *test_dir_prefix = "./pmem";
//const char test_dir_prefix[] = "./ssd";
#endif

char *test_file_name = "testfile";

unsigned long ops_cap;
int do_fsync;
int remote;

#define ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define ALIGN(x, a)  ALIGN_MASK((x), ((__typeof__(x))(a) - 1))
#define BUF_SIZE (2 << 20)

typedef enum {SEQ_WRITE, SEQ_READ, RAND_WRITE, RAND_READ, ZIPF_WRITE, ZIPF_READ, SEQ_WRITE_READ, NONE} test_t;

typedef enum {FS} test_mode_t;

typedef unsigned long addr_t;

using namespace std;

class io_bench : public CThread 
{
	public:
		io_bench(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
				test_t _test_type, string _zipf_file);

		int id, fd;
		unsigned long file_size_bytes;
		unsigned int io_size;
		test_t test_type;
		string test_file;
		string zipf_file;
		char *buf;
		struct time_stats iostats;
		struct time_stats fstats;

		std::list<uint64_t> io_list;

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
	test_file += "/" + std::string(test_file_name); //+ std::to_string(id);
}

#define handle_error_en(en, msg) \
	do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

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

	if (id < 8) {
		for (int j = 0; j < 8; j++)
			CPU_SET(j, &cpuset);
	} else {
		for (int j = 8; j < 15; j++)
			CPU_SET(j, &cpuset);
	}

	s = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (s != 0)
		handle_error_en(s, "pthread_setaffinity_np");
#endif

	//ret = mkdir(test_dir_prefix, 0777);

	//if (ret < 0 && errno != EEXIST) { 
	//	perror("mkdir\n");
	//	exit(-1);
	//}

	buf = new char[(4 << 20)];

	if (test_type == SEQ_READ || test_type == RAND_READ) {
		for(unsigned long i = 0; i < BUF_SIZE; i++)
			buf[i] = 1;

		if ((fd = open(test_file.c_str(), O_RDWR)) < 0)
			err(1, "open");
	} else {
		for (unsigned long i = 0; i < BUF_SIZE; i++) 
			buf[i] = '0' + (i % 10);

		fd = open(test_file.c_str(), O_RDWR | O_CREAT| O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

		//fd = open(test_file.c_str(), O_RDWR | O_CREAT, 0775);
		//FILE *file = fopen(test_file.c_str(), "w"); 
		//fd = fileno(file);
		if (fd < 0) {
			err(1, "open");
		}
		else
			cout << "opened file: " << test_file.c_str() << endl;
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
		std::uniform_int_distribution<uint64_t> dist(0, file_size_bytes - (4 << 10));
		for (uint64_t i = 0; i < ops_cap; i++) 
			//io_list.push_back(dist(mt));
			io_list.push_back(ALIGN((dist(mt)), (4 << 10)));
	}
	else if (test_type == ZIPF_WRITE || test_type == ZIPF_READ) {
		std::ifstream infile(zipf_file);
		unsigned long offset;

		while (infile >> offset)
			io_list.push_back(offset);	
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
	int count = 0;
	unsigned int size = 0;
	float perc = 0.1;

	random_range = file_size_bytes / io_size;


	cout << "# of ops: " << ops_cap << endl;

	time_stats_init(&iostats, ops_cap);
	time_stats_init(&fstats, ops_cap);

	if (test_type == SEQ_WRITE || test_type == SEQ_WRITE_READ) {
		for (unsigned long i = 0; i < file_size_bytes; i += io_size) {
			if (i + io_size > file_size_bytes)
				size = file_size_bytes - i;
			else
				size = io_size;

#ifdef VERIFY
			for (int j = 0; j < io_size; j++) 
				buf[j] = '0' + (i % 10);
#endif
			time_stats_start(&iostats);

			bytes_written = write(fd, buf, size);
			if (do_fsync) {
				time_stats_start(&fstats);
				fsync(fd);
				time_stats_stop(&fstats);
			}

			time_stats_stop(&iostats);

			//if (bytes_written != size) {
			//	printf("write request %u received len %d\n",
			//			size, bytes_written);
			//	errx(1, "write");
			//}

			count++;
			if(count >= ops_cap)
				break;

			//printf("write-op-%u\n", count);
			//if(count >= perc * ops_cap) {
			//	printf("total ops finished = %u (%f %)\n", count, perc*100);
			//	perc += 0.1;
			//}
		}
	}
	else if (test_type == RAND_WRITE || test_type == ZIPF_WRITE) {
		for (auto it : io_list) {
			if (it + io_size > file_size_bytes)
				size = file_size_bytes - it;
			else
				size = io_size;

			lseek(fd, it, SEEK_SET);

			time_stats_start(&iostats);

			bytes_written = write(fd, buf, size);
			if (bytes_written != size) {
				printf("write request %u received len %d\n",
						size, bytes_written);
				errx(1, "write");
			}
			if (do_fsync) {
				time_stats_start(&fstats);
				fsync(fd);
				time_stats_stop(&fstats);
			}

			time_stats_stop(&iostats);

			count++;
			if(count >= ops_cap)
				break;
		}
	}

	time_stats_print(&iostats, (char *)"--------------- Aggregate Latency (Write + Fsync)");

	if(do_fsync) {
		double avg = time_stats_get_avg(&fstats); 
		printf("\tfsync-avg: %.3f msec (%.2f usec)\n", avg * 1000.0, avg * 1000000.0);
	}

	return ;
}

void io_bench::do_read(void)
{
	int ret;
	int count = 0;

	int size = 0;

	cout << "# of ops: " << ops_cap << endl;
	time_stats_init(&iostats, ops_cap);

	if (test_type == SEQ_READ || test_type == SEQ_WRITE_READ) {
		for (unsigned long i = 0; i < file_size_bytes ; i += io_size) {
			if (i + io_size > file_size_bytes)
				size = file_size_bytes - i;
			else
				size = io_size;
#ifdef VERIFY
			memset(buf, 0, io_size);

#endif
			time_stats_start(&iostats);
			ret = read(fd, buf, size);
			time_stats_stop(&iostats);
#if 0
			if (ret != size) {
				printf("read size mismatch: return %d, request %lu\n",
						ret, io_size);
			}
#endif
#ifdef VERIFY
			// verify buffer
			for (int j = 0; j < size; j++) {
				if (buf[j] != '0' + (i % 10)) {
					//hexdump(buf + j, 256);
					printf("read data mismatch at %lu\n", i);
					printf("expected %c read %c\n", (int)('0' + (i % 10)), buf[j]);
					//exit(-1);
					break;
				}
			}
#endif
			count++;
			if(count >= ops_cap)
				break;
		}
	} else if (test_type == RAND_READ || test_type == ZIPF_READ) {
		for (auto it : io_list) {
			if (it + io_size > file_size_bytes)
				size = file_size_bytes - it;
			else
				size = io_size;

			time_stats_start(&iostats);

			ret = pread(fd, buf, size, it);

			time_stats_stop(&iostats);

			count++;
			if(count >= ops_cap)
				break;
		}
	}

#if 0
	for (unsigned long i = 0; i < file_size_bytes; i++) {
		int bytes_read = read(fd, buf+i, size + 100);

		if (bytes_read != size) {
			printf("read too far: length %d\n", bytes_read);
		}
	}
#endif

	time_stats_print(&iostats, (char *)"---------------");

	return ;
}

void io_bench::Run(void)
{
	cout << "thread " << id << " start - ";
	cout << "file: " << test_file << endl;

	if (test_type == SEQ_READ || test_type == RAND_READ) {
		this->do_read();
	}
	else {
		this->do_write();
	}

	lseek(fd, 0, SEEK_SET);

	if (test_type == SEQ_WRITE_READ) {
		//io_size = 128;
		//ops_cap = file_size_bytes / io_size;
		for(int i=0; i<4; i++) {
			this->do_read();
			lseek(fd, 0, SEEK_SET);
		}
	}

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

	delete buf;
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
	else if (!strcmp(test_type, "rw")) {
		return RAND_WRITE;
	}
	else if (!strcmp(test_type, "rr")) {
		return RAND_READ;
	}
	else if (!strcmp(test_type, "wr")) {
		return SEQ_WRITE_READ;
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
		<< " [-d <directory>] [-f <file-name>] [-n <ops cap>] [-s 'fsync'] <sr/sw/rr/rw/zr/zw>"
		<< " <size: X{G,M,K,P}, eg: 100M> <IO size, e.g.: 4K> <# of thread>"
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
   }

   return argc;
}

int main(int argc, char *argv[])
{
	int n_threads, i;
	std::vector<io_bench *> io_workers;
	unsigned long file_size_bytes;
	unsigned int io_size = 0;
	char zipf_file_name[100];
	ops_cap = 0;
	do_fsync = 0;
	remote = 0;

	argc = process_opt_args(argc, argv);

	if (argc < 5) {
		io_bench::show_usage(argv[0]);
		exit(-1);
	}

	n_threads = std::stoi(argv[4]);

	file_size_bytes = io_bench::str_to_size(argv[2]);
	io_size = io_bench::str_to_size(argv[3]);

	if (io_bench::get_test_type(argv[1]) == ZIPF_WRITE ||
			io_bench::get_test_type(argv[1]) == ZIPF_READ) {
		if (argc != 6) {
			cout << "must supply zipf file" << endl;
			exit(-1);
		}

		strncpy(zipf_file_name, argv[5], 100);
	} else 
		strncpy(zipf_file_name, "none", 4); 

	if(!ops_cap)
		ops_cap = file_size_bytes / io_size;
	else
		ops_cap = min(file_size_bytes / io_size, ops_cap);

	/*
	std::cout << "Total file size: " << file_size_bytes << "B" << endl
		<< "io size: " << io_size << "B" << endl
		<< "# of thread: " << n_threads << endl;
	*/

	//if(do_fsync)
	//	std::cout << "Sync mode" << endl;

	for (i = 0; i < n_threads; i++) {
		io_workers.push_back(new io_bench(i, 
					file_size_bytes,
					io_size,
					io_bench::get_test_type(argv[1]),
					zipf_file_name));
	}

	for (auto it : io_workers) {
		it->prepare();
		pthread_mutex_lock(&it->cv_mutex);
	}

	for (auto it : io_workers) 
		it->Start();

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

#ifdef MLFS

	// To get THPOOL profile result.
	// for (int cnt = 0; cnt < 20; cnt++) {
	//         printf("Waiting 20 seconds. %d\n", cnt);
	//         sleep(1);
	// }

	sleep(2);
	shutdown_fs();
#endif

	fflush(stdout);
	fflush(stderr);

	return 0;
}
