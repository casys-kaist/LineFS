/* Simple performance tests to compare mlfs mutex, pthreads
 * mutexes and pthrreads spinlocks.
 *
 * Compile with MLFS_MUTEX, PTHREADS or SPINLOCK defined for what you want
 * to measure.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef PERF_mlfs
#include <concurrency/synchronization.h>
#else
#include <pthread.h>
#endif

#if defined(PERF_pthreads)

typedef pthread_mutex_t mutex_t;

static int mutex_init(mutex_t *mutex)
{
	return pthread_mutex_init(mutex, NULL);
}

#define mutex_destroy pthread_mutex_destroy
#define mutex_lock pthread_mutex_lock
#define mutex_unlock pthread_mutex_unlock

#elif defined(PERF_mlfs)

typedef mlfs_mutex_t mutex_t;

#define mutex_init mlfs_mutex_init
#define mutex_destroy mlfs_mutex_destroy
#define mutex_lock mlfs_mutex_lock
#define mutex_unlock mlfs_mutex_unlock

#elif defined(PERF_spinlock)

typedef pthread_spinlock_t mutex_t;

static int mutex_init(mutex_t *mutex)
{
	return pthread_spin_init(mutex, PTHREAD_PROCESS_PRIVATE);
}

#define mutex_destroy pthread_spin_destroy
#define mutex_lock pthread_spin_lock
#define mutex_unlock pthread_spin_unlock

#endif

struct test_results {
	int reps;
	long long start;
	long long stop;
};

static long long now_usecs(void)
{
	struct timeval tv;
	assert(!gettimeofday(&tv, NULL));
	return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* Simply acquiring and releasing a lock, without any contention. */
static void lock_unlock(struct test_results *res)
{
	mutex_t mutex;
	int i;

	assert(!mutex_init(&mutex));

	res->start = now_usecs();

	for (i = res->reps; i--;) {
		assert(!mutex_lock(&mutex));
		assert(!mutex_unlock(&mutex));
	}

	res->stop = now_usecs();

	assert(!mutex_destroy(&mutex));
}

/* Robustly measuring the performance of contended locks is not as
 * easy as it sounds.  We can't simply have a few locks, and throw a
 * larger number of threads at them, acquiring and releasing
 * individual locks.  This is because the lock types we are measuring
 * do not guarantee fair behaviour.  So what you can easily get is one
 * thread that runs for a while, acquiring and releasing many times,
 * while other threads sit waiting on locks without managing to
 * acquire them.  (This kind of thing is not a problem in real
 * applications because they actually do useful work while holding
 * locks, rather than acquiring and immediately releasing them.)
 *
 * So we need to reliably induce the interesting contention case:
 * Every time a thread releases a lock, some other waiting thread
 * acquires it and gets to run.
 *
 * We do this by having a set of locks arranged in a ring, with one
 * more lock than there are threads involved.  Each thread holds a
 * lock, and also tries to acquire the next lock in the ring.  When it
 * acquires the next lock, it drops the previous lock.  Then it tries
 * to acquire the next next lock, and so on.  The effect is that at
 * every moment, only one thread is able to acquire two locks and so
 * make progress; in doing so, it releases a lock allowing another
 * thread to make progress and then promptly gets blocked.
 */

#define CONTENTION_THREAD_COUNT 4
#define CONTENTION_MUTEX_COUNT (CONTENTION_THREAD_COUNT + 1)

struct contention_info {
	mutex_t mutexes[CONTENTION_MUTEX_COUNT];

	pthread_mutex_t ready_mutex;
	pthread_cond_t ready_cond;
	int ready_count;

	int thread_reps;
};

struct contention_thread_info {
	struct contention_info *info;
	pthread_mutex_t start_mutex;
	int thread_index;
	long long start;
	long long stop;
};

static void *contention_thread(void *v_thread_info)
{
	struct contention_thread_info *thread_info = v_thread_info;
	struct contention_info *info = thread_info->info;
	int i = thread_info->thread_index;
	int reps = info->thread_reps;
	int j;

	/* Lock our first mutex */
	assert(!mutex_lock(&info->mutexes[i]));

	/* Indicate that we are ready for the test. */
	assert(!pthread_mutex_lock(&info->ready_mutex));
	if (++info->ready_count == CONTENTION_THREAD_COUNT)
		assert(!pthread_cond_signal(&info->ready_cond));
	assert(!pthread_mutex_unlock(&info->ready_mutex));

	/* Line up to start */
	assert(!pthread_mutex_lock(&thread_info->start_mutex));
	assert(!pthread_mutex_unlock(&thread_info->start_mutex));

	thread_info->start = now_usecs();

	for (j = 1; j < reps; j++) {
		int next = (i + 1) % CONTENTION_MUTEX_COUNT;
		assert(!mutex_lock(&info->mutexes[next]));
		assert(!mutex_unlock(&info->mutexes[i]));
		i = next;
	}

	thread_info->stop = now_usecs();

	assert(!mutex_unlock(&info->mutexes[i]));
	return NULL;
}

static void contention(struct test_results *res)
{
	struct contention_info info;
	struct contention_thread_info thread_infos[CONTENTION_THREAD_COUNT];
	pthread_t threads[CONTENTION_THREAD_COUNT];
	int i;

	for (i = 0; i < CONTENTION_MUTEX_COUNT; i++)
		assert(!mutex_init(&info.mutexes[i]));

	assert(!pthread_mutex_init(&info.ready_mutex, NULL));
	assert(!pthread_cond_init(&info.ready_cond, NULL));
	info.ready_count = 0;
	info.thread_reps = res->reps / CONTENTION_THREAD_COUNT;

	for (i = 0; i < CONTENTION_THREAD_COUNT; i++) {
		thread_infos[i].info = &info;
		thread_infos[i].thread_index = i;
		assert(!pthread_mutex_init(&thread_infos[i].start_mutex, NULL));
		assert(!pthread_mutex_lock(&thread_infos[i].start_mutex));
		assert(!pthread_create(&threads[i], NULL,
					contention_thread, &thread_infos[i]));
	}

	assert(!pthread_mutex_lock(&info.ready_mutex));
	while (info.ready_count < CONTENTION_THREAD_COUNT)
		assert(!pthread_cond_wait(&info.ready_cond,
					&info.ready_mutex));
	assert(!pthread_mutex_unlock(&info.ready_mutex));

	for (i = 0; i < CONTENTION_THREAD_COUNT; i++)
		assert(!pthread_mutex_unlock(&thread_infos[i].start_mutex));

	for (i = 0; i < CONTENTION_THREAD_COUNT; i++) {
		assert(!pthread_join(threads[i], NULL));
		assert(!pthread_mutex_destroy(&thread_infos[i].start_mutex));
	}

	for (i = 0; i < CONTENTION_MUTEX_COUNT; i++)
		assert(!mutex_destroy(&info.mutexes[i]));

	assert(!pthread_mutex_destroy(&info.ready_mutex));
	assert(!pthread_cond_destroy(&info.ready_cond));

	res->start = thread_infos[0].start;
	res->stop = thread_infos[0].stop;
	for (i = 1; i < CONTENTION_THREAD_COUNT; i++) {
		if (thread_infos[i].start < res->start)
			res->start = thread_infos[i].start;
		if (thread_infos[i].stop > res->stop)
			res->stop = thread_infos[i].stop;
	}
}

static int cmp_long_long(const void *ap, const void *bp)
{
	long long a = *(long long *)ap;
	long long b = *(long long *)bp;

	if (a < b)
		return -1;
	else if (a > b)
		return 1;
	else
		return 0;
}

#define SETS 10

static void measure(void (*test)(struct test_results *res),
		const char *name, int reps)
{
	struct test_results res;
	long long times[SETS];
	int i;

	printf("Measuring %s: ", name);
	fflush(stdout);

	res.reps = reps;

	for (i = 0; i < SETS; i++) {
		test(&res);
		times[i] = res.stop - res.start;
	}

	qsort(times, SETS, sizeof(long long), cmp_long_long);
	printf("best %dns, 50%%-tile %dns\n", (int)(times[0] * 1000 / reps),
			(int)(times[SETS / 2] * 1000 / reps));
}

int main(void)
{
	measure(lock_unlock, "Locking and unlocking without contention",
			10000000);
	measure(contention, "Locking and unlocking with contention",
			100000);
	return 0;
}

