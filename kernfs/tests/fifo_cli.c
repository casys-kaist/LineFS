#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define MAX_FIFO_BUF 4096
#define fifo_path "/tmp/digest_fifo"

int main()
{
	int fd;
	char buf[MAX_FIFO_BUF];

	memset(buf, 0, MAX_FIFO_BUF);

	sprintf(buf, "Message from client\n");
	/* open, read, and display the message from the FIFO */
	fd = open(fifo_path, O_WRONLY);
	write(fd, buf, MAX_FIFO_BUF);

	close(fd);

	return 0;
}
