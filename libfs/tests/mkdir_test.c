#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mlfs/mlfs_interface.h>	


int main(int argc, char ** argv)
{
	struct stat st = {0};

	init_fs();

	//if (stat("/mlfs/zvon/Maildir/tmp/", &st) == -1) {
	  //  mkdir("/mlfs/zvon/Maildir/tmp/", 0700);
	//}

	//if (stat("/mlfs/zvon/Maildir/", &st) == -1) {
	    mkdir("/mlfs/", 0700);
	    mkdir("/mlfs/0/", 0700);
	    mkdir("/mlfs/1/", 0700);
	    mkdir("/mlfs/2/", 0700);
	    mkdir("/mlfs/3/", 0700);
	    mkdir("/mlfs/4/", 0700);
	    mkdir("/mlfs/5/", 0700);
	    mkdir("/mlfs/6/", 0700);
	    mkdir("/mlfs/7/", 0700);
	    mkdir("/mlfs/8/", 0700);
	    mkdir("/mlfs/9/", 0700);
	    mkdir("/mlfs/10/", 0700);
	    mkdir("/mlfs/11/", 0700);
	    mkdir("/mlfs/12/", 0700);
	    mkdir("/mlfs/13/", 0700);
	    mkdir("/mlfs/14/", 0700);
	    mkdir("/mlfs/15/", 0700);
	    mkdir("/mlfs/16/", 0700);

	    // pause();
	    //mkdir("/mlfs/zvon/Maildir/cur/", 0700);
	    //mkdir("/mlfs/zvon/Maildir/tmp/", 0700);
	    //mkdir("/mlfs/zvon/Maildir/new/", 0700);
	//}

	shutdown_fs();
	return 0;
}
