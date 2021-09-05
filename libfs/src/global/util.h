#ifndef _UTIL_H_
#define _UTIL_H_

#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/socket.h>
#include "global/global.h"
#include <stdio.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define HEXDUMP_COLS 8

#ifdef __GNUC__
#define TYPEOF(x) (__typeof__(x))
#else
#define TYPEOF(x)
#endif

#if defined(__i386__)

static inline unsigned long long asm_rdtsc(void)
{
	unsigned long long int x;
	__asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
	return x;
}

static inline unsigned long long asm_rdtscp(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi)::"ecx");
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );

}
#elif defined(__x86_64__)

static inline unsigned long long asm_rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

static inline unsigned long long asm_rdtscp(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi)::"rcx");
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#elif defined(__aarch64__)
static inline unsigned long long asm_rdtsc(void)
{
	// TODO to be implemented.
        printf ("WARNING: %s is not implemented in aarch64 architecture.\n", __func__);
	return 0;
}

static inline unsigned long long asm_rdtscp(void)
{
	// TODO to be implemented.
        printf ("WARNING: %s is not implemented in aarch64 architecture.\n", __func__);
	return 0;
}
#else
#error "Only support for X86 architecture"
#endif

#define IS_ALIGNED(x, a) (((x) & ((__typeof__(x))(a) - 1)) == 0)

#ifdef __cplusplus
#define ALIGN(x, a)  ALIGN_MASK((x), ((__typeof__(x))(a) - 1))
#else
#define ALIGN(x, a)  ALIGN_MASK((x), ((typeof(x))(a) - 1))
#endif
#define ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))

static inline unsigned long ALIGN_FLOOR(unsigned long x, int mask)
{
	if (IS_ALIGNED(x, mask))
		return x;
	else
		return ALIGN(x, mask) - mask;
}

int is_power_of_two(unsigned long x);
unsigned int get_rand_interval(unsigned int min, unsigned int max);
void hexdump(void *mem, unsigned int len);
float get_cpu_clock_speed(void);
int sockaddr_cmp(struct sockaddr *x, struct sockaddr *y);
int get_cpuid(void);
int fetch_intf_ip(char* intf, char* host);
void send_ready_signal(const char* dir);

// for directory path management

static inline int collapse_name(const char *input, char *_output)
{
	char *output = _output;

	while(1) {
		/* Detect a . or .. component */
		if (input[0] == '.') {
			if (input[1] == '.' && input[2] == '/') {
				/* A .. component */
				if (output == _output)
					return -1;
				input += 2;
				while (*(++input) == '/');
				while(--output != _output && *(output - 1) != '/');
				continue;
			} else if (input[1] == '/') {
				/* A . component */
				input += 1;
				while (*(++input) == '/');
				continue;
			}
		}

		/* Copy from here up until the first char of the next component */
		while(1) {
			*output++ = *input++;
			if (*input == '/') {
				*output++ = '/';
				/* Consume any extraneous separators */
				while (*(++input) == '/');
				break;
			} else if (*input == 0) {
				*output = 0;
				return output - _output;
			}
		}
	}
}

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   get_next_name("a/bb/c", name) = "bb/c", setting name = "a"
//   get_next_name("///a//bb", name) = "bb", setting name = "a"
//   get_next_name("a", name) = "", setting name = "a"
//   get_next_name("", name) = get_next_name("////", name) = 0
//
static inline char* get_next_name(char *path, char *name)
{
	char *s;
	int len;

	while (*path == '/')
		path++;
	if (*path == 0)
		return 0;
	s = path;
	while (*path != '/' && *path != 0)
		path++;
	len = path - s;
	if (len >= DIRSIZ)
		memmove(name, s, DIRSIZ);
	else {
		memmove(name, s, len);
		name[len] = 0;
	}
	while (*path == '/')
		path++;
	return path;
}

/* /mlfs/aa/bb/c -> /mlfs/aa/bb (parent path) and c (name) */
static inline char* get_parent_path(char *path, char *parent_path, char *name)
{
	int len = strlen(path);
	char *_path = path;

	path += (len - 1);

	while (*path == '0') {
		path--;
		len--;
	}

	while (*path == '/') {
		path--;
		len--;
	}

	while (*path != '/' && len > 0) {
		len--;
		path--;
	}

	if (len == 0)
		return NULL;

	while (*path == '/') {
		len--;
		path--;
	}

	memmove(parent_path, _path, len);
	get_next_name(&_path[len], name);

	parent_path[len] = '\0';

	return parent_path;
}

struct event_timer {
	unsigned long   count;
	double          time_sum;
	struct timespec start_time;
	struct timespec end_time;
	int used;   // for validation.

	// MP related.
	int ref_count;
	pthread_spinlock_t update_lock; // lock for time_sum and count.
	pthread_spinlock_t ref_count_lock; // lock for ref_count.
	uint64_t data_size; // Used to store the size of data processed.
};

static inline double get_duration(struct timespec *time_start, struct timespec *time_end)
{
       double start_sec = (double)(time_start->tv_sec * 1000000000.0 + (double)time_start->tv_nsec) / 1000000000.0;
       double end_sec = (double)(time_end->tv_sec * 1000000000.0 + (double)time_end->tv_nsec) / 1000000000.0;
       return end_sec - start_sec;
}

static inline uint64_t get_duration_in_ns(struct timespec *time_start, struct timespec *time_end)
{
       uint64_t start_sec = time_start->tv_sec * 1000000000 + (double)time_start->tv_nsec;
       uint64_t end_sec = time_end->tv_sec * 1000000000 + (double)time_end->tv_nsec;
       return end_sec - start_sec;
}

static inline double get_time(struct timespec *time)
{
       return (double)(time->tv_sec * 1000000000.0 + (double)time->tv_nsec) / 1000000000.0;
}

struct realtime_bw_stat {
	uint64_t bytes_until_now;
	struct timespec start_time;
	struct timespec end_time;
	pthread_spinlock_t lock;
	char *name;
};
typedef struct realtime_bw_stat rt_bw_stat;

static inline void init_rt_bw_stat(rt_bw_stat *stat, char *name)
{
	if (!stat) {
		printf("[Warn] Real time bandwidth stat is not allocated.\n");
		return;
	}
	stat->name = name;
	pthread_spin_init(&stat->lock, PTHREAD_PROCESS_PRIVATE);
}

static inline void check_rt_bw(rt_bw_stat *stat, uint64_t sent_bytes)
{
	if (!stat) {
		printf("[Warn] Real time bandwidth stat is not allocated.\n");
		return;
	}

	pthread_spin_lock(&stat->lock);

	if (stat->start_time.tv_sec == 0) { // First run.
		clock_gettime(CLOCK_MONOTONIC, &stat->start_time);
	}

	clock_gettime(CLOCK_MONOTONIC, &stat->end_time);

	if (get_duration(&stat->start_time, &stat->end_time) > 1.0) {
		printf("[RT_BW %s]: %lu MB/s\n", stat->name,
		       stat->bytes_until_now >> 20);

		clock_gettime(CLOCK_MONOTONIC, &stat->start_time);
		stat->bytes_until_now = 0;
	}

	stat->bytes_until_now += sent_bytes;

	pthread_spin_unlock(&stat->lock);
}

#define TO_STR(x) __STR_VALUE(x)
#define __STR_VALUE(x) #x

/* Thread local events. */
#ifdef PROFILE_PIPELINE
#define TL_EVENT_TIMER(event) __thread struct event_timer *event = NULL;

// Free event. START_TL_TIMER will re-alloc.
#define RESET_TL_TIMER(event)                                                  \
	do {                                                                   \
		if ((event)) {                                                 \
			mlfs_free(event);                                      \
		}                                                              \
	} while (0)

#define START_TL_TIMER(event)                                                  \
	do {                                                                   \
		if (!(event)) {                                                \
			event = (struct event_timer *)mlfs_zalloc(             \
				sizeof(struct event_timer));                   \
			printf("%lu TIMER_INIT: " #event "\n", get_tid());     \
		}                                                              \
		if ((event)->used) {                                           \
			printf("START_TIMER: timer already used. [tid:%lu "    \
			       "%s():%d]\n",                                   \
			       get_tid(), __func__, __LINE__);                 \
		}                                                              \
		clock_gettime(CLOCK_MONOTONIC, &(event)->start_time);          \
		(event)->used = 1;                                             \
	} while (0)

#define END_TL_TIMER(event)                                                    \
	do {                                                                   \
		if (!(event)->used) {                                          \
			printf("END_TIMER: timer not started. [%s():%d]\n",    \
			       __func__, __LINE__);                            \
		}                                                              \
		clock_gettime(CLOCK_MONOTONIC, &(event)->end_time);            \
		(event)->time_sum += get_duration(&(event)->start_time,        \
						  &(event)->end_time);         \
		(event)->count++;                                              \
		(event)->used = 0;                                             \
	} while (0)

#define PRINT_TL_HDR()                                                         \
	do {                                                                   \
		printf("%-32s %12s %10s %6s\n", "evt_name", "usec", "count",   \
		       "avg");                                                 \
	} while (0)

#define PRINT_TL_FMT "%lu_%-32s %12.2f %10lu %6.2f\n"
#define PRINT_TL_TIMER(event, th_id)                                           \
	do {                                                                   \
		if (event) {                                                   \
			printf(PRINT_TL_FMT, (uint64_t)th_id, #event,          \
			       event->time_sum * 1000000.0, event->count,      \
			       event->time_sum * 1000000.0 / event->count);    \
		}                                                              \
	} while (0)
#else
#define TL_EVENT_TIMER(event)
#define RESET_TL_TIMER(event)
#define START_TL_TIMER(event)
#define END_TL_TIMER(event)
#define PRINT_TL_HDR()
#define PRINT_TL_TIMER(event, th_id)
#endif /* PROFILE_PIPELINE */

#define EVENT_TIMER(event) struct event_timer event; \

#define RESET_TIMER(event) \
    do { \
	if (mlfs_conf.breakdown) { \
	    event = ((struct event_timer) {0,}); \
	    if (mlfs_conf.breakdown_mp) { \
		pthread_spin_destroy(&(event).update_lock); \
		pthread_spin_destroy(&(event).ref_count_lock); \
		pthread_spin_init(&(event).update_lock, PTHREAD_PROCESS_SHARED); \
		pthread_spin_init(&(event).ref_count_lock, PTHREAD_PROCESS_SHARED); \
	    } \
	} \
    } while (0)

#define START_TIMER(event) \
    do { \
	if (mlfs_conf.breakdown && !mlfs_conf.breakdown_mp) { \
	    if ((event).used) { \
		printf("START_TIMER: timer already used. [tid:%lu %s():%d]\n", \
			get_tid(), __func__, __LINE__); \
	    } \
	    clock_gettime(CLOCK_MONOTONIC, &(event).start_time); \
	    (event).used = 1; \
	} \
    } while (0)

#define END_TIMER(event) \
    do { \
	if (mlfs_conf.breakdown && !mlfs_conf.breakdown_mp) { \
	    if (!(event).used) { \
		printf("END_TIMER: timer not started. [%s():%d]\n", \
			__func__, __LINE__); \
	    } \
	    clock_gettime(CLOCK_MONOTONIC, &(event).end_time); \
	    (event).time_sum += get_duration(&(event).start_time, &(event).end_time); \
	    (event).count++; \
	    (event).used = 0; \
	} \
    } while (0)

// Limitation: START_MP_PER_TASK_TIMER and END_MP_PER_TASK_TIMER should be called in the same function.
// They are per thread timers.
#define START_MP_PER_TASK_TIMER(event) \
    struct timespec event##_start_time; \
    do { \
	if (mlfs_conf.breakdown_mp) { \
	    clock_gettime(CLOCK_MONOTONIC, &(event##_start_time)); \
	} \
    } while (0)
	    // printf("START_TIMER: %lu %s():%d\n", get_tid(), __func__, __LINE__); \

#define END_MP_PER_TASK_TIMER(event) \
    do { \
	if (mlfs_conf.breakdown_mp) { \
	    struct timespec event##_end_time; \
	    clock_gettime(CLOCK_MONOTONIC, &(event##_end_time)); \
	    pthread_spin_lock(&(event).update_lock); \
	    (event).time_sum += get_duration(&(event##_start_time), &(event##_end_time)); \
	    (event).count++; \
	    pthread_spin_unlock(&(event).update_lock); \
	} \
    } while (0)
	    // printf("END_TIMER: %lu %s():%d\n", get_tid(), __func__, __LINE__); \

#define END_MP_PER_TASK_TIMER_AND_INC_DATA_SIZE(event, d_size) \
    do { \
	if (mlfs_conf.breakdown_mp) { \
	    struct timespec event##_end_time; \
	    clock_gettime(CLOCK_MONOTONIC, &(event##_end_time)); \
	    pthread_spin_lock(&(event).update_lock); \
	    (event).time_sum += get_duration(&(event##_start_time), &(event##_end_time)); \
	    (event).count++; \
	    (event).data_size += d_size; \
	    pthread_spin_unlock(&(event).update_lock); \
	} \
    } while (0)

#define START_MP_TOTAL_TIMER(event) \
    do { \
	if (mlfs_conf.breakdown_mp) { \
	    pthread_spin_lock(&(event).ref_count_lock); \
	    mlfs_assert((event).ref_count >= 0); \
	    if ((event).ref_count == 0) { \
		clock_gettime(CLOCK_MONOTONIC, &(event).start_time); \
	    } \
	    (event).ref_count++; \
	    pthread_spin_unlock(&(event).ref_count_lock); \
	} \
    } while (0)
		// printf("START_TOTAL_TIMER: %lu %s():%d\n", get_tid(), __func__, __LINE__); \

#define END_MP_TOTAL_TIMER(event) \
    do { \
	if (mlfs_conf.breakdown_mp) { \
	    pthread_spin_lock(&(event).ref_count_lock); \
	    mlfs_assert((event).ref_count >= 0); \
	    if ((event).ref_count == 1) { \
		clock_gettime(CLOCK_MONOTONIC, &(event).end_time); \
		(event).time_sum += get_duration(&(event).start_time, &(event).end_time); \
		(event).count++; \
	    } \
	    (event).ref_count--; \
	    pthread_spin_unlock(&(event).ref_count_lock); \
	} \
    } while (0)
		// printf("END_TOTAL_TIMER: %lu %s():%d\n", get_tid(), __func__, __LINE__); \

#define PRINT_HDR() \
    do { \
	printf(",%-30s, %12s, %10s, %6s, %6s,\n", "evt_name", "usec", "count", "avg", "%"); \
    } while (0)
#define PRINT_MP_HDR() \
    do { \
	printf(",%-30s, %12s, %10s,\n", "evt_name", "usec", "count"); \
    } while (0)

#define PRINT_FMT ",%-30s, %12.2f, %10lu, %6.2f, %6.2f %%,\n"
#define PRINT_TIMER(event, desc, event_denom) \
    printf(PRINT_FMT, \
	    desc, \
	    event.time_sum * 1000000.0, \
	    event.count, \
	    event.time_sum * 1000000.0 / event.count, \
	    (event.time_sum * 1000000.0)*100/(event_denom.time_sum * 1000000.0))

#define PRINT_FMT_MP ",%-30s, %12.2f, %10lu,\n"
#define PRINT_MP_TIMER(event, desc) \
    printf(PRINT_FMT_MP, \
	    desc, \
	    event.time_sum * 1000000.0, \
	    event.count)

#define PRINT_START_TIME(event, desc) \
    printf("%-30s %lu %8.4f\n", \
	    desc, \
	    get_tid(), \
	    get_time(&(event).start_time))

#define PRINT_END_TIME(event, desc) \
    printf("%-30s %lu %8.4f\n", \
	    desc, \
	    get_tid(), \
	    get_time(&(event).end_time))

//////// Breakdown timers.
//// Common
EVENT_TIMER(evt_rep_create_rsync);
EVENT_TIMER(evt_rep_start_session);
EVENT_TIMER(evt_rep_gen_rsync_msgs);
EVENT_TIMER(evt_rep_rdma_add_sge);
EVENT_TIMER(evt_rep_rdma_finalize);
EVENT_TIMER(evt_rep_grm_nr_blks);
EVENT_TIMER(evt_rep_grm_update_ptrs);
EVENT_TIMER(evt_rep_log_copy); /* TODO Re-investigate it. Not sure about below.
				* <With 3 replicas>
				*   Replica 1: posting RDMA request (not
				*              including waiting for WR
				*              completion).
				*   Replica 2: posting RDMA request + time of
				*              waiting for WR completion.
				* <With 2 replicas>
				*   Includes WR completion time.
                                */
EVENT_TIMER(evt_rep_build_msg);
EVENT_TIMER(evt_rep_log_copy_write);
EVENT_TIMER(evt_rep_log_copy_read);
EVENT_TIMER(evt_rep_log_copy_compl);
EVENT_TIMER(evt_rep_send_and_wait_msg); // LibFS's waiting time for replication.

//// Libfs
// replication timers
EVENT_TIMER(evt_rep_by_fsync);
EVENT_TIMER(evt_wait_fsync_ack);
EVENT_TIMER(evt_prefetch_req);
EVENT_TIMER(evt_rep_by_digest);
EVENT_TIMER(evt_rep_libfs_wait_ack);
// digestion timers
EVENT_TIMER(evt_dig_wait_sync);

//// Kernfs
// EVENT_TIMER(evt_k_signal_callback);

// replication timers
EVENT_TIMER(evt_rep_local);	// replication time at local node.
EVENT_TIMER(evt_rep_chain);	// replication time at chain node.
EVENT_TIMER(evt_wait_log_prefetch);
EVENT_TIMER(evt_rep_read_log);
// EVENT_TIMER(evt_loghdr);
EVENT_TIMER(evt_build_loghdrs);
EVENT_TIMER(evt_empty_log_buf);
EVENT_TIMER(evt_loghdr_misc);
EVENT_TIMER(evt_send_loghdr);
EVENT_TIMER(evt_rep_wait_req_done);
EVENT_TIMER(evt_set_remote_bit);
EVENT_TIMER(evt_set_remote_bit1);

EVENT_TIMER(evt_rep_critical_host);
EVENT_TIMER(evt_rep_critical_host2);
EVENT_TIMER(evt_rep_critical_nic);
EVENT_TIMER(evt_handle_rpcmsg_misc);
EVENT_TIMER(evt_free_arg);
EVENT_TIMER(evt_reset_log_buf);
EVENT_TIMER(evt_atomic_add);
EVENT_TIMER(evt_atomic_test);
EVENT_TIMER(evt_clear_replicaing);
EVENT_TIMER(evt_rep_critical_nic2);
EVENT_TIMER(evt_wait_local_host_copy);

// lock wait.
EVENT_TIMER(evt_wait_loghdrs_lock_rep);
EVENT_TIMER(evt_wait_loghdrs_lock_dig);

// digestion timers
EVENT_TIMER(evt_hdr);
EVENT_TIMER(evt_coal);
EVENT_TIMER(evt_rlh);
EVENT_TIMER(evt_dro);
EVENT_TIMER(evt_dlfrl);
EVENT_TIMER(evt_dc);
EVENT_TIMER(evt_icache);
EVENT_TIMER(evt_cre);
EVENT_TIMER(evt_geb);
EVENT_TIMER(evt_ntype_i);
EVENT_TIMER(evt_ntype_f);
EVENT_TIMER(evt_dig_fcon);
EVENT_TIMER(evt_dig_fcon_in);
EVENT_TIMER(evt_dig_wait_peer);
EVENT_TIMER(evt_ntype_u);
EVENT_TIMER(evt_wait_rdma_memcpy);
EVENT_TIMER(evt_raw_memcpy);

// MP (Multi-process) timers
EVENT_TIMER(evt_dig_req_total);
EVENT_TIMER(evt_dig_req_per_task);
#endif
