/* ********************************
 * Author:       Johan Hanssen Seferidis
 * License:	     MIT
 * Description:  Library providing a threading pool where you can add
 *               work. For usage, check the thpool.h file or README.md
 *
 *//** @file thpool.h *//*
 *
 ********************************/
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include "thpool.h"
#include "global/defs.h"
#include "synchronization.h"
#include "global/util.h"
#include "global/defs.h"

#ifdef THPOOL_DEBUG
#define THPOOL_DEBUG 1
#else
#define THPOOL_DEBUG 0
#endif

#if !defined(DISABLE_PRINT) || defined(THPOOL_DEBUG)
#define err(str) fprintf(stderr, str)
#else
#define err(str)
#endif

static volatile int threads_keepalive;
static volatile int threads_on_hold;



/* ========================== STRUCTURES ============================ */


/* Binary semaphore */
typedef struct bsem {
	pthread_mutex_t mutex;
	pthread_cond_t   cond;
	int v;
} bsem;


/* Job */
typedef struct job{
	struct job*  prev;                   /* pointer to previous job   */
	void   (*function)(void* arg);       /* function pointer          */
	void*  arg;                          /* function's argument       */
#ifdef PROFILE_THPOOL
	struct timespec time_added;
	struct timespec time_scheduled;
#endif
} job;


/* Job queue */
typedef struct jobqueue{
	pthread_mutex_t rwmutex;             /* used for queue r/w access */
	job  *front;                         /* pointer to front of queue */
	job  *rear;                          /* pointer to rear  of queue */
	bsem *has_jobs;                      /* flag as binary semaphore  */
	int   len;                           /* number of jobs in queue   */
#ifdef PROFILE_JOBQUEUE_LEN
	int len_100; 		     /* rounded down at hundred digit */
#endif
} jobqueue;

/* Thread */
typedef struct thread{
	int       id;                        /* friendly id               */
	pthread_t pthread;                   /* pointer to actual thread  */
	struct thpool_* thpool_p;            /* access to thpool          */
#ifdef PROFILE_THPOOL
	unsigned long schedule_cnt;
	double schedule_delay_sum;
	double schedule_delay_max;
	double schedule_delay_min;
#endif
	void (*stat_print_func_p)(void*); // function to print stats.
} thread;


/* Threadpool */
typedef struct thpool_{
	thread**   threads;                  /* pointer to threads        */
	volatile int num_threads_alive;      /* threads currently alive   */
	volatile int num_threads_working;    /* threads currently working */
	pthread_mutex_t  thcount_lock;       /* used for thread count etc */
	pthread_cond_t  threads_all_idle;    /* signal to thpool_wait     */
	jobqueue  jobqueue;                  /* job queue                 */
	int no_sleep;			     /* busy waiting instead of sleep  */
	char name[64];			     /* thread pool name	  */
} thpool_;





/* ========================== PROTOTYPES ============================ */


static int  thread_init(thpool_* thpool_p, struct thread** thread_p, int id);
static void* thread_do(struct thread* thread_p);
static void  thread_hold(int sig_id);
static void  thread_destroy(struct thread* thread_p);

static int   jobqueue_init(jobqueue* jobqueue_p);
static void  jobqueue_clear(jobqueue* jobqueue_p);
static void  jobqueue_push(jobqueue* jobqueue_p, struct job* newjob_p);
static struct job* jobqueue_pull(jobqueue* jobqueue_p);
static void  jobqueue_destroy(jobqueue* jobqueue_p);

static void  bsem_init(struct bsem *bsem_p, int value);
static void  bsem_reset(struct bsem *bsem_p);
static void  bsem_post(struct bsem *bsem_p);
static void  bsem_post_all(struct bsem *bsem_p);
static void  bsem_wait(struct bsem *bsem_p);
static void  bsem_wait_no_sleep(struct bsem *bsem_p);

// static int thread_init_with_pipeline_profile(thpool_ *thpool_p,
//                                              struct thread **thread_p, int id,
//                                              void (*stat_print_func_p)(void*));




/* ========================== THREADPOOL ============================ */

struct thpool_* __thpool_init(int num_threads, const char *name, int no_sleep){
	if (num_threads <= 0) {
		printf("[Warn] %s thread num = 0. It is not created.\n", name);
		return NULL;
	}

	threads_on_hold   = 0;
	threads_keepalive = 1;

	if (num_threads < 0){
		num_threads = 0;
	}

	/* Make new thread pool */
	thpool_* thpool_p;
	thpool_p = (struct thpool_*)malloc(sizeof(struct thpool_));
	if (thpool_p == NULL){
		err("thpool_init(): Could not allocate memory for thread pool\n");
		return NULL;
	}
	thpool_p->num_threads_alive   = 0;
	thpool_p->num_threads_working = 0;
	thpool_p->no_sleep = no_sleep;
	if (name)
	    sprintf(thpool_p->name, "_%s", name);
	else
	    sprintf(thpool_p->name, "thpool");

	/* Initialise the job queue */
	if (jobqueue_init(&thpool_p->jobqueue) == -1){
		err("thpool_init(): Could not allocate memory for job queue\n");
		free(thpool_p);
		return NULL;
	}

	/* Make threads in pool */
	thpool_p->threads = (struct thread**)malloc(num_threads * sizeof(struct thread *));
	if (thpool_p->threads == NULL){
		err("thpool_init(): Could not allocate memory for threads\n");
		jobqueue_destroy(&thpool_p->jobqueue);
		free(thpool_p);
		return NULL;
	}

	pthread_mutex_init(&(thpool_p->thcount_lock), NULL);
	pthread_cond_init(&thpool_p->threads_all_idle, NULL);

	/* Thread init */
	int n;
	for (n=0; n<num_threads; n++){
		thread_init(thpool_p, &thpool_p->threads[n], n);
#if THPOOL_DEBUG
		printf("THPOOL_DEBUG: Created thread %d in pool \n", n);
#endif
	}

	/* Wait for threads to initialize */
	while (thpool_p->num_threads_alive != num_threads) {}

	return thpool_p;
}

//struct thpool_ *
//__thpool_init_with_pipeline_profile(int num_threads, const char *name,
//				    int no_sleep,
//				    void (*stat_print_func_p)(void*))
//{
//	mlfs_assert(num_threads > 0);
//
//	threads_on_hold   = 0;
//	threads_keepalive = 1;
//
//	if (num_threads < 0){
//		num_threads = 0;
//	}
//
//	/* Make new thread pool */
//	thpool_* thpool_p;
//	thpool_p = (struct thpool_*)malloc(sizeof(struct thpool_));
//	if (thpool_p == NULL){
//		err("thpool_init(): Could not allocate memory for thread pool\n");
//		return NULL;
//	}
//	thpool_p->num_threads_alive   = 0;
//	thpool_p->num_threads_working = 0;
//	thpool_p->no_sleep = no_sleep;
//	if (name)
//	    sprintf(thpool_p->name, "tp-%s", name);
//	else
//	    sprintf(thpool_p->name, "thpool");
//
//	/* Initialise the job queue */
//	if (jobqueue_init(&thpool_p->jobqueue) == -1){
//		err("thpool_init(): Could not allocate memory for job queue\n");
//		free(thpool_p);
//		return NULL;
//	}
//
//	/* Make threads in pool */
//	thpool_p->threads = (struct thread**)malloc(num_threads * sizeof(struct thread *));
//	if (thpool_p->threads == NULL){
//		err("thpool_init(): Could not allocate memory for threads\n");
//		jobqueue_destroy(&thpool_p->jobqueue);
//		free(thpool_p);
//		return NULL;
//	}
//
//	pthread_mutex_init(&(thpool_p->thcount_lock), NULL);
//	pthread_cond_init(&thpool_p->threads_all_idle, NULL);
//
//	/* Thread init */
//	int n;
//	for (n=0; n<num_threads; n++){
//		thread_init_with_pipeline_profile(
//			thpool_p, &thpool_p->threads[n], n, stat_print_func_p);
//#if THPOOL_DEBUG
//		printf("THPOOL_DEBUG: Created thread %d in pool \n", n);
//#endif
//	}
//
//	/* Wait for threads to initialize */
//	while (thpool_p->num_threads_alive != num_threads) {}
//
//	return thpool_p;
//}

/* Initialise thread pool */
struct thpool_* thpool_init(int num_threads, const char *thpool_name) {

	return __thpool_init(num_threads, thpool_name, 0);
}

//struct thpool_ *
//thpool_init_with_pipeline_profile(int num_threads, const char *thpool_name,
//				  void (*stat_print_func_p)(void*))
//{
//	return __thpool_init_with_pipeline_profile(num_threads, thpool_name, 0,
//						   stat_print_func_p);
//}

/* Initialise thread pool that does not sleep on waiting. */
struct thpool_* thpool_init_no_sleep(int num_threads, const char *thpool_name){

	return __thpool_init(num_threads, thpool_name, 1);
}

/* Add work to the thread pool */
int thpool_add_work(thpool_* thpool_p, void (*function_p)(void*), void* arg_p){
	job* newjob;

	newjob=(struct job*)malloc(sizeof(struct job));
	if (newjob==NULL){
		panic("thpool_add_work(): Could not allocate memory for new job\n");
		// err("thpool_add_work(): Could not allocate memory for new job\n");
		// return -1;
	}

	/* add function and argument */
	newjob->function=function_p;
	newjob->arg=arg_p;

#ifdef PROFILE_THPOOL
	// Check job added time.
	clock_gettime(CLOCK_MONOTONIC, &newjob->time_added);
#endif

	/* add job to queue */
	jobqueue_push(&thpool_p->jobqueue, newjob);
#ifdef PROFILE_JOBQUEUE_LEN
	if (thpool_p->jobqueue.len > 1000) {
		int temp_len_100 = (thpool_p->jobqueue.len/100) * 100;
		if (thpool_p->jobqueue.len_100 != temp_len_100) {
			thpool_p->jobqueue.len_100 = temp_len_100;
			printf("%lu WARN More than 1000 jobs in jobqueue. "
			       "jobqueue_len=%d thpool_name=%s\n",
			       get_tid(), thpool_p->jobqueue.len_100,
			       thpool_p->name);
		}


	}
#endif

	return 0;
}


/* Wait until all jobs have finished */
void thpool_wait(thpool_* thpool_p){
	pthread_mutex_lock(&thpool_p->thcount_lock);
	while (thpool_p->jobqueue.len || thpool_p->num_threads_working) {
		pthread_cond_wait(&thpool_p->threads_all_idle, &thpool_p->thcount_lock);
	}
	pthread_mutex_unlock(&thpool_p->thcount_lock);
}


/* Destroy the threadpool */
void thpool_destroy(thpool_* thpool_p){
	/* No need to destory if it's NULL */
	if (thpool_p == NULL) return ;

	volatile int threads_total = thpool_p->num_threads_alive;

	/* End each thread 's infinite loop */
	threads_keepalive = 0;

	/* Give one second to kill idle threads */
	double TIMEOUT = 1.0;
	time_t start, end;
	double tpassed = 0.0;
	time (&start);
	while (tpassed < TIMEOUT && thpool_p->num_threads_alive){
		bsem_post_all(thpool_p->jobqueue.has_jobs);
		time (&end);
		tpassed = difftime(end,start);
	}

	/* Poll remaining threads */
	while (thpool_p->num_threads_alive){
		bsem_post_all(thpool_p->jobqueue.has_jobs);
		sleep(1);
	}

	/* Job queue cleanup */
	jobqueue_destroy(&thpool_p->jobqueue);
	/* Deallocs */
	int n;
	for (n=0; n < threads_total; n++){
		thread_destroy(thpool_p->threads[n]);
	}
	free(thpool_p->threads);
	free(thpool_p);
}


/* Pause all threads in threadpool */
void thpool_pause(thpool_* thpool_p) {
	int n;
	for (n=0; n < thpool_p->num_threads_alive; n++){
		pthread_kill(thpool_p->threads[n]->pthread, SIGUSR1);
	}
}


/* Resume all threads in threadpool */
void thpool_resume(thpool_* thpool_p) {
    // resuming a single threadpool hasn't been
    // implemented yet, meanwhile this supresses
    // the warnings
    (void)thpool_p;

	threads_on_hold = 0;
}


int thpool_num_threads_working(thpool_* thpool_p){
	return thpool_p->num_threads_working;
}

#ifdef PROFILE_THPOOL
void print_profile_result(thpool_ *thpool_p)
{
	if (!thpool_p)
		return;

	int n;
	double sum = 0.0, avg = 0.0, max = 0.0;
	double min = 99999.99; //sufficiently large value for min.
	unsigned long cnt = 0;
	for (n = 0; n < thpool_p->num_threads_alive; n++) {
		if (!thpool_p->threads[n]->schedule_cnt)
			continue;
		sum += thpool_p->threads[n]->schedule_delay_sum;
		cnt += thpool_p->threads[n]->schedule_cnt;

		if (thpool_p->threads[n]->schedule_delay_max > max)
			max = thpool_p->threads[n]->schedule_delay_max;

		if (thpool_p->threads[n]->schedule_delay_min &&
		    thpool_p->threads[n]->schedule_delay_min < min)
			min = thpool_p->threads[n]->schedule_delay_min;
	}

	if (cnt) {
		avg = (double)sum / cnt;
		printf("Thread_name %30s\n", thpool_p->name);
		printf("Total_scheduled_cnt %19lu\n", cnt);
		printf("Schedule_delay_avg(us) %16.4f\n", avg * 1000000.0);
		printf("Schedule_delay_min(us) %16.4f\n", min * 1000000.0);
		printf("Schedule_delay_max(us) %16.4f\n", max * 1000000.0);
		printf("--------------------- --------\n");
	} else {
		// printf("Thread name: %s\n", thpool_p->name);
		// printf("Total scheduled cnt: %lu\n", cnt);
		// printf("----------------------------------------------\n");
	}
}

#endif

// It doesn't work with TLS event.
/**
 * @Synopsis  It execute print_stat_func registered in each thread.
 *
 * @Param thpool_p
 */
//void print_per_thread_pipeline_stat(thpool_ *thpool_p)
//{
//	int n;
//	void (*func_buff)(void*);
//
//	if (!thpool_p)
//		return;
//
//	for (n = 0; n < thpool_p->num_threads_alive; n++) {
//		printf("Thread_name %s_%d\n", thpool_p->name, n);
//		if (!thpool_p->threads[n]->stat_print_func_p)
//			continue;
//
//		// Print per thread stats.
//		func_buff = thpool_p->threads[n]->stat_print_func_p;
//		func_buff(NULL);
//	}
//}

/* ============================ THREAD ============================== */


/* Initialize a thread in the thread pool
 *
 * @param thread        address to the pointer of the thread to be created
 * @param id            id to be given to the thread
 * @return 0 on success, -1 otherwise.
 */
static int thread_init (thpool_* thpool_p, struct thread** thread_p, int id){

	*thread_p = (struct thread*)malloc(sizeof(struct thread));
	if (*thread_p == NULL){
		err("thread_init(): Could not allocate memory for thread\n");
		return -1;
	}

	(*thread_p)->thpool_p = thpool_p;
	(*thread_p)->id       = id;

#ifdef PROFILE_THPOOL
	// Init profile variables.
	(*thread_p)->schedule_cnt = 0;
	(*thread_p)->schedule_delay_sum = 0.0;
	(*thread_p)->schedule_delay_max = 0.0;
	(*thread_p)->schedule_delay_min = 99999.99; //large value
#endif
	(*thread_p)->stat_print_func_p = NULL;

	pthread_create(&(*thread_p)->pthread, NULL, (void *)thread_do, (*thread_p));
	pthread_detach((*thread_p)->pthread);
	return 0;
}

//static int thread_init_with_pipeline_profile(thpool_ *thpool_p,
//					     struct thread **thread_p, int id,
//					     void (*stat_print_func_p)(void*))
//{
//	*thread_p = (struct thread *)malloc(sizeof(struct thread));
//	if (*thread_p == NULL) {
//		err("thread_init(): Could not allocate memory for thread\n");
//		return -1;
//	}
//
//	(*thread_p)->thpool_p = thpool_p;
//	(*thread_p)->id = id;
//
//#ifdef PROFILE_THPOOL
//	// Init profile variables.
//	(*thread_p)->schedule_cnt = 0;
//	(*thread_p)->schedule_delay_sum = 0.0;
//	(*thread_p)->schedule_delay_max = 0.0;
//	(*thread_p)->schedule_delay_min = 99999.99; // large value
//#endif
//	(*thread_p)->stat_print_func_p = stat_print_func_p;
//
//	pthread_create(&(*thread_p)->pthread, NULL, (void *)thread_do,
//		       (*thread_p));
//	pthread_detach((*thread_p)->pthread);
//	return 0;
//}

/* Sets the calling thread on hold */
static void thread_hold(int sig_id) {
    (void)sig_id;
	threads_on_hold = 1;
	while (threads_on_hold){
		sleep(1);
	}
}


/* What each thread is doing
*
* In principle this is an endless loop. The only time this loop gets interuppted is once
* thpool_destroy() is invoked or the program exits.
*
* @param  thread        thread that will run this function
* @return nothing
*/
static void* thread_do(struct thread* thread_p){

	/* Set thread name for profiling and debuging */
	char thread_name[128] = {0};

#ifdef PROFILE_THPOOL
	double duration;
#endif
        sprintf(thread_name, "%s_%d", thread_p->thpool_p->name,
                thread_p->id);

#if defined(__linux__)
	/* Use prctl instead to prevent using _GNU_SOURCE flag and implicit declaration */
	prctl(PR_SET_NAME, thread_name);
#elif defined(__APPLE__) && defined(__MACH__)
	pthread_setname_np(thread_name);
#else
	err("thread_do(): pthread_setname_np is not supported on this system");
#endif

	/* Assure all threads have been created before starting serving */
	thpool_* thpool_p = thread_p->thpool_p;

	/* Register signal handler */
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = thread_hold;
	if (sigaction(SIGUSR1, &act, NULL) == -1) {
		err("thread_do(): cannot handle SIGUSR1");
	}

	/* Mark thread as alive (initialized) */
	pthread_mutex_lock(&thpool_p->thcount_lock);
	thpool_p->num_threads_alive += 1;
	pthread_mutex_unlock(&thpool_p->thcount_lock);

	while(threads_keepalive){
		if (thpool_p->no_sleep)
		    bsem_wait_no_sleep(thpool_p->jobqueue.has_jobs);
		else
		    bsem_wait(thpool_p->jobqueue.has_jobs);

		if (threads_keepalive){

			pthread_mutex_lock(&thpool_p->thcount_lock);
			thpool_p->num_threads_working++;
			pthread_mutex_unlock(&thpool_p->thcount_lock);

			/* Read job from queue and execute it */
			void (*func_buff)(void*);
			void*  arg_buff;
			job* job_p = jobqueue_pull(&thpool_p->jobqueue);
			if (job_p) {
#ifdef PROFILE_THPOOL
				// Check scheduled time.
				clock_gettime(CLOCK_MONOTONIC,
					      &job_p->time_scheduled);
				duration = get_duration(&job_p->time_added,
							&job_p->time_scheduled);
				thread_p->schedule_delay_sum += duration;
				thread_p->schedule_cnt++;
				if (duration > thread_p->schedule_delay_max)
					thread_p->schedule_delay_max = duration;
				if (duration < thread_p->schedule_delay_min)
					thread_p->schedule_delay_min = duration;
#endif

				func_buff = job_p->function;
				arg_buff  = job_p->arg;
				func_buff(arg_buff);
				free(job_p);
			}

			pthread_mutex_lock(&thpool_p->thcount_lock);
			thpool_p->num_threads_working--;
			if (!thpool_p->num_threads_working) {
				pthread_cond_signal(&thpool_p->threads_all_idle);
			}
			pthread_mutex_unlock(&thpool_p->thcount_lock);

		}
	}
	pthread_mutex_lock(&thpool_p->thcount_lock);
	thpool_p->num_threads_alive --;
	pthread_mutex_unlock(&thpool_p->thcount_lock);

	return NULL;
}


/* Frees a thread  */
static void thread_destroy (thread* thread_p){
	free(thread_p);
}





/* ============================ JOB QUEUE =========================== */


/* Initialize queue */
static int jobqueue_init(jobqueue* jobqueue_p){
	jobqueue_p->len = 0;
	jobqueue_p->front = NULL;
	jobqueue_p->rear  = NULL;

	jobqueue_p->has_jobs = (struct bsem*)malloc(sizeof(struct bsem));
	if (jobqueue_p->has_jobs == NULL){
		return -1;
	}

	pthread_mutex_init(&(jobqueue_p->rwmutex), NULL);
	bsem_init(jobqueue_p->has_jobs, 0);

	return 0;
}


/* Clear the queue */
static void jobqueue_clear(jobqueue* jobqueue_p){

	while(jobqueue_p->len){
		free(jobqueue_pull(jobqueue_p));
	}

	jobqueue_p->front = NULL;
	jobqueue_p->rear  = NULL;
	bsem_reset(jobqueue_p->has_jobs);
	jobqueue_p->len = 0;

}


/* Add (allocated) job to queue
 */
static void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob){

	pthread_mutex_lock(&jobqueue_p->rwmutex);
	newjob->prev = NULL;

	switch(jobqueue_p->len){

		case 0:  /* if no jobs in queue */
					jobqueue_p->front = newjob;
					jobqueue_p->rear  = newjob;
					break;

		default: /* if jobs in queue */
					jobqueue_p->rear->prev = newjob;
					jobqueue_p->rear = newjob;

	}
	jobqueue_p->len++;

	bsem_post(jobqueue_p->has_jobs);
	pthread_mutex_unlock(&jobqueue_p->rwmutex);
}


/* Get first job from queue(removes it from queue)
 *
 * Notice: Caller MUST hold a mutex
 */
static struct job* jobqueue_pull(jobqueue* jobqueue_p){

	pthread_mutex_lock(&jobqueue_p->rwmutex);
	job* job_p = jobqueue_p->front;

	switch(jobqueue_p->len){

		case 0:  /* if no jobs in queue */
		  			break;

		case 1:  /* if one job in queue */
					jobqueue_p->front = NULL;
					jobqueue_p->rear  = NULL;
					jobqueue_p->len = 0;
					break;

		default: /* if >1 jobs in queue */
					jobqueue_p->front = job_p->prev;
					jobqueue_p->len--;
					/* more than one job in queue -> post it */
					bsem_post(jobqueue_p->has_jobs);

	}

	pthread_mutex_unlock(&jobqueue_p->rwmutex);
	return job_p;
}


/* Free all queue resources back to the system */
static void jobqueue_destroy(jobqueue* jobqueue_p){
	jobqueue_clear(jobqueue_p);
	free(jobqueue_p->has_jobs);
}





/* ======================== SYNCHRONISATION ========================= */


/* Init semaphore to 1 or 0 */
static void bsem_init(bsem *bsem_p, int value) {
	if (value < 0 || value > 1) {
		err("bsem_init(): Binary semaphore can take only values 1 or 0");
		exit(1);
	}
	pthread_mutex_init(&(bsem_p->mutex), NULL);
	pthread_cond_init(&(bsem_p->cond), NULL);
	bsem_p->v = value;
}


/* Reset semaphore to 0 */
static void bsem_reset(bsem *bsem_p) {
	bsem_init(bsem_p, 0);
}


/* Post to at least one thread */
static void bsem_post(bsem *bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	bsem_p->v = 1;
	pthread_cond_signal(&bsem_p->cond);
	pthread_mutex_unlock(&bsem_p->mutex);
}


/* Post to all threads */
static void bsem_post_all(bsem *bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	bsem_p->v = 1;
	pthread_cond_broadcast(&bsem_p->cond);
	pthread_mutex_unlock(&bsem_p->mutex);
}


/* Wait on semaphore until semaphore has value 0 */
static void bsem_wait(bsem* bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	while (bsem_p->v != 1) {
		pthread_cond_wait(&bsem_p->cond, &bsem_p->mutex);
	}
	bsem_p->v = 0;
	pthread_mutex_unlock(&bsem_p->mutex);
}

/* Wait on semaphore until semaphore has value 0 */
static void bsem_wait_no_sleep(bsem* bsem_p) {
	while (1) {
		pthread_mutex_lock(&bsem_p->mutex);
		if (bsem_p->v == 1) {
		    bsem_p->v = 0;
		    pthread_mutex_unlock(&bsem_p->mutex);
		    break;
		}
		pthread_mutex_unlock(&bsem_p->mutex);
		cpu_relax();
	}
}
