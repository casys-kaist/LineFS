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
#include <fcntl.h>

#include "time_stat.h"

#ifdef DMFS
const char test_dir_prefix[] = "/mnt/dmfs/";
#else
const char test_dir_prefix[] = "./";
#endif

typedef unsigned long addr_t;

char test_file[100] = "\0";

typedef enum {SEQ_WRITE, SEQ_READ, SEQ_WRITE_READ, RAND_WRITE, RAND_READ} test_t;
static test_t test_type;

typedef enum {EXT4, NVM, SPDK} storage_t;

unsigned long write_size_bytes;

/**
 * Necessary stuff for async: callback, transaction struct, block to write
 */
typedef struct {
	unsigned int volatile issued;
	unsigned int volatile done;
} async_tx_t;

static async_tx_t *async_tx;
static int first_block;
static int next_block = 0;

void usage(const char *prog, FILE *out)
{
	fprintf(out, "usage %s <wr/sr/sw/rr/rw> <filename> "
			"<size: X{G,M,K,P}, eg: 100M> <IO size, e.g.: 4K>\n", prog);

	exit(out == stderr);
}

unsigned long str_to_size(char* str)
{
	/* magnitude is last character of size */
	char size_magnitude = str[strlen(str)-1];
	/* erase magnitude char */
	str[strlen(str)-1] = 0;
	unsigned long total_size_bytes = strtoull(str, NULL, 0);
	switch(size_magnitude) {
		case 'g':
		case 'G':
			total_size_bytes *= 1024;
		case 'm':
		case 'M':
			total_size_bytes *= 1024;
		case '\0':
		case 'k':
		case 'K':
			total_size_bytes *= 1024;
		case 'b':
		case 'B':
			break;
		case 'p':
		case 'P':
			total_size_bytes *= 4;
			break;
		default:
			usage("", stderr);
			break;
	}
	return total_size_bytes;
}

#define HEXDUMP_COLS 8
static void hexdump(void *mem, unsigned int len)
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

int main(int argc, char *argv[])
{
	int fd, ret;
	int do_fflush = 0, do_fsync = 0;
	unsigned long total_size_bytes, i;
	unsigned long random_range;
	struct timeval t_start,t_end,t_elap;
	float sec;
	struct stat f_stat;
	unsigned int io_size = 0;

	ssize_t (*write_fn)(int, const void*, size_t);
	off_t (*seek_fn)(int, off_t, int);

	if (argc != 5) {
		usage(argv[0], stderr);
	}

	srand(time(NULL));   

	async_tx = malloc(sizeof(async_tx_t));

	/**
	 * Calculate the op size in bytes
	 */
	write_size_bytes = str_to_size(argv[4]);
	printf("Total size of each operation is %lu\n", write_size_bytes);
	total_size_bytes = str_to_size(argv[3]);
	printf("Total size of the test is %lu\n", total_size_bytes);
	random_range = total_size_bytes/write_size_bytes;

	/**
	 * Check the mode to bench: read or write and type
	 */
	if (!strcmp(argv[1], "sr")){
		test_type = SEQ_READ;
		printf("Sequential read\n");
	}
	else if (!strcmp(argv[1], "sw")) {
		test_type = SEQ_WRITE;
		printf("Sequential write\n");
	}
	else if (!strcmp(argv[1], "wr")) {
		test_type = SEQ_WRITE_READ;
		printf("Sequnetial write and read\n");
	}
	else if (!strcmp(argv[1], "rw")) {
		test_type = RAND_WRITE;
		printf("Random write\n");
	}
	else if (!strcmp(argv[1], "rr")) {
		test_type = RAND_READ;
		printf("Random read\n");
	}
	else { 
		usage(argv[0], stderr);
	}

	write_fn = write;
	seek_fn = lseek;

	/**
	 * Copy filename to test_file
	 */
	strncpy(test_file, argv[2], strlen(argv[2]));

	do_fflush = do_fsync = 1;

	char file_path[256];
	unsigned int len = sprintf(file_path,"%s%s", test_dir_prefix, test_file);

	char* buf = malloc(total_size_bytes);

	if(test_type == SEQ_READ || test_type == RAND_READ)
		goto READ_START;

WRITE_START:

	for (unsigned long i = 0; i < total_size_bytes; i++) {
		buf[i] = '0' + (i % 10);
	}

	/**                                                                                           
	 *  Run each test 3 so we have variance                                                       
	 */
#define N_TESTS 1
	struct time_stats stats;

	time_stats_init(&stats, N_TESTS);
	for(int test = 0 ; test < N_TESTS ; test++) {

		fd = open(file_path, O_RDWR | O_CREAT| O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd < 0) {
			err(1, "open");
		}

		/**
		 * If its random write and EXT4, we preallocate the file so we can do
		 * random writes
		 */
		if(test_type == RAND_WRITE) { 
			posix_fallocate(fd, 0, total_size_bytes);
		}

		/* start timer */
		//gettimeofday(&t_start,NULL);
		time_stats_start(&stats);

		/**
		 *  Do (size/rw-int) write operations
		 */
		int bytes_written;
		for (unsigned long i = 0; i < total_size_bytes; i += io_size) {
			if(test_type == RAND_WRITE) {
				unsigned int rand_io_offset = rand() % random_range;
				(*seek_fn)(fd, rand_io_offset*write_size_bytes, SEEK_SET);
			}

ISSUE_WRITE:
			if (i + write_size_bytes > total_size_bytes)
				io_size = total_size_bytes - i;
			else
				io_size = write_size_bytes;

			bytes_written = (*write_fn)(fd, buf+i, io_size);

			if(bytes_written == -1 && errno == EBUSY) {
				goto ISSUE_WRITE;
			}
			else if (bytes_written != io_size) {
				printf("write request %lu received len %d\n",
						write_size_bytes, bytes_written);
				errx(1, "write");
			}
		}

		/**
		 * EXT4 sync/flush
		 */
		if (do_fflush) {
			fflush(fopen(file_path, "w+"));
		}
		if (do_fsync) {
			printf("do_sync\n");
			fsync(fd);
		}

		time_stats_stop(&stats);

		/**
		 * clean up after testing and quit
		 */
		close(fd);

		//end of tests loop
	}

	time_stats_print(&stats, "");

	printf("Throughput: %3.3f MB\n",(float)(total_size_bytes)
			/ (1024.0 * 1024.0 * (float) time_stats_get_avg(&stats)));

	if (test_type == SEQ_WRITE_READ)
		goto READ_START;

	free(buf);

#ifdef MLFS
	//make_digest_request_async(0);
#endif

	fflush(stdout);
	fflush(stderr);
	//pause();

	return 0;

READ_START:
	printf("try to read file %s\n", file_path);
	if ((fd = open(file_path, O_RDWR)) < 0)
		err(1, "open");

	/*
	fstat(fd,&f_stat);
	total_size_bytes = f_stat.st_size;
	*/

	printf("File size %lu bytes\n", (unsigned long)total_size_bytes);

	for(unsigned long i = 0; i < total_size_bytes; i++)
		buf[i] = 1;

	time_stats_init(&stats, 1);

	time_stats_start(&stats);

	for (i = 0; i < total_size_bytes ; i += write_size_bytes) {
		if (i + write_size_bytes > total_size_bytes)
			io_size = total_size_bytes - i;
		else
			io_size = write_size_bytes;

		ret = read(fd, buf + i, io_size);
		if (ret != write_size_bytes) {
			printf("read size mismatch at %lu : return %d, request %lu\n",
					i, ret, write_size_bytes);
			// exit(-1);
		}
	}

	printf("%lx\n", i);

	time_stats_stop(&stats);

	/*
	for (unsigned long i = 0; i < total_size_bytes; i++) {
		int bytes_read = read(fd, buf+i, write_size_bytes + 100);

		if (bytes_read != write_size_bytes) {
			printf("read too far: length %d\n", bytes_read);
		}
	}
	*/

	// Read data integrity check.
	for (unsigned long i = 0; i < total_size_bytes; i++) {
		if (buf[i] != '0' + (i % 10)) {
			hexdump(buf + i, 256);
			printf("read data mismatch at %lx\n", i);
			printf("expected %c read %c\n", (int)('0' + (i % 10)), buf[i]);
			exit(-1);
		}
	}

	printf("Read data matches\n");

	time_stats_print(&stats, "");

	printf("%f\n", (float) time_stats_get_avg(&stats));

	printf("Throughput: %3.3f MB\n",(float)((total_size_bytes) >> 20)
			/ (float) time_stats_get_avg(&stats));

	close(fd);

	free(buf);

#ifdef MLFS
	//make_digest_request(0);
#endif
	fflush(stdout);
	fflush(stderr);
	//pause();
	return 0;
}
