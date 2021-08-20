#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUF_SIZE 128
#define LARGE_BUF_SIZE (BUF_SIZE * 100)
#define N_IO 20

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

int main(int argc, char ** argv)
{
    int fd, i, j;
    int bytes, ret;
    char small_buffer[BUF_SIZE], large_buffer[LARGE_BUF_SIZE];
    int write_count;
    
    ret = mkdir("/mnt/dmfs", 0600);

	/*
	if (ret < 0) {
		perror("mkdir\n");
		return 1;
	}
	*/

    printf("--- mkdir\n");
    
    fd = creat("/mnt/dmfs/testfile", 0600);

    if (fd < 0) {
        perror("creat");
        return 1;
    }

    close(fd);

    printf("--- write (io size %d byte)\n", BUF_SIZE);

    fd = open("/mnt/dmfs/testfile", O_RDWR| O_CREAT, 0600);

    if (fd < 0) {
        perror("write: open without O_CREAT");
        return 1;
    }

	for (i = 0; i < BUF_SIZE; i++)
		small_buffer[i] = '0' + (i % 5);

	for (i = 0; i < LARGE_BUF_SIZE; i++)
		large_buffer[i] = '0' + (i % 7);

	bytes = write(fd, small_buffer, BUF_SIZE);

	close(fd);

    fd = open("/mnt/dmfs/testfile", O_RDWR, 0600);

	// append large_buffer
	lseek(fd, BUF_SIZE, SEEK_SET);

	bytes = write(fd, large_buffer, LARGE_BUF_SIZE);

	close(fd);

	printf("--- verify\n");

    fd = open("/mnt/dmfs/testfile", O_RDONLY, 0600);
    if (fd < 0) {
        perror("read: open without O_CREAT");
        return 1;
    }

	memset(small_buffer, 0, BUF_SIZE);
	bytes = read(fd, small_buffer, BUF_SIZE);
	if (bytes != BUF_SIZE) {
		perror("read");
		return 1;
	}

	printf("verifying small buffer.. ");

	// verify data of small buffer
	for (j = 0; j < BUF_SIZE; j++) {
		if (small_buffer[j] != '0' + (j % 5)) {
			hexdump(&small_buffer[j], 64);
			printf("read data does not match: offset %u\n", j);
			exit(-1);
		}
	}

	printf("OK\n");

	memset(large_buffer, 0, LARGE_BUF_SIZE);
	bytes = read(fd, large_buffer, LARGE_BUF_SIZE);

	printf("verifying large buffer.. ");

	// verify data of small buffer
	for (j = 0; j < LARGE_BUF_SIZE; j++) {
		if (large_buffer[j] != '0' + (j % 7)) {
			printf("\n --------- from offset %u  ---------- \n", j);
			hexdump(&large_buffer[j], 128);
			printf("read data does not match: offset %u\n", j);
			printf("expected %c, got %c\n", '0' + (j % 7), large_buffer[j]);
			exit(-1);
		}
	}

	printf("OK\n");

	close(fd);

    return 0;
}
