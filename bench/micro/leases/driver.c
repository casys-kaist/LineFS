#define _GNU_SOURCE
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/time.h>
#include <semaphore.h>

#include "sync_up.h"

double get_wall_time(){
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        //  Handle error
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

//int cgroup_fds[N_CORES];
int pipefds[2];
int ready_pipe[2];
int done_pipe[2];

pid_t spawn_child(char **argv, cpu_set_t* cpuset, int devid)
{
   pid_t pid;
   char buf[128];

   snprintf(buf, 128, "%d", devid);
   pid = fork();
   if (!pid) {
      /* child */
      prctl(PR_SET_PDEATHSIG, SIGTERM);
      if (sched_setaffinity(0, sizeof(cpu_set_t), cpuset))
         perror("set affinity");

      if (dup2(pipefds[0], HOLDUP_FD) < 0)
         perror("dup HOLDUP");
      if (setenv("DEV_ID", buf, 1))
         perror("setenv");

      close(pipefds[0]);
      close(pipefds[1]);

      if (dup2(done_pipe[1], DONE_FD) < 0)
         perror("dup DONE");
      if (dup2(ready_pipe[1], READY_FD) < 0)
         perror("dup READY");

      close(done_pipe[0]);
      close(done_pipe[1]);
      close(ready_pipe[0]);
      close(ready_pipe[1]);

      execv(argv[0], argv);
      perror("Returned from execv");
      exit(1);
   }

   return pid;
}

#if 0
int write_int_to_fd(int fd, int i)
{
   int ret, len;
   char buf[128];

   snprintf(buf, 128, "%d", i);
   len = strlen(buf);
   ret = write(fd, buf, len);
   if (ret != len) {
      fprintf(stderr, "write_int_to_fd fd: %d, int: %s\n", fd, buf);
      perror("SYS error");
      return ret;
   }
   return ret;
}

void ensure_cgroups_ready(void)
{
   int ret;
   int i;
   char filename[256];

   for (i = 0; i < N_CORES; i++) {
      snprintf(filename, 256, "/sys/fs/cgroup/cpuset/core%d", i);
      if (mkdir(filename, 0555) && errno != EEXIST) {
         printf("Failed to create cpuset \"%s\"\n", filename);
         continue;
      }

      snprintf(filename, 256, "/sys/fs/cgroup/cpuset/core%d/cpuset.cpus", i);
      ret = open(filename, O_RDWR);
      if (ret < 0) {
         printf("Failed to open \"%s\"\n", filename);
         continue;
      }

      write_int_to_fd(ret, i);
      close(ret);

      snprintf(filename, 256, "/sys/fs/cgroup/cpuset/core%d/cpuset.mems", i);
      ret = open(filename, O_RDWR);
      if (ret < 0) {
         printf("Failed to open \"%s\"\n", filename);
         continue;
      }
      write_int_to_fd(ret, 0);
      close(ret);

      snprintf(filename, 256, "/sys/fs/cgroup/cpuset/core%d/cgroup.procs", i);
      ret = open(filename, O_RDWR);
      if (ret < 0) {
         printf("Failed to open \"%s\"\n", filename);
         continue;
      }
      cgroup_fds[i] = ret;
   }
}


void assign_to_cgroup(pid_t pid, int index)
{
   write_int_to_fd(cgroup_fds[index % N_CORES], pid);
}
#endif

void cleanup(void)
{
   sem_unlink(LEASE_SEM_NAME);
   unlink(lock_filename);
}

int main(int argc, char **argv)
{
   int nthreads;
   char **child_argv;
   int i;
   struct stat stat_buf;
   pid_t children[256];
   cpu_set_t cpuset;
   double begin, end;

   atexit(cleanup);

   if (argc < 3) {
      printf("Usage: %s <nthreads> <binary> [binary_args]\n", argv[0]);
      return 1;
   }

   nthreads = atoi(argv[1]);
   child_argv = argv + 2;


   if (!nthreads || nthreads < 1 || nthreads > 256) {
      printf("nthreads (arv[1]) must be positive integer <= 256\n");
      return 1;
   }

   if (stat(child_argv[0], &stat_buf) || (!S_ISREG(stat_buf.st_mode))){
      perror(child_argv[0]);
      return 1;
   }

   //ensure_cgroups_ready();

   if (pipe(pipefds)) {
      perror("pipe");
      return 1;
   }
   if (pipe(done_pipe)) {
      perror("Done pipe");
      return 1;
   }
   if (pipe(ready_pipe)) {
      perror("Ready pipe");
      return 1;
   }

   for (i = 0; i < nthreads; i++) {
      CPU_ZERO(&cpuset);
      CPU_SET(i % N_CORES, &cpuset);
      children[i] = spawn_child(child_argv, &cpuset, (i % 2) + 4);
      if (children[i] < -1)
         return 1;
      //assign_to_cgroup(children[i], i);
   }
   close(pipefds[0]);
   close(done_pipe[1]);
   close(ready_pipe[1]);

   printf("%d threads running: ", nthreads);
   while (*child_argv)
      printf("%s ", *(child_argv++));
   printf("\n");

   for (i = 0; i < nthreads; i++) {
      wait_for_int(ready_pipe[0]);
   }
   close(ready_pipe[0]);

   /* start children */
   begin = get_wall_time();
   for (i = 0; i < nthreads; i++)
      send_int(pipefds[1]);
   close(pipefds[1]);

   for (i = 0; i < nthreads; i++) {
      wait_for_int(done_pipe[0]);
   }
   close(done_pipe[0]);
   end = get_wall_time();

   while (nthreads) {
      int status;
      pid_t c = wait(&status);
      if (c > 0 && (WIFEXITED(status) || WIFSIGNALED(status)))
         nthreads--;
      else if (errno == ECHILD) {
         nthreads = 0;
         break;
      } else
         perror("unexpected return from wait");
   }

   printf("Total time: %f secs\n", end - begin);

   return 0;
}
