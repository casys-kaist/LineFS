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

#include "mlfs/mlfs_interface.h"

#include "time_stat.h"
#include "thread.h"

#define  test_dir_prefix "/mlfs/"

#define N_TESTS 1
class io_bench : public CThread 
{
	public:
		io_bench(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
				int _n_files, char _test_mode);

		int id;
		unsigned long file_size_bytes;
		unsigned int io_size;
		int n_files;
		char test_mode;
		std::vector<string> filenames;
		char *buf;
		struct time_stats stats;

		void prepare(void);

		void do_read(char *filename);
		void do_write(char *filename);

		// Thread entry point.
		void Run(void);

		// util methods
		static unsigned long str_to_size(char* str);
		static void hexdump(void *mem, unsigned int len);
		static void show_usage(const char *prog);
};

io_bench::io_bench(int _id, unsigned long _file_size_bytes, 
		unsigned int _io_size, int _n_files, char _test_mode)
	: id(_id), 
	file_size_bytes(_file_size_bytes), 
	io_size(_io_size), 
	n_files(_n_files), 
	test_mode(_test_mode)
{
}

void io_bench::prepare(void)
{
	int ret;

	init_fs();

	ret = mkdir(test_dir_prefix, 0777);

	if (ret < 0 && errno != EEXIST) { 
		perror("mkdir\n");
		exit(-1);
	}

	ret = mkdir(test_dir_prefix "fileset/", 0777);

	if (ret < 0 && errno != EEXIST) { 
		perror("mkdir\n");
		exit(-1);
	}

	buf = new char[file_size_bytes];

	srand(time(NULL));   
}

void io_bench::do_write(char *filename)
{
	int fd;
	unsigned long random_range;

	random_range = file_size_bytes / io_size;

WRITE_START:
	for (unsigned long i = 0; i < file_size_bytes; i++) {
		buf[i] = '0' + (i % 10);
	}


	//time_stats_init(&stats, N_TESTS);
	for (int test = 0 ; test < N_TESTS ; test++) {

		fd = open(filename, O_RDWR | O_CREAT| O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd < 0) {
			err(1, "open");
		}

		//time_stats_start(&stats);

		int bytes_written;
		for (unsigned long i = 0; i < file_size_bytes; i += io_size) {
			if (i + io_size > file_size_bytes)
				io_size = file_size_bytes - i;
			else
				io_size = io_size;

			bytes_written = write(fd, buf+i, io_size);

			if (bytes_written != io_size) {
				printf("write request %u received len %d\n",
						io_size, bytes_written);
				errx(1, "write");
			}
		}

		//time_stats_stop(&stats);

		close(fd);
	}

	/*
	printf("Throughput: %3.3f MB\n",(float)(file_size_bytes)
			/ (1024.0 * 1024.0 * (float) time_stats_get_avg(&stats)));

	time_stats_print(&stats, (char *)"---------------");
	*/

	return ;
}

void io_bench::do_read(char *filename)
{
	int fd;
	unsigned long i;
	int ret;

READ_START:
	printf("try to read file %s\n", filename);
	if ((fd = open(filename, O_RDWR)) < 0)
		err(1, "open");

	printf("File size %lu bytes\n", (unsigned long)file_size_bytes);

	for(unsigned long i = 0; i < file_size_bytes; i++)
		buf[i] = 1;

	//time_stats_init(&stats, 1);

	//time_stats_start(&stats);

	for (i = 0; i < file_size_bytes ; i += io_size) {
		if (i + io_size > file_size_bytes)
			io_size = file_size_bytes - i;
		else
			io_size = io_size;

		ret = read(fd, buf + i, io_size);
#if 0
		if (ret != io_size) {
			printf("read size mismatch: return %d, request %lu\n",
					ret, io_size);
		}
#endif
	}

	//time_stats_stop(&stats);

#if 0
	for (unsigned long i = 0; i < file_size_bytes; i++) {
		int bytes_read = read(fd, buf+i, io_size + 100);

		if (bytes_read != io_size) 
			printf("read too far: length %d\n", bytes_read);
	}
#endif


	/*
	printf("%f\n", (float) time_stats_get_avg(&stats));

	printf("Throughput: %3.3f MB\n",(float)((file_size_bytes) >> 20)
			/ (float) time_stats_get_avg(&stats));

	time_stats_print(&stats, (char *)"---------------");
	*/

	close(fd);

	return ;
}

void io_bench::Run(void)
{
	int ret;
	cout << "thread " << id << " start - ";
	cout << "test mode " << test_mode << endl;

	// verify file data
	if (test_mode == 'r') {
		//this->do_read();
		exit(-1);
	} 

	time_stats_init(&stats, 1);
	// Create files
	for (int i = 0; i < n_files; i++) {
		string filename;
		filename = test_dir_prefix"/fileset/file" + std::to_string(i);
		filenames.push_back(filename);

		cout << "create file " << filename << endl;

		time_stats_start(&stats);
		if ((ret = creat(filename.c_str(), 0600)) < 0) {
			perror("open");
			exit(1);
		}
		time_stats_stop(&stats);
	}

	printf("Throughput: %3.3f MB\n",(float)(file_size_bytes)
			/ (1024.0 * 1024.0 * (float) time_stats_get_avg(&stats)));

	time_stats_print(&stats, (char *)"---------------");

	// for (auto it : filenames)
	//         do_write((char *)it.c_str());

#if 0
	int i = 0;
	for (auto it : filenames) {
		if (i % 3 == 0) {
			cout << "delete file " << it << endl;
			ret = unlink((char *)it.c_str());
		}
		i++;
	}

	i = 0;
	for (auto it : filenames) {
		if (i % 3 == 0) {
			cout << "create file " << it << endl;
			ret = creat(it.c_str(), 0600);
			do_write((char *)it.c_str());
		}
		i++;
	}
#endif

	delete buf;

	return;
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
		<< " <w/r> <# of file> <# of thread>"
		<< endl;
}

int main(int argc, char *argv[])
{
	int n_threads, i;
	std::vector<io_bench *> io_workers;
	unsigned long file_size_bytes;
	unsigned int io_size = 16;
	int ret, n_files;
	struct timeval t_start,t_end,t_elap;
	struct stat f_stat;

	if (argc != 4) {
		io_bench::show_usage(argv[0]);
		exit(-1);
	}

	n_files = std::stoi(argv[2]);

	n_threads = std::stoi(argv[3]);

	file_size_bytes = 128;

	std::cout << "Total file size: " << file_size_bytes << " KB" << endl
		<< "io size: " << io_size << " KB" << endl
		<< "# of thread: " << n_threads << endl;

	for (i = 0; i < n_threads; i++) {
		io_workers.push_back(new io_bench(i, 
					file_size_bytes << 10,
					io_size << 10, n_files, *argv[1]));
	}

	for (auto it : io_workers) 
		it->prepare();

	for (auto it : io_workers) 
		it->Start();

	for (auto it : io_workers) 
		it->Join();

	fflush(stdout);
	fflush(stderr);

	shutdown_fs();

	return 0;
}
