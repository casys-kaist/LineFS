#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <mlfs/mlfs_interface.h>

int main(int argc, char ** argv)
{
    struct dirent * dirent;

    DIR * dir;
    
    init_fs();
    dir = opendir("/mlfs");

	//dir_test creates 1000 files in /mlfs/test_dir/files
    //dir = opendir("/mlfs/test_dir/files/");

    while ((dirent = readdir(dir)))
        printf("found %s\n", dirent->d_name);

    closedir(dir);

    return 0;
}
