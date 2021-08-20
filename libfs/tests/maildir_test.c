#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#ifdef MLFS
#include <mlfs/mlfs_interface.h>
#endif


int main(int argc, char ** argv)
{

    if(argc < 2) {
	    printf("Usage: ./maildir_test list_of_users.dat\n");
	    exit(-1);
    }

    struct stat st = {0};
    char const* const fileName = argv[1]; /* should check that argc > 1 */
    char const* mlfs_dir = "/mlfs/";
    char const* mail_dir = "/Maildir";
    char const* new_dir = "/new";
    char const* tmp_dir = "/tmp";
    char const* cur_dir = "/cur";
    FILE* file = fopen(fileName, "r"); /* should check the result */
    char line[25];
    char user_dir_new[50];
    char user_dir_tmp[50];
    char user_dir_cur[50];

#ifdef MLFS
    init_fs();
#endif

    //mkdir(mlfs_dir, 0700);

    while (fgets(line, sizeof(line), file)) {
        /* note that fgets don't strip the terminating \n, checking its
           presence would allow to handle lines longer that sizeof(line) */

	    strtok(line, "\n");
            strcpy(user_dir_new, mlfs_dir);
	    strcpy(user_dir_tmp, mlfs_dir);
	    strcpy(user_dir_cur, mlfs_dir);

	    strcat(user_dir_new, line);
	    strcat(user_dir_tmp, line);
	    strcat(user_dir_cur, line);

   	    mkdir(user_dir_new, 0700);

	    strcat(user_dir_new, mail_dir);
	    strcat(user_dir_tmp, mail_dir);
	    strcat(user_dir_cur, mail_dir);

	    mkdir(user_dir_new, 0700);
	    //mkdir(user_dir_tmp, 0700);
	    //mkdir(user_dir_cur, 0700);

	    strcat(user_dir_new, "/new");
	    strcat(user_dir_tmp, "/tmp");
	    strcat(user_dir_cur, "/cur");

	    printf("creating dir: %s\n", user_dir_new);
	    mkdir(user_dir_new, 0700);
	    printf("creating dir: %s\n", user_dir_tmp);
	    mkdir(user_dir_tmp, 0700);
	    printf("creating dir: %s\n", user_dir_cur);
	    mkdir(user_dir_cur, 0700);
    }
    /* may check feof here to make a difference between eof and io failure -- network
       timeout for instance */

    fclose(file);

    	//pause();
#ifdef MLFS
	shutdown_fs();
#endif
	return 0;
}
