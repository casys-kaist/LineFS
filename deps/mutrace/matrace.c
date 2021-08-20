/*-*- Mode: C; c-basic-offset: 8 -*-*/

/***
  This file is part of mutrace.

  Copyright 2009 Lennart Poettering

  mutrace is free software: you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  mutrace is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with mutrace. If not, see <http://www.gnu.org/licenses/>.
***/

#include "config.h"

#include <pthread.h>
#include <execinfo.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sched.h>
#include <malloc.h>

#if !defined (__linux__) || !defined(__GLIBC__)
#error "This stuff only works on Linux!"
#endif

#ifndef SCHED_RESET_ON_FORK
/* "Your libc lacks the definition of SCHED_RESET_ON_FORK. We'll now
 * define it ourselves, however make sure your kernel is new
 * enough! */
#define SCHED_RESET_ON_FORK 0x40000000
#endif

#if defined(__i386__) || defined(__x86_64__)
#define DEBUG_TRAP __asm__("int $3")
#else
#include <signal.h>
#define DEBUG_TRAP raise(SIGTRAP)
#endif

#define LIKELY(x) (__builtin_expect(!!(x),1))
#define UNLIKELY(x) (__builtin_expect(!!(x),0))

static unsigned frames_max = 16;

static volatile unsigned n_allocations_rt = 0;
static volatile unsigned n_frees_rt = 0;
static volatile unsigned n_allocations_non_rt = 0;
static volatile unsigned n_frees_non_rt = 0;

static void* (*real_malloc)(size_t s) = NULL;
static void* (*real_calloc)(size_t n, size_t s) = NULL;
static void* (*real_realloc)(void *p, size_t s) = NULL;
static void (*real_free)(void *p) = NULL;
static void (*real_cfree)(void *p) = NULL;
static void* (*real_memalign)(size_t a, size_t s) = NULL;
static int (*real_posix_memalign)(void **p, size_t a, size_t s) = NULL;
static void* (*real_valloc)(size_t s) = NULL;
static void (*real_exit)(int status) __attribute__((noreturn)) = NULL;
static void (*real__exit)(int status) __attribute__((noreturn)) = NULL;
static void (*real__Exit)(int status) __attribute__((noreturn)) = NULL;
static int (*real_backtrace)(void **array, int size) = NULL;
static char **(*real_backtrace_symbols)(void *const *array, int size) = NULL;
static void (*real_backtrace_symbols_fd)(void *const *array, int size, int fd) = NULL;

static __thread bool recursive = false;

static volatile bool initialized = false;

static void setup(void) __attribute ((constructor));
static void shutdown(void) __attribute ((destructor));

static pid_t _gettid(void) {
        return (pid_t) syscall(SYS_gettid);
}

static const char *get_prname(void) {
        static char prname[17];
        int r;

        r = prctl(PR_GET_NAME, prname);
        assert(r == 0);

        prname[16] = 0;

        return prname;
}

static int parse_env(const char *n, unsigned *t) {
        const char *e;
        char *x = NULL;
        unsigned long ul;

        if (!(e = getenv(n)))
                return 0;

        errno = 0;
        ul = strtoul(e, &x, 0);
        if (!x || *x || errno != 0)
                return -1;

        *t = (unsigned) ul;

        if ((unsigned long) *t != ul)
                return -1;

        return 0;
}

#define LOAD_FUNC(name)                                                 \
        do {                                                            \
                *(void**) (&real_##name) = dlsym(RTLD_NEXT, #name);     \
                assert(real_##name);                                    \
        } while (false)

static void load_functions(void) {
        static volatile bool loaded = false;

        if (LIKELY(loaded))
                return;

        recursive = true;

        LOAD_FUNC(malloc);
        LOAD_FUNC(calloc);
        LOAD_FUNC(realloc);
        LOAD_FUNC(free);
        LOAD_FUNC(cfree);
        LOAD_FUNC(memalign);
        LOAD_FUNC(posix_memalign);
        LOAD_FUNC(valloc);

        LOAD_FUNC(exit);
        LOAD_FUNC(_exit);
        LOAD_FUNC(_Exit);

        LOAD_FUNC(backtrace);
        LOAD_FUNC(backtrace_symbols);
        LOAD_FUNC(backtrace_symbols_fd);

        loaded = true;
        recursive = false;
}

static void setup(void) {
        unsigned t;

        load_functions();

        if (LIKELY(initialized))
                return;

        if (!dlsym(NULL, "main"))
                fprintf(stderr,
                        "mutrace: Application appears to be compiled without -rdynamic. It might be a\n"
                        "mutrace: good idea to recompile with -rdynamic enabled since this produces more\n"
                        "mutrace: useful stack traces.\n\n");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        if (__malloc_hook) {
#pragma GCC diagnostic pop
                fprintf(stderr,
                        "matrace: Detected non-glibc memory allocator. Your program uses some\n"
                        "matrace: alternative memory allocator (jemalloc?) which is not compatible with\n"
                        "matrace: matrace. Please rebuild your program with the standard memory\n"
                        "matrace: allocator or fix matrace to handle yours correctly.\n");

                real_exit(1);
        }

        t = frames_max;
        if (parse_env("MATRACE_FRAMES", &t) < 0 || t <= 0)
                fprintf(stderr, "matrace: WARNING: Failed to parse $MATRACE_FRAMES.\n");
        else
                frames_max = t;

        initialized = true;

        fprintf(stderr, "matrace: "PACKAGE_VERSION" successfully initialized for process %s (PID: %lu).\n",
                get_prname(), (unsigned long) getpid());
}

static void show_summary(void) {
        static pthread_mutex_t summary_mutex = PTHREAD_MUTEX_INITIALIZER;
        static bool shown_summary = false;

        pthread_mutex_lock(&summary_mutex);

        if (shown_summary)
                goto finish;

        fprintf(stderr,
                "\n"
                "matrace: Total of %u allocations and %u frees in non-realtime threads in process %s (PID: %lu).\n"
                "matrace: Total of %u allocations and %u frees in realtime threads.\n",
                n_allocations_non_rt,
                n_frees_non_rt,
                get_prname(), (unsigned long) getpid(),
                n_allocations_rt,
                n_frees_rt);

finish:
        shown_summary = true;

        pthread_mutex_unlock(&summary_mutex);
}

static void shutdown(void) {
        show_summary();
}

void exit(int status) {
        show_summary();
        real_exit(status);
}

void _exit(int status) {
        show_summary();
        real_exit(status);
}

void _Exit(int status) {
        show_summary();
        real__Exit(status);
}

static bool is_realtime(void) {
        int policy;

        policy = sched_getscheduler(_gettid());
        assert(policy >= 0);

        policy &= ~SCHED_RESET_ON_FORK;

        return
                policy == SCHED_FIFO ||
                policy == SCHED_RR;
}

static bool verify_frame(const char *s) {

        /* Generated by glibc's native backtrace_symbols() on Fedora */
        if (strstr(s, "/" SONAME "("))
                return false;

        /* Generated by glibc's native backtrace_symbols() on Debian */
        if (strstr(s, "/" SONAME " ["))
                return false;

        /* Generated by backtrace-symbols.c */
        if (strstr(s, __FILE__":"))
                return false;

        return true;
}

static char* generate_stacktrace(void) {
        void **buffer;
        char **strings, *ret, *p;
        int n, i;
        size_t k;
        bool b;

        buffer = malloc(sizeof(void*) * frames_max);
        assert(buffer);

        n = real_backtrace(buffer, frames_max);
        assert(n >= 0);

        strings = real_backtrace_symbols(buffer, n);
        assert(strings);

        free(buffer);

        k = 0;
        for (i = 0; i < n; i++)
                k += strlen(strings[i]) + 2;

        ret = real_malloc(k + 1);
        assert(ret);

        b = false;
        for (i = 0, p = ret; i < n; i++) {
                if (!b && !verify_frame(strings[i]))
                        continue;

                if (!b && i > 0) {
                        /* Skip all but the first stack frame of ours */
                        *(p++) = '\t';
                        strcpy(p, strings[i-1]);
                        p += strlen(strings[i-1]);
                        *(p++) = '\n';
                }

                b = true;

                *(p++) = '\t';
                strcpy(p, strings[i]);
                p += strlen(strings[i]);
                *(p++) = '\n';
        }

        *p = 0;

        real_free(strings);

        return ret;
}

static void print_backtrace(void) {
        char *bt;

        if (UNLIKELY(recursive))
                return;

        recursive = true;

        bt = generate_stacktrace();

        fprintf(stderr,
                "\n"
                "matrace: Memory allocator operation in realtime thread %lu:\n"
                "%s", (unsigned long) _gettid(), bt);
        real_free(bt);

        recursive = false;
}

static void check_allocation(void) {

        if (is_realtime()) {
                __sync_fetch_and_add(&n_allocations_rt, 1);
                print_backtrace();
        } else
                __sync_fetch_and_add(&n_allocations_non_rt, 1);
}

static void check_free(void) {

        if (is_realtime()) {
                __sync_fetch_and_add(&n_frees_rt, 1);
                print_backtrace();
        } else
                __sync_fetch_and_add(&n_frees_non_rt, 1);
}

void *malloc(size_t s) {

        /* In dlsym() glibc might actually call malloc() itself which
         * could make us enter an endless loop, when we try to call it
         * from inside the malloc(). However, glibc gracefully handles
         * malloc() returning NULL in this case. We use this to escape
         * this endless loop. */

        if (UNLIKELY(!initialized && recursive)) {
                errno = ENOMEM;
                return NULL;
        }

        load_functions();
        check_allocation();

        return real_malloc(s);
}

void *calloc(size_t n, size_t s) {

        if (UNLIKELY(!initialized && recursive)) {
                errno = ENOMEM;
                return NULL;
        }

        load_functions();
        check_allocation();

        return real_calloc(n, s);
}

void *realloc(void *p, size_t s) {

        if (UNLIKELY(!initialized && recursive)) {
                errno = ENOMEM;
                return NULL;
        }

        load_functions();
        check_allocation();

        return real_realloc(p, s);
}

void free(void *p) {

        load_functions();
        check_free();

        real_free(p);
}

void cfree(void *p) {

        load_functions();
        check_free();

        real_cfree(p);
}

void *memalign(size_t a, size_t s) {

        if (UNLIKELY(!initialized && recursive)) {
                errno = ENOMEM;
                return NULL;
        }

        load_functions();
        check_allocation();

        return real_memalign(a, s);
}

int posix_memalign(void **p, size_t a, size_t s) {

        if (UNLIKELY(!initialized && recursive))
                return ENOMEM;

        load_functions();
        check_allocation();

        return real_posix_memalign(p, a, s);
}

void *valloc(size_t s) {

        if (UNLIKELY(!initialized && recursive)) {
                errno = ENOMEM;
                return NULL;
        }

        load_functions();
        check_allocation();

        return real_valloc(s);
}

int backtrace(void **array, int size) {
        int r;

        load_functions();

        recursive = true;
        r = real_backtrace(array, size);
        recursive = false;

        return r;
}

char **backtrace_symbols(void *const *array, int size) {
        char **r;

        load_functions();

        /* backtrace_symbols() internally uses malloc(). To avoid an
         * endless loop we need to disable ourselves so that we don't
         * try to call backtrace() ourselves when looking at that
         * malloc(). */

        recursive = true;
        r = real_backtrace_symbols(array, size);
        recursive = false;

        return r;
}

void backtrace_symbols_fd(void *const *array, int size, int fd) {
        load_functions();

        recursive = true;
        real_backtrace_symbols_fd(array, size, fd);
        recursive = false;
}
