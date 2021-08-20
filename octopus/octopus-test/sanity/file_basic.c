#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUF_SIZE 4096

int main(int argc, char ** argv)
{
    int fd1, fd2, fd3;
    int bytes, ret;
    char buffer[BUF_SIZE], str[BUF_SIZE];
	int write_count;

    fd3 = open("/mnt/dmfs/testfile", O_RDWR|O_CREAT, 0600);

    if (fd3 < 0) {
        perror("open with O_CREAT");
        return -1;
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

    if(write(fd3, buffer, strlen(buffer)) != strlen(buffer)) {
	    perror("write failed");
	    return -1;
    }

    close(fd3);
    //unlink("testfile");

    printf("--- open(CREAT)/read/write/close\n");

	fd3 = open("/mnt/dmfs/testfile", O_RDWR, 0600);  
	lseek(fd3, 0, SEEK_SET);  

	bytes = read(fd3, buffer, BUF_SIZE); 
	printf("Read data (in-memory) from fd3: %s\n", buffer);  
	close(fd3);   

	printf("--- open(CREAT)/read again/close\n"); 

	//pause();
    return 0;
}
