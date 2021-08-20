#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "mlfs/mlfs_interface.h"

#define TESTDIR "/mlfs/test_dir"
#define N_FILES 1000

int main(int argc, char ** argv)
{
    struct stat buf;
    int i, ret = 0;
	
	init_fs();

	/*
    if ((ret = lstat(TESTDIR "/files/files1", &buf)) == 0) {
		printf("%s: inode number %lu", TESTDIR "files/file1", buf.st_ino);
	}
	*/

	/*
    if ((ret = rmdir(TESTDIR)) < 0 && errno != ENOENT) {
        perror("rmdir");
        exit(1);
    }
	*/

    if ((ret = mkdir(TESTDIR, 0700)) < 0) {
        perror("mkdir");
        exit(1);
    }

    if ((ret = creat(TESTDIR "/file", 0600)) < 0) {
        perror("open");
        exit(1);
    }

    if ((ret = unlink(TESTDIR "/file")) < 0) {
        perror("unlink");
        exit(1);
    }

    if ((ret = mkdir(TESTDIR "/files", 0600)) < 0) {
        perror("open");
        exit(1);
    }

	for (i = 0; i < N_FILES; i++) {
		char file_path[4096];
		memset(file_path, 0, 4096);

		sprintf(file_path, "%s%d", TESTDIR "/files/file", i);
		if ((ret = creat(file_path, 0600)) < 0) {
			perror("open");
			exit(1);
		}
	}

    if ((ret = unlink(TESTDIR "/files/file2")) < 0) {
        perror("unlink");
        exit(1);
    }

	if ((ret = creat(TESTDIR "/files/file2", 0600)) < 0) {
		perror("open");
		exit(1);
	}

	shutdown_fs();

    return 0;
}
