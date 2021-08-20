#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mlfs/mlfs_interface.h>

//rsync test.

#define IO_SIZE (12UL << 10) //12KB io
#define FILE_SIZE (40UL << 10) //40KB files 
#define REPEAT 20000
#define TEMP_FILE "/mlfs/temp"
#define DURABLE_FILE "/mlfs/durable"

int open_write_close(char file_name[])
{
	int fd, ret;
	char buf[FILE_SIZE];
	unsigned int io_size;
	unsigned long cur_offset;

	io_size = IO_SIZE;

	fd = open(file_name, O_RDWR | O_CREAT, 0600);
	//printf("rsync: opening file %s with fd[%d]\n", file_name, fd);

	if (fd < 0) {
		perror("rsync: failed to create and/or open file");
		exit(-1);
	}

	lseek(fd, 0, SEEK_SET);

	for (unsigned long i = 0; i < FILE_SIZE; i += io_size) {
		if (i + io_size > FILE_SIZE)
			io_size = FILE_SIZE - i;
		else
			io_size = IO_SIZE;

#ifdef VERIFY
		for (int j = 0; j < io_size; j++) {
			cur_offset = i + j;	
			buf[j] = '0' + (cur_offset % 10);
		}
#endif
		ret = write(fd, buf, io_size);

		if (ret != io_size) {
			printf("write request %u received len %d\n",
					io_size, ret);
			exit(-1);
		}
	}

	close(fd);
	return fd;
}

int main(int argc, char ** argv)
{
	int fd, bytes, ret;
	int write_count;
	struct stat statbuf;

	if(sizeof(FILE_SIZE) < sizeof(IO_SIZE)) {
		perror("invalid setting. FILE_SIZE must be >= IO_SIZE");
		exit(-1);
	}

	init_fs();

	ret = mkdir("/mlfs/", 0600);

	if (ret < 0) {
		perror("mkdir\n");
		return 1;
	}

	printf("--- stat\n");
	fd = stat(TEMP_FILE, &statbuf);

	if (errno != ENOENT)
		printf("WARNING: file %s exist. fd is: %d. check unlink syscall\n",
				TEMP_FILE, fd);

	printf("--- stat\n");
	fd = stat(DURABLE_FILE, &statbuf);

	if (errno != ENOENT)
		printf("WARNING: file %s exist. fd is: %d. check unlink syscall\n",
				DURABLE_FILE, fd);




	for(int i = 0; i < REPEAT; i++) {

		//printf("[benchmark: rsync_test] -- starting trial no %d\n", i+1);
		fd = open_write_close(TEMP_FILE);

		//delete file
		unlink(TEMP_FILE);

		//at this point, all previous entries should be discarded from log
		//when coalescing
		fsync(fd);
		open_write_close(DURABLE_FILE);

		//printf("Finished iter[%d] of total[%d]\n", i+1, REPEAT);
	}

	unlink(DURABLE_FILE);

#if 0
	printf("--- file integrity check \n");

	fd3 = open(FILE_NAME, O_RDWR, 0600);  
	printf("fd3 %d\n", fd3);

	lseek(fd3, 0, SEEK_SET);  

	for (i = 0; i < FILE_COUNT; i++) {
		bytes = read(fd3, buffer, IO_SIZE); 

		for (int j = 0; j < IO_SIZE; j++)
			if (buffer[j] != '0' + (j % 10)) {
				printf("read data mismatch at %lx\n", 
						(i * FILE_COUNT) + j);
				exit(-1);
			}
	}

	printf("read data is verified\n");

	close(fd3);   

	unlink(FILE_NAME);
#endif
	printf("[benchmark: rsync_test] -- shutting down fs\n");
	shutdown_fs();

	return 0;
}
