#define _GNU_SOURCE //fallocate

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mlfs/mlfs_interface.h>

//fallocate, stat and unlink test.

#define BUF_SIZE (64UL << 10)
//#define BUF_COUNT 10000UL
#define BUF_COUNT 2000UL
#define FILE_NAME "/mlfs/fallocate"

int main(int argc, char ** argv)
{
	int fd1, fd2, fd3;
	int bytes, ret, i;
	char buffer[BUF_SIZE];
	int write_count;
	struct stat statbuf;

	init_fs();

	ret = mkdir("/mlfs/", 0600);

	if (ret < 0) {
		perror("mkdir\n");
		return 1;
	}

	printf("--- stat\n");
	fd1 = stat(FILE_NAME, &statbuf);

	if (fd1 != ENOENT)
		printf("WARNING: file %s exist. check unlink syscall\n",
				FILE_NAME);

	fd1 = creat(FILE_NAME, 0600);
	printf("fd1 %d\n", fd1);

	if (fd1 < 0) {
		perror("creat");
		return 1;
	}

	close(fd1);

	printf("--- fallocate\n");

	fd2 = open(FILE_NAME, O_RDWR, 0600);
	printf("fd2 %d\n", fd2);

	if (fd2 < 0) {
		perror("fallocate: open without O_RDWR");
		return 1;
	}

	printf("allocate length %lx\n", BUF_SIZE * BUF_COUNT);

	bytes = fallocate(fd2, 0, 0, BUF_SIZE * BUF_COUNT);

	if (bytes < 0) {
		perror("read");
		return 1;
	}

	close(fd2);

	unlink(FILE_NAME);

	printf("--- overwrite\n");

	//it should be fail to open the file
	//fd3 = open(FILE_NAME, O_RDWR, 0600);
	
	fd3 = open(FILE_NAME, O_RDWR|O_CREAT, 0600);

	if (fd3 < 0) {
		perror("overwrite: open with O_DRWR");
		return 1;
	}

	printf("fd3 %d\n", fd3);

	for (i = 0; i < BUF_SIZE; i++)
		buffer[i] = '0' + (i % 10);

	lseek(fd3, 0, SEEK_SET);

	for (i = 0; i < BUF_COUNT; i++)
		bytes = write(fd3, buffer, BUF_SIZE);

	write(fd3, buffer, strlen(buffer));

	close(fd3);

#if 0
	printf("--- read (verify overwrite)\n");

	fd3 = open(FILE_NAME, O_RDWR, 0600);  
	printf("fd3 %d\n", fd3);

	lseek(fd3, 0, SEEK_SET);  

	for (i = 0; i < BUF_COUNT; i++) {
		bytes = read(fd3, buffer, BUF_SIZE); 

		for (int j = 0; j < BUF_SIZE; j++)
			if (buffer[j] != '0' + (j % 10)) {
				printf("read data mismatch at %lx\n", 
						(i * BUF_COUNT) + j);
				exit(-1);
			}
	}

	printf("read data is verified\n");

	close(fd3);   
#endif

	unlink(FILE_NAME);

	shutdown_fs();

	return 0;
}
