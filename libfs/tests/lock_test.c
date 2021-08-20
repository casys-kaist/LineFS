#define _GNU_SOURCE

#include <time.h>
#include <assert.h>
#include <errno.h>

#include <concurrency/synchronization.h>

static void test_static_mutex(void)
{
	static mlfs_mutex_t static_mutex = {0};

	assert(!mlfs_mutex_lock(&static_mutex));
	assert(!mlfs_mutex_unlock(&static_mutex));
	assert(!mlfs_mutex_destroy(&static_mutex));
}

static void test_lock_unlock(mlfs_mutex_t *mutex)
{
	assert(!mlfs_mutex_lock(mutex));
	assert(!mlfs_mutex_unlock(mutex));
}

/* Wait a millisecond */
static void delay(void)
{
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;
	assert(!nanosleep(&ts, NULL));
}

struct test_contention {
	mlfs_mutex_t *mutex;
	int held;
	int count;
};

static void *bump(void *v_tc)
{
	struct test_contention *tc = v_tc;

	assert(!mlfs_mutex_lock(tc->mutex));
	assert(!tc->held);
	tc->held = 1;
	delay();
	tc->held = 0;
	tc->count++;
	assert(!mlfs_mutex_unlock(tc->mutex));

	return NULL;
}

static void test_contention(mlfs_mutex_t *mutex)
{
	struct test_contention tc;
	pthread_t threads[10];
	int i;

	tc.mutex = mutex;
	tc.held = 0;
	tc.count = 0;

	assert(!mlfs_mutex_lock(tc.mutex));

	for (i = 0; i < 10; i++)
		assert(!pthread_create(&threads[i], NULL, bump, &tc));

	assert(!mlfs_mutex_unlock(tc.mutex));

	for (i = 0; i < 10; i++)
		assert(!pthread_join(threads[i], NULL));

	assert(!mlfs_mutex_lock(tc.mutex));
	assert(!tc.held);
	assert(tc.count == 10);
	assert(!mlfs_mutex_unlock(tc.mutex));
}

static void *lock_cancellation_thread(void *v_mutex)
{
	mlfs_mutex_t *mutex = v_mutex;
	assert(!mlfs_mutex_lock(mutex));
	assert(!mlfs_mutex_unlock(mutex));
	return NULL;
}

/* mlfs_mutex_lock is *not* a cancellation point. */
static void test_lock_cancellation(mlfs_mutex_t *mutex)
{
	pthread_t thread;
	void *retval;

	assert(!mlfs_mutex_lock(mutex));
	assert(!pthread_create(&thread, NULL, lock_cancellation_thread,
				mutex));
	delay();
	assert(!pthread_cancel(thread));
	assert(!mlfs_mutex_unlock(mutex));
	assert(!pthread_join(thread, &retval));
	assert(!retval);
}

static void *trylock_thread(void *v_mutex)
{
	mlfs_mutex_t *mutex = v_mutex;
	assert(mlfs_mutex_trylock(mutex) == EBUSY);
	return NULL;
}

static void *trylock_contender_thread(void *v_mutex)
{
	mlfs_mutex_t *mutex = v_mutex;
	assert(!mlfs_mutex_lock(mutex));
	delay();
	delay();
	assert(!mlfs_mutex_unlock(mutex));
	return NULL;
}

static void test_trylock(mlfs_mutex_t *mutex)
{
	pthread_t thread1, thread2;

	assert(!mlfs_mutex_trylock(mutex));

	assert(!pthread_create(&thread1, NULL, trylock_thread, mutex));
	assert(!pthread_join(thread1, NULL));

	assert(!pthread_create(&thread1, NULL, trylock_contender_thread,
				mutex));
	delay();
	assert(!pthread_create(&thread2, NULL, trylock_thread, mutex));
	assert(!pthread_join(thread2, NULL));
	assert(!mlfs_mutex_unlock(mutex));
	assert(!pthread_join(thread1, NULL));
}

struct test_cond_wait {
	mlfs_mutex_t *mutex;
	mlfs_condvar_t cond;
	int flag;
};

static void *test_cond_wait_thread(void *v_tcw)
{
	struct test_cond_wait *tcw = v_tcw;

	assert(!mlfs_mutex_lock(tcw->mutex));
	while (!tcw->flag)
		assert(!mlfs_condvar_wait(&tcw->cond, tcw->mutex));
	assert(!mlfs_mutex_unlock(tcw->mutex));

	return NULL;
}

static void test_cond_wait(mlfs_mutex_t *mutex)
{
	struct test_cond_wait tcw;
	pthread_t thread;

	tcw.mutex = mutex;
	assert(!mlfs_condvar_init(&tcw.cond));
	tcw.flag = 0;

	assert(!pthread_create(&thread, NULL, test_cond_wait_thread, &tcw));

	delay();
	assert(!mlfs_mutex_lock(mutex));
	tcw.flag = 1;
	assert(!mlfs_condvar_signal(&tcw.cond));
	assert(!mlfs_mutex_unlock(mutex));

	assert(!pthread_join(thread, NULL));

	assert(!mlfs_condvar_destroy(&tcw.cond));
}

static void test_cond_timedwait(mlfs_mutex_t *mutex)
{
	pthread_cond_t cond;
	struct timespec t;

	assert(!pthread_cond_init(&cond, NULL));

	assert(!clock_gettime(CLOCK_REALTIME, &t));

	t.tv_nsec += 1000000;
	if (t.tv_nsec > 1000000000) {
		t.tv_nsec -= 1000000000;
		t.tv_sec++;
	}

	assert(!mlfs_mutex_lock(mutex));
	//assert(mlfs_condvar_timedwait(&cond, mutex, &t) == ETIMEDOUT);
	assert(!mlfs_mutex_unlock(mutex));

	assert(!pthread_cond_destroy(&cond));
}

static void test_cond_wait_cancellation(mlfs_mutex_t *mutex)
{
	struct test_cond_wait tcw;
	pthread_t thread;
	void *retval;

	tcw.mutex = mutex;
	assert(!mlfs_condvar_init(&tcw.cond));
	tcw.flag = 0;

	assert(!pthread_create(&thread, NULL, test_cond_wait_thread, &tcw));

	delay();
	assert(!pthread_cancel(thread));
	assert(!pthread_join(thread, &retval));
	assert(retval == PTHREAD_CANCELED);

	assert(!mlfs_condvar_destroy(&tcw.cond));
}

static void test_unlock_not_held(mlfs_mutex_t *mutex)
{
	assert(mlfs_mutex_unlock(mutex) == EPERM);
}

struct do_test {
	mlfs_mutex_t mutex;
	mlfs_condvar_t cond;
	int phase;
};

static void *do_test_cond_thread(void *v_dt)
{
	struct do_test *dt = v_dt;

	assert(!mlfs_mutex_lock(&dt->mutex));
	dt->phase = 1;
	assert(!mlfs_condvar_signal(&dt->cond));

	do {
		assert(!mlfs_condvar_wait(&dt->cond, &dt->mutex));
	} while (dt->phase != 2);

	assert(!mlfs_mutex_unlock(&dt->mutex));

	return NULL;
}

static void do_test(void (*f)(mlfs_mutex_t *m))
{
	struct do_test dt;
	pthread_t thread;

	/* First do the test with a fresh mutex. */
	assert(!mlfs_mutex_init(&dt.mutex));
	f(&dt.mutex);
	assert(!mlfs_mutex_destroy(&dt.mutex));

	/* Do the test with a thread waiting on a cond var associated
	   with the mutex.  This ensures that the skinny mutex has a
	   fat mutex during the text. */
	assert(!mlfs_mutex_init(&dt.mutex));
	assert(!mlfs_condvar_init(&dt.cond));

	dt.phase = 0;
	assert(!pthread_create(&thread, NULL, do_test_cond_thread, &dt));

	assert(!mlfs_mutex_lock(&dt.mutex));

	while (dt.phase != 1) {
		assert(!mlfs_condvar_wait(&dt.cond, &dt.mutex));
	};

	assert(!mlfs_mutex_unlock(&dt.mutex));

	f(&dt.mutex);

	assert(!mlfs_mutex_lock(&dt.mutex));
	dt.phase = 2;

	assert(!mlfs_condvar_signal(&dt.cond));
	assert(!mlfs_mutex_unlock(&dt.mutex));

	assert(!pthread_join(thread, NULL));
	assert(!mlfs_mutex_destroy(&dt.mutex));
	assert(!mlfs_condvar_destroy(&dt.cond));
}

int main(void)
{
	test_static_mutex();

	printf("lock and unlock test...");
	do_test(test_lock_unlock);
	printf("done\n");

	printf("contention test...");
	do_test(test_contention);
	printf("done\n");

	printf("lock cancellation test...");
	do_test(test_lock_cancellation);
	printf("done\n");

	printf("trylock test...");
	do_test(test_trylock);
	printf("done\n");

	printf("condvar wait test...");
	do_test(test_cond_wait);
	printf("done\n");

	//do_test(test_cond_timedwait);

#if 0
	// thread cancellation while waiting condvar.
	// This case does not support yet.
	printf("condvar wait cancellation test...");
	do_test(test_cond_wait_cancellation);
	printf("done\n");
#endif

	printf("unlock_not_held test...");
	do_test(test_unlock_not_held);
	printf("done\n");

	return 0;
}
