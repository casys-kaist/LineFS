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

#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/math/distributions/pareto.hpp>
#include <boost/random.hpp>

#ifdef MLFS
#include <mlfs/mlfs_interface.h>	
#endif

#include "storage/spdk/async.h"
#include "storage/spdk/sync.h"

#include "time_stat.h"
#include "thread.h"

#ifdef MLFS
const char *test_dir_prefix = "/mlfs/";
#else
const char *test_dir_prefix = "./pmem";
//const char test_dir_prefix[] = "./ssd";
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
	ZIPF_WRITE, ZIPF_READ, NONE} test_t;

typedef enum {FS} test_mode_t;

typedef unsigned long addr_t;
uint8_t dev_id;

class io_bench : public CThread 
{
	public:
		io_bench(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
				test_t _test_type);

		int id, fd, per_thread_stats;
		unsigned long file_size_bytes;
		unsigned int io_size;
		test_t test_type;
		string test_file;
		int do_fsync;
		char *buf;
		struct time_stats stats;

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
		unsigned int _io_size, test_t _test_type)
	: id(_id), file_size_bytes(_file_size_bytes), io_size(_io_size), 
	test_type(_test_type)
{
	test_file.assign(test_dir_prefix);
	test_file += "/file" + std::to_string(dev_id) + "-" + std::to_string(id);
	per_thread_stats = 0;
}

#define handle_error_en(en, msg) \
	do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

typedef boost::mt19937 base_gen_type;
typedef boost::uniform_int<int> dist_type;
typedef boost::variate_generator<base_gen_type &, dist_type> var_gen_type;

void io_bench::prepare(void)
{
	int ret, s;
	cpu_set_t cpuset;

#ifdef MLFS
	init_fs();
	do_fsync = 0;
#else
	do_fsync = 1;
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
		if ((fd = open(test_file.c_str(), O_RDWR| O_DIRECT)) < 0)
#else
		if ((fd = open(test_file.c_str(), O_RDWR)) < 0)
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

		for (uint64_t i = 0; i < file_size_bytes / io_size; i++) {
			io_list.push_back(dist(mt));
			//io_list.push_back(ALIGN((dist(mt)), (4 << 10)));
		}
	} else if (test_type == ZIPF_WRITE || test_type == ZIPF_READ) {
		boost::mt19937 rg;
		boost::math::pareto_distribution<> dist;
		boost::random::uniform_real_distribution<> uniformReal(1.0,10.0);
		//rg.seed(time(NULL));
		boost::variate_generator<boost::mt19937&, 
			boost::random::uniform_real_distribution<> > generator(rg, uniformReal);

		double value;
		for (uint64_t i = 0; i < file_size_bytes / io_size; i++)  {
			//cout << (int)(file_size_bytes * boost::math::pdf(dist, uniformReal(rg))) << endl;
			value = boost::math::pdf(dist,generator());
			//cout << (int)(file_size_bytes * value) << endl;
			//io_list.push_back(ALIGN((int)(file_size_bytes * value), (4 << 10)));
			io_list.push_back(ALIGN_FLOOR((int)(file_size_bytes * value), io_size));
		}
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

	random_range = file_size_bytes / io_size;

	if (per_thread_stats) {
		time_stats_init(&stats, 1);
		time_stats_start(&stats);
	}

	if (test_type == SEQ_WRITE) {
		for (unsigned long i = 0; i < file_size_bytes; i += io_size) {
			if (i + io_size > file_size_bytes)
				io_size = file_size_bytes - i;
			else
				io_size = io_size;

#ifdef VERIFY
			for (int j = 0; j < io_size; j++) 
				buf[j] = '0' + (i % 10);
#endif

			bytes_written = write(fd, buf, io_size);

			if (bytes_written != io_size) {
				printf("write request %u received len %d\n",
						io_size, bytes_written);
				errx(1, "write");
			}
		}
	} else if (test_type == RAND_WRITE || test_type == ZIPF_WRITE) {
		for (auto it : io_list) {
			if (it + io_size > file_size_bytes)
				io_size = file_size_bytes - it;
			else
				io_size = io_size;

			lseek(fd, it, SEEK_SET);
			bytes_written = write(fd, buf, io_size);
			if (bytes_written != io_size) {
				printf("write request %u received len %d\n",
						io_size, bytes_written);
				errx(1, "write");
			}
		}
	}

	if (do_fsync) {
		printf("do_sync\n");
		fsync(fd);
	}

	if (per_thread_stats) {
		time_stats_stop(&stats);

		time_stats_print(&stats, (char *)"---------------");

		printf("Throughput: %3.3f MB\n",(float)(file_size_bytes)
				/ (1024.0 * 1024.0 * (float) time_stats_get_avg(&stats)));
	}

	return ;
}

void io_bench::do_read(void)
{
	int ret;

	if (per_thread_stats) {
		time_stats_init(&stats, 1);
		time_stats_start(&stats);
	}

	if (test_type == SEQ_READ) {
		for (unsigned long i = 0; i < file_size_bytes ; i += io_size) {
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
		}
	} else if (test_type == RAND_READ || test_type == ZIPF_READ) {
		for (auto it : io_list) {
			if (it + io_size > file_size_bytes)
				io_size = file_size_bytes - it;
			else
				io_size = io_size;

			ret = pread(fd, buf, io_size, it);
		}
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
		time_stats_print(&stats, (char *)"---------------");

		printf("%f\n", (float) time_stats_get_avg(&stats));

		printf("Throughput: %3.3f MB\n",(float)((file_size_bytes) >> 20)
				/ (float) time_stats_get_avg(&stats));
	}

	return ;
}

void io_bench::Run(void)
{
	cout << "thread " << id << " start - ";
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
		<< " [-d <directory>] <wr/sr/sw/rr/rw/zr/zw>"
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

   for (int i = 0; i < argc; i++) {
      if (strncmp("-d", argv[i], 2) == 0) {
         test_dir_prefix = argv[i+1];
         dash_d = i;
      }
   }

   return adjust_args(dash_d, argv, argc, 2);
}

int main(int argc, char *argv[])
{
	int n_threads, i;
	std::vector<io_bench *> io_workers;
	unsigned long file_size_bytes;
	unsigned int io_size = 0;
	struct time_stats main_stats, total_stats;
	const char *device_id;

	device_id = getenv("DEV_ID");

	if (device_id) 
		dev_id = atoi(device_id);
	else
		dev_id = 0;

	argc = process_opt_args(argc, argv);
	if (argc != 5) {
		io_bench::show_usage(argv[0]);
		exit(-1);
	}

	n_threads = std::stoi(argv[4]);

	file_size_bytes = io_bench::str_to_size(argv[2]);
	io_size = io_bench::str_to_size(argv[3]);

	std::cout << "Total file size: " << file_size_bytes << "B" << endl
		<< "io size: " << io_size << "B" << endl
		<< "# of thread: " << n_threads << endl;

	for (i = 0; i < n_threads; i++) {
		io_workers.push_back(new io_bench(i, 
					file_size_bytes,
					io_size,
					io_bench::get_test_type(argv[1])));
	}

	time_stats_init(&main_stats, 1);
	time_stats_init(&total_stats, 1);

	time_stats_start(&total_stats);

	for (auto it : io_workers) {
		it->prepare();
		pthread_mutex_lock(&it->cv_mutex);
		it->per_thread_stats = 0;
	}

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
	shutdown_fs();
#endif

	time_stats_stop(&total_stats);

	time_stats_print(&main_stats, (char *)"--------------- stats");

	printf("Aggregated throughput: %3.3f MB\n",
			((float)n_threads * (float)((file_size_bytes) >> 20))
			/ (float) time_stats_get_avg(&main_stats));
	printf("--------------------------------------------\n");

	time_stats_print(&total_stats, (char *)"----------- total stats");

	fflush(stdout);
	fflush(stderr);

	return 0;
}
