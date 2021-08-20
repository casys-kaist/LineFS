#include "concurrency/synchronization.h"

#include <errno.h>

#define LOCK_BUSY 1

void mlfs_spinlock_init(struct mlfs_spinlock *lk, char *name)
{
	lk->lock = 0;
	strncpy(lk->name, name, LOCK_NAME_SIZE);
}

void mlfs_spin_lock(struct mlfs_spinlock *lk)
{
	while (1)
	{
		if (!xchg_32(&lk->lock, LOCK_BUSY)) 
			return;

		while (lk->lock) 
			cpu_relax();
	}
}

void mlfs_spin_unlock(struct mlfs_spinlock *lk)
{
	lk->lock = 0;
}

int mlfs_mutex_init(mlfs_mutex_t *mu)
{
	mu->u = 0;
	return 0;
}

int mlfs_mutex_destroy(mlfs_mutex_t *mu)
{
	// no-op
	return 0;
}

/* cmpxchg (*p, cmp, chg) pseudo code.
 *   old_val = *p
 *   if *p != cmp
 *    return old_val
 *   
 *   *p = chg
 *   return old_val
 */

/* state of lock variable
 * 0 : unlocked
 * 1 : locked - lock/unlock can be done by spinning.
 * 2 : locked and contented - lock can be into sleep.
 */
#define SPIN_TRY_COUNT 100
#if 0
int mlfs_mutex_lock(struct mlfs_mutex *mu)
{
	int i, c;

	/* Spin and try to take lock */
	for (i = 0; i < SPIN_TRY_COUNT; i++) {
		c = cmpxchg(mu, 0, 1);

		/* can hold lock immediately */
		if (!c) 
			return 0;

		cpu_relax();
	}

	/* The lock is now contended */
	if (c == 1) 
		c = xchg_32(mu, 2);

	/* contented state: Wait in the kernel */
	while (c) {
		/* It uses the FUTEX_PRIVATE version of the wait and wake operations. 
		 * This currently undocumented flag converts the operations 
		 * to be process-local. Normal futexes need to obtain mmap_sem 
		 * inside the kernel to compare addresses between processes. 
		 * If a futex is process-local, then this semaphore isn't required 
		 * and a simple comparison of virtual addresses is all that's needed. 
		 * Thus, using the private flag speeds things up somewhat 
		 * by reducing in-kernel contention. */
		sys_futex(mu, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
		c = xchg_32(mu, 2);
	}

	return 0;
}

int mlfs_mutex_unlock(mlfs_mutex_t *mu)
{
	int i;

	/* contented state: setup for unlock */
	if (*mu == 2) 
		// does not need atomic operation.
		*mu = 0;
	/* uncontented state: unlock and return */
	else if (xchg_32(mu, 0) == 1) 
		return 0;

	/* contented state: spin and check someone takes the lock */
	for (i = 0; i < (SPIN_TRY_COUNT << 1); i++) {
		if (*mu) {
			/* Need to set to state 2 because there may be waiters */
			if (cmpxchg(mu, 1, 2)) 
				return 0;
		}
		cpu_relax();
	}

	/* We need to wake someone up */
	sys_futex(mu, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);

	return 0;
}
#endif
int mlfs_mutex_lock(union mlfs_mutex *m)
{
	int i;

	/* Try to grab lock */
	for (i = 0; i < 100; i++) {
		if (!xchg_8(&m->b.locked, 1)) 
			return 0;

		cpu_relax();
	}

	/* Have to sleep */
	while (xchg_32(&m->u, 257) & 1) {
		sys_futex(m, FUTEX_WAIT_PRIVATE, 257, NULL, NULL, 0);
	}

	return 0;
}

int mlfs_mutex_unlock(mlfs_mutex_t *m)
{
	int i;

	if (m->u == 0)
		return EPERM;

	/* Locked and not contended */
	if ((m->u == 1) && (cmpxchg(&m->u, 1, 0) == 1)) 
		return 0;

	/* Unlock */
	m->b.locked = 0;

	m_barrier();

	/* Spin and hope someone takes the lock */
	for (i = 0; i < 200; i++)
	{
		if (m->b.locked) 
			return 0;

		cpu_relax();
	}

	/* We need to wake someone up */
	m->b.contended = 0;

	sys_futex(m, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);

	return 0;
}

int mlfs_mutex_trylock(mlfs_mutex_t *m)
{
	unsigned c = xchg_8(&m->b.locked, 1);
	if (!c) 
		return 0;
	return EBUSY;
}
