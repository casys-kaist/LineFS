#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <mlfs/mlfs_interface.h>
#include <unistd.h>
#include <signal.h>

volatile sig_atomic_t stop;

void inthand(int signum)
{
	stop = 1;
}

int main(int argc, char ** argv)
{
    struct dirent * dirent;

    DIR * dir;
    
    init_fs();

    signal(SIGINT, inthand);

    while (!stop)
    {
	    sleep(1);
    }

    shutdown_fs();

    return 0;
}
