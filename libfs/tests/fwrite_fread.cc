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

#include <iostream>
#include <string>
#include <vector>

#ifdef MLFS
#include <mlfs/mlfs_interface.h>	
#endif

#include "time_stat.h"
#include "thread.h"

#ifdef MLFS
const char test_dir_prefix[] = "/mlfs/";
#else
const char test_dir_prefix[] = "./t";
#endif

typedef enum {SEQ_WRITE, SEQ_READ, SEQ_WRITE_READ, NONE} test_t;

typedef enum {FS} test_mode_t;

typedef unsigned long addr_t;

class io_bench : public CThread 
{
	public:
		io_bench(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
				test_t _test_type);

		FILE *fp;
		int id, per_thread_stats;
		unsigned long file_size_bytes;
		unsigned int io_size;
		test_t test_type;
		string test_file;
		int do_fsync;
		char *buf;
		struct time_stats stats;

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
	test_file += "/file" + std::to_string(id);
	per_thread_stats = 0;
}

void io_bench::prepare(void)
{
	int ret;

#ifdef MLFS
	init_fs();
#endif
	do_fsync = 1;

	pthread_mutex_init(&cv_mutex, NULL);
	pthread_cond_init(&cv, NULL);

	ret = mkdir(test_dir_prefix, 0777);

	if (ret < 0 && errno != EEXIST) { 
		perror("mkdir\n");
		exit(-1);
	}

	buf = new char[file_size_bytes];

	if (test_type == SEQ_READ) {
		for(unsigned long i = 0; i < file_size_bytes; i++)
			buf[i] = 1;

		fp = fopen(test_file.c_str(), "w+");

		if (fp == NULL) {
			cerr << "cannot open " << test_file << endl;
			exit(-1);
		}
	} else {
		for (unsigned long i = 0; i < file_size_bytes; i++) 
			buf[i] = '0' + (i % 10);

		fp = fopen(test_file.c_str(), "w+");
		if (fp == NULL) {
			cerr << "cannot open " << test_file << endl;
			exit(-1);
		}
	}

	srand(time(NULL));   
}

void io_bench::do_write(void)
{
	unsigned long random_range;

	random_range = file_size_bytes / io_size;

	if (per_thread_stats) {
		time_stats_init(&stats, 1);
		time_stats_start(&stats);
	}

	int bytes_written;
	for (unsigned long i = 0; i < file_size_bytes; i += io_size) {
		if (i + io_size > file_size_bytes)
			io_size = file_size_bytes - i;
		else
			io_size = io_size;

		bytes_written = fwrite(buf+i, 1, io_size, fp);

		if (bytes_written != io_size) {
			printf("write request %u received len %d\n",
					io_size, bytes_written);
			errx(1, "write");
		}
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

	for (unsigned long i = 0; i < file_size_bytes ; i += io_size) {
		if (i + io_size > file_size_bytes)
			io_size = file_size_bytes - i;
		else
			io_size = io_size;

		ret = fread(buf + i, 1, io_size, fp);
#if 0
		if (ret != io_size) {
			printf("read size mismatch: return %d, request %lu\n",
					ret, io_size);
		}
#endif
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

	if (test_type == SEQ_READ)
		this->do_read();
	else 
		this->do_write();

	if (test_type == SEQ_WRITE_READ)
		this->do_read();

	//pthread_mutex_lock(&cv_mutex);
	//pthread_cond_signal(&cv);
	pthread_mutex_unlock(&cv_mutex);

	return;
}

void io_bench::cleanup(void)
{
	fclose(fp);

	if (test_type == SEQ_READ) {
		// Read data integrity check.
		for (unsigned long i = 0; i < file_size_bytes; i++) {
			if (buf[i] != '0' + (i % 10)) {
				hexdump(buf + i, 256);
				printf("read data mismatch at %lx\n", i);
				printf("expected %c read %c\n", (int)('0' + (i % 10)), buf[i]);
				exit(-1);
			}
		}

		printf("Read data matches\n");
	}

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
		<< " <wr/sr/sw>" 
		<< " <size: X{G,M,K,P}, eg: 100M> <IO size, e.g.: 4K> <# of thread>"
		<< endl;
}

int main(int argc, char *argv[])
{
	int n_threads, i;
	std::vector<io_bench *> io_workers;
	unsigned long file_size_bytes;
	unsigned int io_size = 0;
	struct time_stats main_stats, total_stats;

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

	for (auto it : io_workers) 
		it->Start();

	/*
	for (auto it : io_workers) 
		pthread_cond_wait(&it->cv, &it->cv_mutex);
	*/
	for (auto it : io_workers) 
		pthread_mutex_lock(&it->cv_mutex);

	time_stats_stop(&main_stats);

	for (auto it : io_workers) 
		it->cleanup();

	for (auto it : io_workers) 
		it->Join();

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
