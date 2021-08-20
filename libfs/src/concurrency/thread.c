#include "global/mem.h"
#include "mlfs/mlfs_user.h"
#include "concurrency/thread.h"
#include "filesystem/shared.h"

#include <pthread.h>

__thread struct logheader_meta tls_loghdr_meta;

struct logheader_meta *get_loghdr_meta(void)
{
	return &tls_loghdr_meta;
}

unsigned long mlfs_create_thread(void *(*entry_point)(void *), volatile int* done)
{
	pthread_t thread_id;

	if (pthread_create(&thread_id, NULL, entry_point, (void*)done)) {
		perror("Thread create ");
		exit(-1);
	}

	return thread_id;
}
