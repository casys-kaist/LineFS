#define _GNU_SOURCE //fallocate

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

//fallocate, stat and ftruncate test.

#define BUF_SIZE (64UL << 10)
#define BUF_COUNT 2UL
#define FILE_NAME "/mnt/dmfs/ftrunc"

int main(int argc, char ** argv)
{
	int fd1, fd2;
	int bytes, ret, i;
	char buffer[BUF_SIZE];
	int write_count;
	unsigned long file_size;
	struct stat statbuf;

	ret = mkdir("/mnt/dmfs/", 0600);

	if (ret < 0) {
		perror("mkdir\n");
		return 1;
	}

	printf("--- stat\n");
	ret = stat(FILE_NAME, &statbuf);

	printf("%d\n", ret);

	if (ret == -1) {
		printf("File does not exist. create a new file\n");

		printf("--- fallocate\n");

		fd1 = creat(FILE_NAME, 0600);
		printf("fd1 %d\n", fd1);

		if (fd1 < 0) {
			perror("creat");
			return 1;
		}

		bytes = fallocate(fd1, 0, 0, BUF_SIZE * BUF_COUNT);
	}

	ret = stat(FILE_NAME, &statbuf);
	if (ret == ENOENT) {
		printf("File does not exist even after fallocate\n");
		exit(-1);
	}

	file_size = statbuf.st_size;

	close(fd1);

	printf("--- ftruncte\n");
	fd2 = open(FILE_NAME, O_RDWR, 0600);
	printf("fd2 %d\n", fd2);

	if (fd2 < 0) {
		perror("ftruncate: open without O_RDWR");
		return 1;
	}

	// Non-zero ftruncate.
	ftruncate(fd2, 100);

	// Make zero-length file.
	ftruncate(fd2, 0);

	ret = stat(FILE_NAME, &statbuf);

	if (statbuf.st_size != 0) {
		printf("ftruncate was not applied: file size %lu\n", statbuf.st_size);
		exit(-1);
	}

	unlink(FILE_NAME);

	return 0;
}
