#include <iostream>
#include <fstream>
#include <random>
#include <list>
#include <utility>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <mlfs/mlfs_interface.h>

#define KGRN  "\x1B[32m"
#define KRED  "\x1B[31m"
#define KNRM  "\x1B[0m"

#define N_BLOCKS 400
#define N_UPDATES 100
#define LARGE_BUF_SIZE (4096 * N_BLOCKS)
#define SMALL_BUF_SIZE (16 << 10)
#define FILE_SIZE (4UL << 30)

#define _min(a, b) ((a) < (b) ? (a) : (b))

#define FILENAME "/mlfs/partial_update"
//#define FILENAME "./ramfs/partial_update"

#define ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define ALIGN_MASK_FLOOR(x, mask) (((x)) & ~(mask))
#define ALIGN(x, a)  ALIGN_MASK((x), ((__typeof__(x))(a) - 1))
#define ALIGN_FLOOR(x, a)  ALIGN_MASK_FLOOR((x), ((__typeof__(x))(a) - 1))

//#define ALIGNED // this is much simpler testing case. used for debugging libfs.
//#define THROUGH_CHECK

using namespace std;

char small_buffer[SMALL_BUF_SIZE], large_buffer[LARGE_BUF_SIZE],
		 read_buffer[LARGE_BUF_SIZE];

#define HEXDUMP_COLS 8
static void hexdump(void *mem, unsigned int len)
{
	unsigned int i, j;

	/*  print column numbers */
	printf("          ");
	for(i = 0; i < HEXDUMP_COLS; i++) {
		if(i < len)
			printf("%02x ", i);
		else
			printf("	");
	}

	printf("\n");

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

static std::string hexdump_off_loc(uintptr_t start, unsigned int len, uintptr_t target)
{
	//cout << "find offending offset in dump. start: " << start << endl
	//       << " len " << len << " target " << target << endl;

	char buf[20];
	if(target < start || target >= (start + len))
		return NULL;

	unsigned int row = ALIGN_FLOOR(target-start, HEXDUMP_COLS);
	unsigned int col = target-start-row;
	sprintf(buf, "row 0x%06x col %02x", row, col);
	return std::string(buf);
}

int main(int argc, char *argv[])
{
	int fd, k, iteration = 30;
	int ret, use_trace_file = 0, offending_read_only = 0;
	unsigned long sum, correct_sum, updated_sum, offending_offset, offending_offset_aligned, i, j, bytes;
	char *verify_buffer, *check_buffer;
	char *trace_file_name = NULL;

	if (argc > 1) {
		use_trace_file = 1;
		trace_file_name = argv[1];
		if (argc > 2) {
			offending_read_only = 1;
			offending_offset = std::atol(argv[2]);
			offending_offset_aligned = ALIGN_FLOOR(offending_offset, 4096);
		}
	}

	init_fs();

	fd = creat(FILENAME, 0600);

	if (fd < 0) {
		perror("creat");
		return 1;
	}

	close(fd);

	for (i = 0; i < SMALL_BUF_SIZE; i++)
		small_buffer[i] = '3';

	for (i = 0; i < LARGE_BUF_SIZE; i++)
		large_buffer[i] = '2';

	verify_buffer = (char *)malloc(FILE_SIZE);
	if (verify_buffer == NULL) {
		cout << "fail to allocate verify buffer" << endl;
		exit(-1);
	}

#ifdef THROUGH_CHECK
	check_buffer = (char *)malloc(FILE_SIZE);
	if (check_buffer == NULL) {
		cout << "fail to allocate check buffer" << endl;
		exit(-1);
	}
#endif

	for (k = 0 ; k < iteration; k++) {
		cout << "--- Write initial data : iteration " << k << endl;

		fd = open(FILENAME, O_RDWR| O_CREAT, 0600);

		if (fd < 0) {
			perror("write: open without O_CREAT");
			return 1;
		}

		/*
		for (i = 0; i < (FILE_SIZE / LARGE_BUF_SIZE) + 1; i++) {
			unsigned long io_size = _min(FILE_SIZE - (i * LARGE_BUF_SIZE), LARGE_BUF_SIZE);

			bytes = write(fd, large_buffer, io_size);
		}
		*/

		for (i = 0; i < (FILE_SIZE / LARGE_BUF_SIZE); i++) {
			bytes = write(fd, large_buffer, LARGE_BUF_SIZE);
		}

		if (FILE_SIZE % LARGE_BUF_SIZE != 0) {
			bytes = write(fd, large_buffer, FILE_SIZE % LARGE_BUF_SIZE);
		}

		close(fd);

		while(make_digest_request_async(100) == -EBUSY);
		wait_on_digesting();

		memset(verify_buffer, '2', FILE_SIZE);

		printf("--- Update data partially\n");

		assert(N_UPDATES < N_BLOCKS);

		fd = open(FILENAME, O_RDWR, 0600);

#ifdef ALIGNED
		// update beginning 128 B data for each 4 KB blocks
		for (i = 0; i < N_UPDATES; i++) {
			lseek(fd, i * LARGE_BUF_SIZE, SEEK_SET);
			bytes = write(fd, small_buffer, SMALL_BUF_SIZE);
		}

		updated_sum = (N_UPDATES * SMALL_BUF_SIZE);
#else
		list<pair<uint64_t, uint64_t>> io_list;
		updated_sum = 0;

		if (use_trace_file) {
			cout << "read update trace from file" << endl;
			ifstream trace_input;
			trace_input.open(trace_file_name);

			while(trace_input) {
				pair<uint64_t, uint64_t> t;
				trace_input >> t.first >> t.second;

				if (t.first == 0)
					continue;
				io_list.push_back(t);

				updated_sum += t.first;
			}

			trace_input.close();
		} else {
			random_device rd;
			mt19937 mt(rd());
			//mt19937 mt;
			uniform_int_distribution<uint64_t> dist_iosize(1, SMALL_BUF_SIZE);
			uniform_int_distribution<uint64_t> dist_offset(0, FILE_SIZE - SMALL_BUF_SIZE);

			for (i = 0; i < N_UPDATES; i++) {
				pair<uint64_t, uint64_t> t;
				t.first = dist_iosize(mt);
				t.second = dist_offset(mt);
				//t.second = ALIGN_FLOOR(dist_offset(mt), 4096);

				io_list.push_back(t);
				updated_sum += t.first;
			}
		}

		for (auto it: io_list) {
			//bytes = pwrite(fd, small_buffer, it.first, it.second);
			lseek(fd, it.second, SEEK_SET);
			bytes = write(fd, small_buffer, it.first);
			assert(bytes == it.first);

			if(offending_read_only) {
				if(offending_offset >= it.second && offending_offset < (it.first + it.second)) {
					void* buffer_to_dump = small_buffer + it.second - offending_offset_aligned;
					cout << "replayed offending write for offset "
						<< offending_offset << " [offset loc: "
						<< hexdump_off_loc(offending_offset_aligned,
								4096, offending_offset) << "]" << endl;
					hexdump((void*)buffer_to_dump, 4096);
				}
			}
			//cout << "update: offset " << it.second << " size " << it.first << endl;

			memmove(verify_buffer + it.second, small_buffer, it.first);
		}
#endif

		close(fd);

		// If comment out the following digest request,
		// This program will test correctness of patching: read blocks from shared area
		// and patching partial updates from the update log.
#if 0
		while(make_digest_request_async(100) == -EBUSY);
		wait_on_digesting();
#endif

		printf("--- verify updated buffer (after digest)\n");

		fd = open(FILENAME, O_RDONLY, 0600);
		if (fd < 0) {
			perror("read: open without O_CREAT");
			return 1;
		}

		sum = 0;

		lseek(fd, 0, SEEK_SET);

#ifdef THROUGH_CHECK
		char *_check_buffer = check_buffer;
#endif

		// verify data
		for (i = 0; i < (FILE_SIZE / LARGE_BUF_SIZE) + 1 ; i++) {
			unsigned long io_size = _min(FILE_SIZE - (i * LARGE_BUF_SIZE), LARGE_BUF_SIZE);
			unsigned long previous_sum = sum;

			if(offending_read_only) {
				if(offending_offset < i*LARGE_BUF_SIZE ||
					       	offending_offset >= (i * LARGE_BUF_SIZE + io_size)) {
					lseek(fd, io_size, SEEK_CUR);
					continue;
				}
			}

			memset(read_buffer, 0, LARGE_BUF_SIZE);
			bytes = read(fd, read_buffer, io_size);
			if (bytes != io_size) {
				printf("read %lu - expect %lu\n", bytes, io_size);
				exit(-1);
			}

			if(offending_read_only) {
				void* buffer_to_dump = read_buffer + i*LARGE_BUF_SIZE - offending_offset_aligned;
				cout << "replayed offending read for offset "
					<< offending_offset << " [offset loc: "
					<< hexdump_off_loc(offending_offset_aligned,
							4096, offending_offset) << "]" << endl;
				hexdump((void*)buffer_to_dump, 4096);
				return 1;
			}
#ifdef THROUGH_CHECK
			memmove(_check_buffer, read_buffer, io_size);
			_check_buffer += io_size;
#endif

			#pragma omp parallel for reduction(+:sum)
			for (j = 0; j < bytes; j++)
				sum += read_buffer[j] - '0';

#ifdef ALIGNED
			if (i < N_UPDATES) {
				if (sum - previous_sum != (LARGE_BUF_SIZE * 2) + SMALL_BUF_SIZE) {
					printf("File intergrity is wrong: offset %lu ", i * LARGE_BUF_SIZE);
					printf("sum %lu - expect %lu\n",
							sum - previous_sum, (LARGE_BUF_SIZE * 2UL) + SMALL_BUF_SIZE);
					//exit(-1);
				}
			}
#endif
		}

#ifdef THROUGH_CHECK
		cout << "Doing byte-by-byte checks to find file offset that having a problem." << endl;
		for (i = 0; i < FILE_SIZE; i++) {
			int ret;
			ret = memcmp(check_buffer + i, verify_buffer + i, 1);
			if (ret != 0) {
				cout << "Contents mismatch: offending offset " << i << endl;
				printf("Expected %c - Found %c\n", *(verify_buffer + i), *(check_buffer + i));
				exit(1);
			}
		}
#endif

#ifdef ALIGNED
		correct_sum = (FILE_SIZE * 2) + updated_sum;
#else
		correct_sum = 0;
		#pragma omp parallel for reduction(+:correct_sum)
		for (i = 0; i < FILE_SIZE; i++)
			correct_sum += verify_buffer[i] - '0';

		assert(correct_sum == (FILE_SIZE * 2) + updated_sum);
#endif

		printf("verifying buffer.. ");

		if (sum != correct_sum) {
			printf(KRED "File integrity is wrong : sum %lu - expect %lu\n" KNRM,
					sum, correct_sum);

			if (correct_sum > sum)
				printf("Missing sum = %lu\n", correct_sum - sum);
			else
				printf("Surplusing sum = %lu\n", sum - correct_sum);

			printf("LARGE_BUF_SIZE  = %u\n", LARGE_BUF_SIZE);
			printf("UPDATE_BUF_SIZE = %u\n", SMALL_BUF_SIZE);
			printf("N_UPDATES       = %u\n", N_UPDATES);
			printf("SUM_OF_UPDATED  = %lu\n", updated_sum);

#ifndef ALIGNED
			cout << "generating update trace causing the bug" << endl;
			ofstream trace_file;
			trace_file.open("./update_trace.txt");

			for (auto it: io_list)
				trace_file << it.first << " " << it.second << endl;

			trace_file.close();
#endif

			exit(1);
		} else
			printf(KGRN "OK\n" KNRM);
	}

	close(fd);

	shutdown_fs();

	return 0;
}
