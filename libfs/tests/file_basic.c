#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mlfs/mlfs_interface.h>

#define BUF_SIZE 4096

int main(int argc, char ** argv)
{
    int fd1, fd2, fd3;
    int bytes, ret;
    char buffer[BUF_SIZE], str[BUF_SIZE];
	int write_count;

	init_fs();

	/*
		 ret = mkdir("/mlfs/", 0600);

		 if (ret < 0) {
		 perror("mkdir\n");
		 return 1;
		 }

		 printf("--- mkdir\n");

		 fd1 = creat("/mlfs/testfile", 0600);

		 if (fd1 < 0) {
		 perror("creat");
		 return 1;
		 }

		 write(fd1, "Hello World\n", 12);
		 close(fd1);

		 printf("--- creat/write/close\n");

		 fd2 = open("/mlfs/testfile", O_RDONLY, 0600);

		 if (fd2 < 0) {
		 perror("open without O_CREAT");
		 return 1;
		 }

		 bytes = read(fd2, buffer, BUF_SIZE);

		 if (bytes < 0) {
		 perror("read");
		 return 1;
		 }

		 buffer[11] = 0;
		 printf("read from file: %s\n", buffer);
		 close(fd2);
		 unlink("testfile");

		 printf("--- open(RDONLY)/read/unlink/close\n");
	*/

    fd3 = open("/mlfs/testfile", O_RDWR|O_CREAT, 0600);

    if (fd3 < 0) {
        perror("open with O_CREAT");
        return 1;
    }
    bytes = read(fd3, buffer, BUF_SIZE);

	printf("Read data from fd3: %s\n", buffer);

	lseek(fd3, 0, SEEK_SET);

	if (bytes > 0) {
		sscanf(buffer, "%s %d\n", str, &write_count);
		memset(buffer, 0, BUF_SIZE);
		sprintf(buffer, "%s %d\n", str, ++write_count);
	} else {
		sprintf(buffer,"file-write-O_CREATE 0\n");
	}

	printf("new data: %s\n", buffer);

    write(fd3, buffer, strlen(buffer));

    close(fd3);
    //unlink("testfile");

    printf("--- open(CREAT)/read/write/close\n");

	fd3 = open("/mlfs/testfile", O_RDWR, 0600);  
	lseek(fd3, 0, SEEK_SET);  

	bytes = read(fd3, buffer, BUF_SIZE); 
	printf("Read data (in-memory) from fd3: %s\n", buffer);  
	close(fd3);   

	printf("--- open(CREAT)/read again/close\n"); 

	pause();
    return 0;
}
