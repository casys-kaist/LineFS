#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>

// forward declaration: user-defined timer function
static void timer_func();

// ----------------------------------------------------------------------------------------
// begin provided code part
// variables needed for timer
static sem_t timer_sem;		// semaphore that's signaled if timer signal arrived
static bool timer_stopped;	// non-zero if the timer is to be timer_stopped
static pthread_t timer_thread;	// thread in which user timer functions execute

/* Timer signal handler.
 * On each timer signal, the signal handler will signal a semaphore.
 */
static void timersignalhandler(int sig) 
{
	/* called in signal handler context, we can only call 
	 * async-signal-safe functions now!
	 */
	sem_post(&timer_sem);	// the only async-signal-safe function pthreads defines
}

static void timer_func(void)
{
	return;
}

/* Timer thread.
 * This dedicated thread waits for posts on the timer semaphore.
 * For each post, timer_func() is called once.
 * 
 * This ensures that the timer_func() is not called in a signal context.
 */
static void * timerthread(void *_) 
{
	while (!timer_stopped) {
		int rc = sem_wait(&timer_sem);		// retry on EINTR
		if (rc == -1 && errno == EINTR)
			continue;
		if (rc == -1) {
			perror("sem_wait");
			exit(-1);
		}

		timer_func();	// user-specific timerfunc, can do anything it wants
	}
	return 0;
}

/* Initialize timer */
void init_timer(void) 
{
	/* One time set up */
	sem_init(&timer_sem, /*not shared*/ 0, /*initial value*/0);
	pthread_create(&timer_thread, (pthread_attr_t*)0, timerthread, (void*)0);
	signal(SIGALRM, timersignalhandler);
}

/* Shut timer down */
void shutdown_timer() 
{
	timer_stopped = true;
	sem_post(&timer_sem);
	pthread_join(timer_thread, 0);
}

/* Set a periodic timer.  You may need to modify this function. */
void set_periodic_timer(long delay) 
{
	struct itimerval tval = { 
		/* subsequent firings */ .it_interval = { .tv_sec = 0, .tv_usec = delay }, 
		/* first firing */       .it_value = { .tv_sec = 0, .tv_usec = delay }};

	setitimer(ITIMER_REAL, &tval, (struct itimerval*)0);
}
