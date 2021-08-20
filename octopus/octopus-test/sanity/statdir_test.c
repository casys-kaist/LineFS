#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#define MAX_PATH 256

static inline void indent(int n)
{
	//for (int i = 0; i < n; i++)
	printf("%*s", n*3, "");
		//putchar('   ');
}

int is_directory(const char *path) {
	   struct stat statbuf;
	      if (stat(path, &statbuf) != 0)
		             return 0;
	         return S_ISDIR(statbuf.st_mode);
}

int is_file(const char *path) {
	   struct stat statbuf;
	      if (stat(path, &statbuf) != 0)
		             return 0;
	         return S_ISREG(statbuf.st_mode);
}

static int print_dir_entries(char * _path, char * name, int level, int recur)
{
	char *path = _path;

	if(name) {
		char new[MAX_PATH];
		strcpy(new, _path);
		strcat(new, "/");
		strcat(new, name);
		path = &new[0];
	}


	if(is_directory(path)) {
		struct dirent * dirent;
		DIR * dir;

		//FIXME: remove
		dir = opendir(path);

		if(!dir)
			printf("[ERROR] Cannot open directory. Returned errorno is: %d\n", errno);
		

		indent(level);

		if(level == 0)
			printf("[%s]\n", path);
		else
			printf("[%s]\n", name);

		// check if we should stat recursively
		if(level > 0 && !recur)
			return 0;

		level++;

		while((dirent = readdir(dir))) {
			// ignore '.' and '.." directories
			if(strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
				continue;
			print_dir_entries(path, dirent->d_name, level, recur);
		}

		closedir(dir);
	}
	else if(is_file(path)) {
		struct stat st;
		stat(path, &st);
		off_t size = st.st_size;
		indent(level);
		printf("- %s (%lu bytes)\n", name, size);
	}
	else {
		printf("[ERROR] unhandled object type for path %s\n", path);
		exit(-1);
	}

		return 0;	
}

void print_usage()
{
	printf("Usage: statdir_test [OPTION] ... PATH...\n");
	printf(" -r \t enable recursion\n");
}

int main(int argc, char ** argv)
{
    extern int optind;
    char *dir_name;
    int recur = 0;
   
    if(getopt(argc, argv, "r") != -1)
	    recur = 1;

    if(optind < argc)
	    dir_name = argv[optind];
    else {
	    printf("[ERROR] Invalid commandline arguments\n");
	    print_usage();
	    exit(-1);
    }

    if(dir_name[0] != '/') {
	    printf("[Error] Relative directories are unsupported\n");
	    exit(-1);
    }

    printf("Outputting directory structure for:\n");
    print_dir_entries(dir_name, 0, 0, recur);

    return 0;
}
