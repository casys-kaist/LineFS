#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

/* Shm util.
 * read shm, write shm.
 *
 * Usage: ./use_shm <-r|-w value>
 *
 */

#define TEST_DIR_PATH "fileset/"
/* #define SHM_PATH "/shm_lease_test" */
#define SHM_F_SIZE 64

enum op {
    Invalid = 0,
    Read    = 1,
    Write   = 2,
    Destroy   = 3
};

static int op = 0;
static int val = 0;

void*
create_shm(char *path)
{
    int res, fd;
    void *addr;
    fd = shm_open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        printf ("(%s) shm_open failed.\n", __func__);
        exit(-1);
    }

    res = ftruncate(fd, SHM_F_SIZE);
    if (res < 0)
    {
        printf ("(%s) ftruncate error.\n", __func__);
        exit(-1);
    }

    addr = mmap(NULL, SHM_F_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED){
        printf ("(%s) mmap failed.\n", __func__);
        exit(-1);
    }

    return addr;
}

void
destroy_shm(char *path, void *addr)
{
    int ret, fd;
    ret = munmap(addr, SHM_F_SIZE);
    if (ret < 0)
    {
        printf ("(%s) munmap error.\n", __func__);
        exit(-1);
    }

    fd = shm_unlink(path);
    if (fd < 0) {
        printf ("(%s) shm_unlink failed.\n", __func__);
        exit(-1);
    }
}

/**
 * Validate command arguments.
 * Return: 1 if options are valid. Otherwise, 0.
 */
static int
parse_args(int argc, char *argv[], char *path)
{
    int opt;
    while((opt = getopt(argc, argv, "rw:p:d")) != -1) {
        switch(opt) {
            case 'p':
                strcpy(path, optarg);
                break;

            case 'r':
                op = Read;
                break;

            case 'w':
                op = Write;
                val = atoi(optarg);
                break;

            case 'd':
                op = Destroy;
                break;

            case 'h':   // print help.
                return 0;

            default:
                return 0;
        }
    }
    return 1;
}

void
print_usage(char *argv[])
{
    printf("Usage: %s <-p path> <-r|-w value>\n", argv[0]);
}

int
main (int argc, char *argv[])
{
    char path[4096];
    int *shm_i;

    if(!parse_args(argc, argv, path) ||
            (strcmp(path,"") == 0) ||
            (op == Invalid)) {
        print_usage(argv);
        exit(1);
    }
    shm_i = (int*)create_shm(path);

    switch(op) {
        case Read:
            printf("%d\n", *shm_i);
            break;

        case Write:
            printf("Write %d to %s\n", val, path);
            *shm_i = val;
            break;

        case Destroy:
            printf("Destroy shm: %s\n", path);
            destroy_shm(path, (void *)shm_i);
            break;

        default:
            printf("Unknown op\n");
            exit(1);
    }

    return 0;
}
