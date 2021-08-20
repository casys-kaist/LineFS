#ifndef _SYNC_UP_H_
#define _SYNC_UP_H_

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define HOLDUP_FD 333
#define DONE_FD 444
#define READY_FD 555
#define LEASE_SEM_NAME "/lease"

#ifdef MLFS
#define lock_filename "/mlfs/lock"
#define write_filename "/mlfs/write"
#else
#define lock_filename "./pmem/lock"
#define write_filename "./pmem/write"
#endif


static inline void wait_for_int(int pipefd)
{
   int readbuf;
   ssize_t sz;

   sz = read(pipefd, &readbuf, sizeof(readbuf));

   if (sz != sizeof(readbuf)) {
      fprintf(stderr, "read from fd: %d (%ld bytes)", pipefd, sz);
      perror(" ");
      exit(1);
   }
}

static inline void send_int(int fd)
{
   int i = 0;

   if (write(fd, &i, sizeof(i)) != sizeof(i)) {
      fprintf(stderr, "wrote to fd: %d", fd);
      perror(" ");
      exit(1);
   }
}

static inline void ready(void)
{
   send_int(READY_FD);
   wait_for_int(HOLDUP_FD);
   close(HOLDUP_FD);
}


static inline void done(void)
{
   send_int(DONE_FD);
   close(DONE_FD);
}

#endif
