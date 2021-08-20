#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

typedef enum {SEQ_WRITE, SEQ_READ, SEQ_WRITE_READ, RAND_WRITE, RAND_READ, NONE} test_t;

typedef enum {FS} test_mode_t;

typedef unsigned long addr_t;

class io_fork
{
	public:
		io_fork(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
				test_t _test_type, test_mode_t _test_mode);

		int id;
		unsigned long file_size_bytes;
		unsigned int io_size;
		test_t test_type;
		test_mode_t test_mode;
		string test_file;
		int do_fsync;
		char *buf;
		struct time_stats stats;

		void prepare(void);

		void do_read(void);
		void do_write(void);

		void Run(void);

		// util methods
		static unsigned long str_to_size(char* str);
		static test_t get_test_type(char *);
		static test_mode_t get_test_mode(char *);
		static void hexdump(void *mem, unsigned int len);
		static void show_usage(const char *prog);
};

io_fork::io_fork(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
		test_t _test_type, test_mode_t _test_mode) 
	: id(_id), file_size_bytes(_file_size_bytes), io_size(_io_size), 
	test_type(_test_type), test_mode(_test_mode)
{
	test_file.assign(test_dir_prefix);
	test_file += "/file" + std::to_string(id);
}

void io_fork::prepare(void)
{
	int ret;

	ret = mkdir(test_dir_prefix, 0777);

	if (ret < 0 && errno != EEXIST) { 
		perror("mkdir\n");
		exit(-1);
	}

	buf = new char[file_size_bytes];

	srand(time(NULL));   
}

void io_fork::do_write(void)
{
	int fd;
	unsigned long random_range;

	random_range = file_size_bytes / io_size;

WRITE_START:
	//async_tx = malloc(sizeof(async_tx_t));

	for (unsigned long i = 0; i < file_size_bytes; i++) {
		buf[i] = '0' + (i % 10);
	}

	/**                                                                                           
	 *  Run each test 3 so we have variance                                                       
	 */
#define N_TESTS 1

	time_stats_init(&stats, N_TESTS);
	for (int test = 0 ; test < N_TESTS ; test++) {

		fd = open(test_file.c_str(), O_RDWR | O_CREAT| O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd < 0) {
			err(1, "open");
		}

		/**
		 * If its random write and FS, we preallocate the file so we can do
		 * random writes
		 */
		if(test_type == RAND_WRITE && test_mode == FS) { 
			posix_fallocate(fd, 0, file_size_bytes);
		}

		/* start timer */
		//gettimeofday(&t_start,NULL);
		time_stats_start(&stats);

		/**
		 *  Do (size/rw-int) write operations
		 */
		int bytes_written;
		for (unsigned long i = 0; i < file_size_bytes; i += io_size) {
			if(test_type == RAND_WRITE) {
				// TODO: fix this with c11 rand function.
				unsigned int rand_io_offset = rand() % random_range;
				lseek(fd, rand_io_offset*io_size, SEEK_SET);
			}

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

		/**
		 * FS sync/flush
		 */
		if (test_mode == FS && do_fsync) {
			printf("do_sync\n");
			fsync(fd);
		}

		time_stats_stop(&stats);

		/**
		 * clean up after testing and quit
		 */
		if(test_mode == FS) {
			close(fd);
		}

		//end of tests loop
	}

	time_stats_print(&stats, (char *)"---------------");

	printf("Throughput: %3.3f MB\n",(float)(file_size_bytes)
			/ (1024.0 * 1024.0 * (float) time_stats_get_avg(&stats)));

#ifdef MLFS
	//make_digest_request(0);
#endif
	//pause();

	return ;
}

void io_fork::do_read(void)
{
	int fd;
	unsigned long i;
	int ret;

READ_START:
	printf("try to read file %s\n", test_file.c_str());
	if ((fd = open(test_file.c_str(), O_RDWR)) < 0)
		err(1, "open");

	/*
	   fstat(fd,&f_stat);
	   file_size_bytes = f_stat.st_size;
	   */

	printf("File size %lu bytes\n", (unsigned long)file_size_bytes);

	for(unsigned long i = 0; i < file_size_bytes; i++)
		buf[i] = 1;

	time_stats_init(&stats, 1);

	time_stats_start(&stats);

	for (i = 0; i < file_size_bytes ; i += io_size) {
		if (i + io_size > file_size_bytes)
			io_size = file_size_bytes - i;
		else
			io_size = io_size;

		ret = read(fd, buf + i, io_size);
		/*
		   if (ret != io_size) {
		   printf("read size mismatch: return %d, request %lu\n",
		   ret, io_size);
		   }
		   */
	}

	printf("%lx\n", i);

	time_stats_stop(&stats);

	/*
	   for (unsigned long i = 0; i < file_size_bytes; i++) {
	   int bytes_read = read(fd, buf+i, io_size + 100);

	   if (bytes_read != io_size) {
	   printf("read too far: length %d\n", bytes_read);
	   }
	   }
	   */

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

	time_stats_print(&stats, (char *)"---------------");

	printf("%f\n", (float) time_stats_get_avg(&stats));

	printf("Throughput: %3.3f MB\n",(float)((file_size_bytes) >> 20)
			/ (float) time_stats_get_avg(&stats));

	close(fd);

#ifdef MLFS
	//make_digest_request(0);
#endif
	//pause();

	return ;
}

void io_fork::Run(void)
{
	cout << "thread " << id << " start - ";
	cout << "file: " << test_file << endl;

	if (test_type == SEQ_READ || test_type == RAND_READ)
		this->do_read();
	else 
		this->do_write();

	if (test_type == SEQ_WRITE_READ)
		this->do_read();

	delete buf;

	cout << "IO operation is done" << endl;

	return;
}

/*
   void io_fork::Join(void)
   {
   cout << CThread::done << endl;
   CThread::Join();
   }
*/

unsigned long io_fork::str_to_size(char* str)
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
		case 'b':
		case 'B':
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

test_t io_fork::get_test_type(char *test_type)
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
	else { 
		show_usage("iobench");
		cerr << "unsupported test type" << test_type << endl;
		exit(-1);
	}
}

test_mode_t io_fork::get_test_mode(char *test_mode)
{
	test_mode_t storage_type = FS;
	if (!strcmp(test_mode, "fs")) {
		storage_type = FS;
	} else {
		show_usage("iobench");
		cerr << "unsupported test mode " << test_mode << endl;
		exit(-1);
	}

	return storage_type;
}


#define HEXDUMP_COLS 8
void io_fork::hexdump(void *mem, unsigned int len)
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

void io_fork::show_usage(const char *prog)
{
	std::cerr << "usage: " << prog  
		<< " <wr/sr/sw/rr/rw> <fs>" 
		<< " <size: X{G,M,K,P}, eg: 100M> <IO size, e.g.: 4K>"
		<< endl;
}

int main(int argc, char *argv[])
{
	std::vector<io_fork *> io_workers;
	unsigned long file_size_bytes;
	unsigned int io_size = 0;
	pid_t pid;

	if (argc != 5) {
		io_fork::show_usage(argv[0]);
		exit(-1);
	}

	file_size_bytes = io_fork::str_to_size(argv[3]);
	io_size = io_fork::str_to_size(argv[4]);

	std::cout << "Total file size: " << file_size_bytes << "B" << endl
		<< "io size: " << io_size << "B" << endl;

	io_workers.push_back(new io_fork(1, 
				file_size_bytes,
				io_size,
				io_fork::get_test_type(argv[1]),
				io_fork::get_test_mode(argv[2])));

#ifdef MLFS
	init_fs();
	io_workers[0]->do_fsync = 0;
#else
	io_workers[0]->do_fsync = 1;
#endif

	pid = fork(); 

	if (pid == 0) {
		// child process
		for (auto it : io_workers) {
			it->prepare();
			it->Run();
		}
	} else if (pid < 0) {
		cerr << "fork failed" << endl;
		exit(-1);
	} 
	
	if (pid > 0) {
		int status;

		cout << "waiting child process" << endl;

		waitpid(pid, &status, 0);

		if (status == 1)
			cout << "child process terminated with an error" << endl;
		else
			cout << "child process terminated normally" << endl;

#ifdef MLFS
		shutdown_fs();
#endif
		fflush(stdout);
		fflush(stderr);
	}

	return 0;
}
