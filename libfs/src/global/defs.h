#ifndef _DEFS_H_
#define _DEFS_H_

#include <stdio.h>
#include <sys/types.h>
#include <sys/syscall.h>

#define get_tid() syscall(__NR_gettid)

#ifdef __cplusplus
extern "C" {
#endif

// printf colorset.
#ifdef PRINT_COLOR
#define ANSI_COLOR_RED			"\x1b[0;31m"
#define ANSI_COLOR_RED_BOLD		"\x1b[1;31m"
#define ANSI_COLOR_GREEN		"\x1b[0;32m"
#define ANSI_COLOR_GREEN_BOLD		"\x1b[1;32m"
#define ANSI_COLOR_YELLOW		"\x1b[0;33m"
#define ANSI_COLOR_YELLOW_BOLD  	"\x1b[1;33m"
#define ANSI_COLOR_BLUE			"\x1b[0;34m"
#define ANSI_COLOR_BLUE_BOLD    	"\x1b[1;34m"
#define ANSI_COLOR_MAGENTA		"\x1b[0;35m"
#define ANSI_COLOR_MAGENTA_BOLD 	"\x1b[1;35m"
#define ANSI_COLOR_CYAN			"\x1b[0;36m"
#define ANSI_COLOR_CYAN_BOLD    	"\x1b[1;36m"
#define ANSI_COLOR_BRIGHT_RED		"\x1b[0;91m"
#define ANSI_COLOR_BRIGHT_RED_BOLD	"\x1b[1;91m"
#define ANSI_COLOR_BRIGHT_GREEN		"\x1b[0;92m"
#define ANSI_COLOR_BRIGHT_GREEN_BOLD	"\x1b[1;92m"
#define ANSI_COLOR_BRIGHT_YELLOW	"\x1b[0;93m"
#define ANSI_COLOR_BRIGHT_YELLOW_BOLD	"\x1b[1;93m"
#define ANSI_COLOR_BRIGHT_BLUE		"\x1b[0;94m"
#define ANSI_COLOR_BRIGHT_BLUE_BOLD	"\x1b[1;94m"
#define ANSI_COLOR_BRIGHT_MAGENTA	"\x1b[0;95m"
#define ANSI_COLOR_BRIGHT_MAGENTA_BOLD	"\x1b[1;95m"
#define ANSI_COLOR_BRIGHT_CYAN		"\x1b[0;96m"
#define ANSI_COLOR_BRIGHT_CYAN_BOLD	"\x1b[1;96m"
#define ANSI_COLOR_RESET		"\x1b[0m"
#else
#define ANSI_COLOR_RED			""
#define ANSI_COLOR_RED_BOLD		""
#define ANSI_COLOR_GREEN		""
#define ANSI_COLOR_GREEN_BOLD   	""
#define ANSI_COLOR_YELLOW		""
#define ANSI_COLOR_YELLOW_BOLD  	""
#define ANSI_COLOR_BLUE			""
#define ANSI_COLOR_BLUE_BOLD    	""
#define ANSI_COLOR_MAGENTA		""
#define ANSI_COLOR_MAGENTA_BOLD 	""
#define ANSI_COLOR_CYAN			""
#define ANSI_COLOR_CYAN_BOLD    	""
#define ANSI_COLOR_BRIGHT_RED		""
#define ANSI_COLOR_BRIGHT_RED_BOLD	""
#define ANSI_COLOR_BRIGHT_GREEN		""
#define ANSI_COLOR_BRIGHT_GREEN_BOLD	""
#define ANSI_COLOR_BRIGHT_YELLOW	""
#define ANSI_COLOR_BRIGHT_YELLOW_BOLD	""
#define ANSI_COLOR_BRIGHT_BLUE		""
#define ANSI_COLOR_BRIGHT_BLUE_BOLD	""
#define ANSI_COLOR_BRIGHT_MAGENTA	""
#define ANSI_COLOR_BRIGHT_MAGENTA_BOLD	""
#define ANSI_COLOR_BRIGHT_CYAN		""
#define ANSI_COLOR_BRIGHT_CYAN_BOLD	""
#define ANSI_COLOR_RESET		""
#endif

#ifdef KERNFS
#define LOG_PATH "/tmp/mlfs_log/kernfs.txt"
#elif LIBFS
#define LOG_PATH "/tmp/mlfs_log/libfs.txt"
#endif
extern int log_fd;

#define MLFS_PRINTF

#ifdef MLFS_LOG
#define mlfs_log(fmt, ...) \
	do { \
		mlfs_time_t t; \
		gettimeofday(&t, NULL); \
		dprintf(log_fd, "[%ld.%03ld][%s():%d] " fmt,  \
				t.tv_sec, t.tv_usec, __func__, \
				__LINE__, __VA_ARGS__); \
		fsync(log_fd); \
	} while (0)
#else
#define mlfs_log(...)
#endif

#define mlfs_muffled(...)
// #define mlfs_muffled(fmt, ...) \
//         do { \
//                 fprintf(stdout, "[tid:%lu][%s():%d] " fmt,  \
//                                 get_tid(), __func__, __LINE__, __VA_ARGS__); \
//         } while (0)

#ifdef MLFS_DEBUG
#define MLFS_INFO
#define mlfs_debug(fmt, ...) \
	do { \
		fprintf(stdout, "[tid:%lu][%s():%d] " fmt,  \
				get_tid(), __func__, __LINE__, __VA_ARGS__); \
	} while (0)
#else
#define mlfs_debug(...)
#endif

#ifdef MLFS_INFO
#define mlfs_info(fmt, ...) \
	do { \
		fprintf(stdout, "[tid:%lu][%s():%d] " fmt,  \
				get_tid(), __func__, __LINE__, __VA_ARGS__); \
	} while (0)
#else
#define mlfs_info(...)
#endif

#ifdef MLFS_PRINTF
//#define mlfs_printf(fmt, ...) \
//	do { \
//		fprintf(stdout, "[%s():%d] " fmt,  \
//				__func__, __LINE__, __VA_ARGS__); \
//	} while (0)
#define mlfs_printf(fmt, ...) \
	do { \
		fprintf(stdout, "%lu %s():%d " fmt,  \
				get_tid(), __func__, __LINE__, __VA_ARGS__); \
	} while (0)
#else
#define mlfs_printf(...)
#endif

//void _panic(void) __attribute__((noreturn));
void _panic(void);

#define panic(str) \
	do { \
		fprintf(stdout, "%s:%d %s(): %s\n",  \
				__FILE__, __LINE__, __func__, str); \
		fflush(stdout);	\
		GDB_TRAP; \
		_panic(); \
	} while (0)

#define stringize(s) #s
#define XSTR(s) stringize(s)

#if defined MLFS_DEBUG
void abort (void);
# if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
# define mlfs_assert(a) \
	do { \
		if (0 == (a)) { \
			fprintf(stderr, \
					"Assertion failed: %s, " \
					"%s(), %d at \'%s\'\n", \
					__FILE__, \
					__func__, \
					__LINE__, \
					XSTR(a)); \
			GDB_TRAP; \
			abort(); \
		} \
	} while (0)
# else
# define mlfs_assert(a) \
	do { \
		if (0 == (a)) { \
			fprintf(stderr, \
					"Assertion failed: %s, " \
					"%d at \'%s\'\n", \
					__FILE__, \
					__LINE__, \
					XSTR(a)); \
			GDB_TRAP; \
			abort(); \
		} \
	} while (0)
# endif
#else
#if 0
# define mlfs_assert(a) assert(a)
#else // turn on assert in release build.
# define mlfs_assert(a) \
	do { \
		if (0 == (a)) { \
			fprintf(stderr, \
					"Assertion failed: %s, " \
					"%s(), %d at \'%s\'\n", \
					__FILE__, \
					__func__, \
					__LINE__, \
					XSTR(a)); \
			GDB_TRAP; \
			abort(); \
		} \
	} while (0)
#endif
#endif

/* Print for each features. */
#ifdef PRINT_REPLICATE
#define pr_rep(fmt, ...) \
    do { \
        fprintf(stdout, "%lu %s():%d " ANSI_COLOR_GREEN fmt ANSI_COLOR_RESET "\n",  \
                get_tid(), __func__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define pr_rep(...)
#endif

#ifdef PRINT_LOG_PREFETCH
#define pr_lpref(fmt, ...) \
    do { \
        fprintf(stdout, "%lu %s():%d " ANSI_COLOR_GREEN_BOLD fmt ANSI_COLOR_RESET "\n",  \
                get_tid(), __func__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define pr_lpref(...)
#endif

#ifdef PRINT_LOGHDRS
#define pr_loghdrs(fmt, ...) \
    do { \
        fprintf(stdout, "%lu %s():%d " ANSI_COLOR_BRIGHT_GREEN fmt ANSI_COLOR_RESET "\n",  \
                get_tid(), __func__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define pr_loghdrs(...)
#endif

#ifdef PRINT_DIGEST
#define pr_digest(fmt, ...) \
    do { \
        fprintf(stdout, "%lu %s():%d " ANSI_COLOR_YELLOW fmt ANSI_COLOR_RESET "\n",  \
                get_tid(), __func__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define pr_digest(...)
#endif

#ifdef PRINT_RPC
#define pr_rpc(fmt, ...) \
    do { \
        fprintf(stdout, "%lu %s():%d " ANSI_COLOR_BLUE fmt ANSI_COLOR_RESET "\n",  \
                get_tid(), __func__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define pr_rpc(...)
#endif

#ifdef PRINT_REP_COALESCE
#define pr_rcoal(fmt, ...) \
    do { \
        fprintf(stdout, "%lu %s():%d " ANSI_COLOR_CYAN fmt ANSI_COLOR_RESET "\n",  \
                get_tid(), __func__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define pr_rcoal(...)
#endif

#ifdef PRINT_POSIX
#define pr_posix(fmt, ...) \
    do { \
        fprintf(stdout, "%lu %s():%d " ANSI_COLOR_MAGENTA fmt ANSI_COLOR_RESET "\n",  \
                get_tid(), __func__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define pr_posix(...)
#endif

#ifdef PRINT_SETUP
#define pr_setup(fmt, ...) \
    do { \
        fprintf(stdout, "%lu %s():%d " fmt "\n",  \
                get_tid(), __func__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define pr_setup(...)
#endif

#ifdef PRINT_RDMA
#define pr_rdma(fmt, ...) \
    do { \
        fprintf(stdout, "%lu %s():%d " fmt "\n",  \
                get_tid(), __func__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define pr_rdma(...)
#endif

#ifdef PRINT_META
#define pr_meta(fmt, ...) \
    do { \
        fprintf(stdout, ANSI_COLOR_RED fmt ANSI_COLOR_RESET "\n",  \
                __VA_ARGS__); \
    } while (0)
#else
#define pr_meta(...)
#endif

#ifdef PRINT_DRAM_ALLOC
#define pr_dram_alloc(fmt, ...) \
    do { \
        fprintf(stdout, "%lu %s():%d " fmt "\n",  \
                get_tid(), __func__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define pr_dram_alloc(...)
#endif

#ifdef PRINT_PIPELINE
#define pr_pipe(fmt, ...) \
    do { \
        fprintf(stdout, "%lu " ANSI_COLOR_CYAN fmt ANSI_COLOR_RESET "\n" ,  \
                get_tid(), __VA_ARGS__); \
    } while (0)
#else
#define pr_pipe(...)
#endif

#if (defined(__i386__) || defined(__x86_64__))
#define GDB_TRAP asm("int $3;");
#elif (defined(__aarch64__))
#define GDB_TRAP asm(".inst 0xd4200000"); // FIXME Not working correctly?
// #define GDB_TRAP __builtin_trap();
#else
#error "Not supported architecture."
#endif

#define COMPILER_BARRIER() asm volatile("" ::: "memory")

#define mlfs_is_set(macro) mlfs_is_set_(macro)
#define mlfs_macroeq_1 ,
#define mlfs_is_set_(value) mlfs_is_set__(mlfs_macroeq_##value)
#define mlfs_is_set__(comma) mlfs_is_set___(comma 1, 0)
#define mlfs_is_set___(_, v, ...) v

// util.c
struct pipe;

void pipeclose(struct pipe*, int);
int piperead(struct pipe*, char*, int);
int pipewrite(struct pipe*, char*, int);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#ifdef __cplusplus
}
#endif

#endif

