#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>

#include <mlfs/mlfs_interface.h>	


int main(int argc, char ** argv)
{
	struct stat st = {0};

	init_fs();

    char const* const fileName = "userlist.dat"; /* should check that argc > 1 */
    char const* mlfs_dir = "/mlfs/";
    FILE* file = fopen(fileName, "r"); /* should check the result */
    char line[10];
    char user_dir_old[25];
    char user_dir_new[25];

    //mkdir(mlfs_dir, 0700);

    while (fgets(line, sizeof(line), file)) {
        /* note that fgets don't strip the terminating \n, checking its
           presence would allow to handle lines longer that sizeof(line) */
	    strtok(line, "\n");
            strcpy(user_dir_old, mlfs_dir);
	    strcpy(user_dir_new, mlfs_dir);
	    strcat(user_dir_old, line);
	    strcat(user_dir_new, line);
	    strcat(user_dir_old, "_old");
	    strcat(user_dir_new, "_new");
	    printf("creating dir: %s\n", user_dir_old);
	    mkdir(user_dir_old, 0700);
	    printf("creating dir: %s\n", user_dir_new);
	    mkdir(user_dir_new, 0700);
    }
    /* may check feof here to make a difference between eof and io failure -- network
       timeout for instance */

    fclose(file);

	shutdown_fs();
	return 0;
}
