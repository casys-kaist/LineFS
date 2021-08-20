#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef struct
{
  bool done;
  pthread_mutex_t mutex;
} shared_data;

static shared_data* data = NULL;

void initialise_shared()
{
    // place our shared data in shared memory
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_ANONYMOUS;
    data = mmap(NULL, sizeof(shared_data), prot, flags, -1, 0);
    assert(data);

    data->done = false;

    // initialise mutex so it works properly in shared memory
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&data->mutex, &attr);
}

void run_child()
{
  while (true)
  {
      puts("child waiting. .. ");
      usleep(500000);

      pthread_mutex_lock(&data->mutex);
      if (data->done) {
          pthread_mutex_unlock(&data->mutex);
          puts("got done!");
          break;
      }
      pthread_mutex_unlock(&data->mutex);
  }

  puts("child exiting ..");
}

void run_parent(pid_t pid)
{
    puts("parent sleeping ..");
    sleep(2);

    puts("setting done ..");
    pthread_mutex_lock(&data->mutex);
    data->done = true;
    pthread_mutex_unlock(&data->mutex);

    waitpid(pid, NULL, NULL);

    puts("parent exiting ..");
}

int main(int argc, char** argv)
{
    initialise_shared();

    pid_t pid = fork();
    if (!pid) {
        run_child();
    }
    else {
        run_parent(pid);
    }

    munmap(data, sizeof(data));
    return 0;
}
