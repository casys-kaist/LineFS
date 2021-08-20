// Mutual exclusion spin locks.

#include "mlfs/mlfs_user.h"

#include "global/defs.h"
#include "concurrency/synchronization.h"
#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <limits.h> //for INT_MAX

void mlfs_cond_wait(void *channel)
{
	struct cond_channel *ch;
	ch = (struct cond_channel *)channel;

	pthread_cond_wait(&ch->cond, &ch->mutex);
}

void mlfs_cond_signal(void *channel)
{
	struct cond_channel *ch;
	ch = (struct cond_channel *)channel;

	pthread_mutex_lock(&ch->mutex);
	pthread_cond_signal(&ch->cond);
	pthread_mutex_unlock(&ch->mutex);
}

int mlfs_condvar_init(struct mlfs_condvar *cv)
{
	cv->m = NULL;
	cv->seq = 0;

	return 0;
}

int mlfs_condvar_destroy(struct mlfs_condvar *cv)
{
	// no-op
	return 0;
}

/* when condvar_wait returns, it holds mutex (contented state). */
int mlfs_condvar_signal(struct mlfs_condvar *cv)
{
	atomic_add(&cv->seq, 1);

	sys_futex(&cv->seq, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);

	return 0;
}

int mlfs_condvar_broadcast(struct mlfs_condvar *cv)
{
	mlfs_mutex_t *m = cv->m;

	/* No mutex means that there are no waiters */
	if (!m)
		return 0;

	/* waking up everyone */
	atomic_add(&cv->seq, 1);

	/* Every time we do a wake operation, we increment the seq variable. 
	 * Thus the only thing we need to do to prevent missed-wakeups is 
	 * to check that this doesn't change whilst falling to sleep. 
	 * This isn't 100% fool-proof. If 2^32 wake operations can happen between 
	 * the read of the seq variable and the implementation of the FUTEX_WAIT system call, 
	 * then we will have a bug. However, this is extremely unlikely 
	 * due to amount of time it would take to generate that many calls.
	 */

	/* Wake one thread, and requeue the rest on the mutex */
	sys_futex(&cv->seq, FUTEX_REQUEUE_PRIVATE, 1, (void *) INT_MAX, m, 0);

	return 0;
}

#if 0
/* We need to make sure that after we wake up that we change the mutex 
 * into the contended state. Thus we cannot use the normal mutex lock 
 * function, and have to use an inline version with this constraint. 
 * The only other thing we need to do is save the value of the mutex 
 * pointer for later. This can be done in a lock-free manner with 
 * a compare-exchange instruction. */
int mlfs_condvar_wait(struct mlfs_condvar *cv, struct mlfs_mutex *mu)
{
	int seq = cv->seq;

	if (cv->m != &mu->lock) {
		mlfs_assert(cv->m == NULL);

		cmpxchg(cv->m, NULL, &mu->lock);

		mlfs_assert(cv->m == &mu->lock);
	}
	
	mlfs_mutex_unlock(mu);

	sys_futex(&cv->seq, FUTEX_WAIT_PRIVATE, seq, NULL, NULL, 0);

	while (xchg_32(&mu->lock, 2)) 
		sys_futex(&mu->lock, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);

	return 0;
}
#endif

int mlfs_condvar_wait(struct mlfs_condvar *c, mlfs_mutex_t *m)
{
	int seq = c->seq;

	if (c->m != m) {
		/* Atomically set mutex inside cv */
		cmpxchg(&c->m, NULL, m);
		if (c->m != m) 
			return -EINVAL;
	}

	mlfs_mutex_unlock(m);

	sys_futex(&c->seq, FUTEX_WAIT_PRIVATE, seq, NULL, NULL, 0);

	while (xchg_32(&m->b.locked, 257) & 1)
	{
		sys_futex(m, FUTEX_WAIT_PRIVATE, 257, NULL, NULL, 0);
	}

	return 0;
}
