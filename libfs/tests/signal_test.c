#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mlfs/mlfs_interface.h>

#define BUF_SIZE 128
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

void sigShutdownHandler(int sig)
{
	exit(0);
}

int main(int argc, char ** argv)
{
    int fd, i, j;
    int bytes, ret;
    char buffer[BUF_SIZE], str[BUF_SIZE];
	int write_count;
	struct sigaction act;

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sigShutdownHandler;
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	init_fs();

	ret = mkdir("/mlfs/", 0600);

	/*
	if (ret < 0) {
		perror("mkdir\n");
		return 1;
	}
	*/

    printf("--- mkdir\n");
    
    fd = open("/mlfs/testfile", O_RDONLY, 0600);
    if (fd < 0) {
		goto skip;
    }

	for (i = 0; i < N_IO; i++) {
		memset(buffer, 0, BUF_SIZE);
		bytes = read(fd, buffer, BUF_SIZE);

		if (bytes != BUF_SIZE) {
			perror("read");
			return 1;
		}

		// verify data
		for (j = 0; j < BUF_SIZE; j++) {
			if (buffer[j] != '0' + (i % 9)) {
				hexdump(buffer, 256);
				printf("read data does not match: io iter %u, offset %u\n",i, j);
				exit(-1);
			}
		}
	}

	printf("read matched write data\n");

skip:
    fd = creat("/mlfs/testfile", 0600);

    if (fd < 0) {
        perror("creat");
        return 1;
    }

    close(fd);

    printf("--- write (io size %d byte)\n", BUF_SIZE);

    fd = open("/mlfs/testfile", O_RDWR| O_CREAT, 0600);

    if (fd < 0) {
        perror("write: open without O_CREAT");
        return 1;
    }

	for (i = 0; i < N_IO; i++) {
		memset(buffer, '0' + (i % 9), BUF_SIZE);
		bytes = write(fd, buffer, BUF_SIZE);

		if (bytes != BUF_SIZE) {
			perror("write");
			return 1;
		}
	}

	close(fd);

	printf("--- read (verify)\n");

    fd = open("/mlfs/testfile", O_RDONLY, 0600);
    if (fd < 0) {
        perror("read: open without O_CREAT");
        return 1;
    }

	for (i = 0; i < N_IO; i++) {
		memset(buffer, 0, BUF_SIZE);
		bytes = read(fd, buffer, BUF_SIZE);

		if (bytes != BUF_SIZE) {
			perror("read");
			return 1;
		}

		// verify data
		for (j = 0; j < BUF_SIZE; j++) {
			if (buffer[j] != '0' + (i % 9)) {
				hexdump(buffer, 256);
				printf("read data does not match: io iter %u, offset %u\n",i, j);
				exit(-1);
			}
		}
	}

	printf("read matched write data\n");

	kill(getpid(), SIGTERM);
	close(fd);

	shutdown_fs();

    return 0;
}
