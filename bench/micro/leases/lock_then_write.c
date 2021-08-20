#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <semaphore.h>

#include "sync_up.h"

#ifdef MLFS
#include <mlfs/mlfs_interface.h>	
#endif

#define FILESIZE 4096LU
//#define FILESIZE (1UL << 20)
//#define FILESIZE (16UL << 20)
//#define FILESIZE (128UL << 20)

char data[FILESIZE];

#ifdef MLFS
sem_t *lease;
#define INIT_LEASE() do {\
   lease = sem_open(LEASE_SEM_NAME, O_CREAT, 0700, 1);\
   if (lease == SEM_FAILED) \
      perror("INIT_LEASE");\
} while (0)

#define GET_LEASE() do {\
   if (sem_wait(lease)) \
      perror("GET_LEASE");\
} while (0)

#define DROP_LEASE() do {\
	make_digest_request_async(100); \
	wait_on_digesting(); \
   if (sem_post(lease)) \
      perror("DROP_LEASE");\
} while (0)
#else
#define INIT_LEASE()
#define  GET_LEASE()
#define DROP_LEASE()
#endif

int aquire_lock(void)
{
   int lock_fd;
   while ((lock_fd = open(lock_filename, O_EXCL|O_CREAT, 0444)) < 0
         && errno == EEXIST)
      /* spin */;
   if (lock_fd < 0 && errno != EEXIST) {
      perror("open lock file");
   }
   close(lock_fd);
   return 1;
}

void write_file(void)
{
   int fd = open(write_filename, O_CREAT|O_RDWR, 0666);
   struct stat fd_stat;

   if (fd < 0) {
      perror("could not open write file!");
      exit(1);
   }

   if (FILESIZE != write(fd, data, FILESIZE)) {
      printf("Failed to write a block!\n");
      exit(1);
   }

   if (fstat(fd, &fd_stat)) {
      perror("fstat failed! wtf?");
      exit(1);
   } else if (fd_stat.st_size != FILESIZE) {
      printf("File has weird size: %lu (should be %lu)",
             fd_stat.st_size, FILESIZE);
      exit(1);
   }
   close(fd);
}

void release_lock(void)
{
   if (unlink(lock_filename)) {
      perror("unlink failed!");
      exit(1);
   }
}

int main(int argc, char **argv)
{
   int i, ntimes = 1;

   if (argc > 1)
      ntimes = atoi(argv[1]);


   INIT_LEASE();

#ifdef MLFS
	init_fs();
#endif

   ready();
   for (i = 0; i < ntimes; i++) {
      GET_LEASE();
      if (aquire_lock()) {
         write_file();
         release_lock();
      } else {
         DROP_LEASE();
         return 1;
      }
      DROP_LEASE();
   }
   done();

#ifdef MLFS
	shutdown_fs();
#endif

   return 0;
}
