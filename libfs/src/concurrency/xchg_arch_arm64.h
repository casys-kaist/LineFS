#ifndef _XCHG_ARM64_H_
#define _XCHG_ARM64_H_

#if (defined(__aarch64__))

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Based on arch/arm/include/asm/cmpxchg.h
 *
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//#include <linux/bug.h>

//#include <asm/atomic.h>
//#include <asm/barrier.h>
//#include <asm/lse.h>
#include <assert.h>

/*
 * We need separate acquire parameters for ll/sc and lse, since the full
 * barrier case is generated as release+dmb for the former and
 * acquire+release for the latter.
 */

#if 0
#define __XCHG_CASE(w, sz, name, mb, nop_lse, acq, acq_lse, rel, cl)    \
static inline unsigned long __xchg_case_##name(unsigned long x,         \
                                               volatile void *ptr)      \
{                                                                       \
        unsigned long ret, tmp;                                         \
                                                                        \
        asm volatile(ARM64_LSE_ATOMIC_INSN(                             \
        /* LL/SC */                                                     \
        "       prfm    pstl1strm, %2\n"                                \
        "1:     ld" #acq "xr" #sz "\t%" #w "0, %2\n"                    \
        "       st" #rel "xr" #sz "\t%w1, %" #w "3, %2\n"               \
        "       cbnz    %w1, 1b\n"                                      \
        "       " #mb,                                                  \
        /* LSE atomics */                                               \
        "       nop\n"                                                  \
        "       nop\n"                                                  \
        "       swp" #acq_lse #rel #sz "\t%" #w "3, %" #w "0, %2\n"     \
        "       nop\n"                                                  \
        "       " #nop_lse)                                             \
        : "=&r" (ret), "=&r" (tmp), "+Q" (*(u8 *)ptr)                   \
        : "r" (x)                                                       \
        : cl);                                                          \
                                                                        \
        return ret;                                                     \
}
#endif

#define __XCHG_CASE(w, sz, name, mb, nop_lse, acq, acq_lse, rel, cl)    \
static inline unsigned long __xchg_case_##name(unsigned long x,         \
                                               volatile void *ptr)      \
{                                                                       \
        unsigned long ret, tmp;                                         \
                                                                        \
        asm volatile(                             			\
        /* LL/SC */                                                     \
        "       prfm    pstl1strm, %2\n"                                \
        "1:     ld" #acq "xr" #sz "\t%" #w "0, %2\n"                    \
        "       st" #rel "xr" #sz "\t%w1, %" #w "3, %2\n"               \
        "       cbnz    %w1, 1b\n"                                      \
        "       " #mb                                             	\
        : "=&r" (ret), "=&r" (tmp), "+Q" (*(uint8_t *)ptr)                   \
        : "r" (x)                                                       \
        : cl);                                                          \
                                                                        \
        return ret;                                                     \
}

__XCHG_CASE(w, b,     1,        ,    ,  ,  ,  ,         )
__XCHG_CASE(w, h,     2,        ,    ,  ,  ,  ,         )
__XCHG_CASE(w,  ,     4,        ,    ,  ,  ,  ,         )
__XCHG_CASE( ,  ,     8,        ,    ,  ,  ,  ,         )
__XCHG_CASE(w, b, acq_1,        ,    , a, a,  , "memory")
__XCHG_CASE(w, h, acq_2,        ,    , a, a,  , "memory")
__XCHG_CASE(w,  , acq_4,        ,    , a, a,  , "memory")
__XCHG_CASE( ,  , acq_8,        ,    , a, a,  , "memory")
__XCHG_CASE(w, b, rel_1,        ,    ,  ,  , l, "memory")
__XCHG_CASE(w, h, rel_2,        ,    ,  ,  , l, "memory")
__XCHG_CASE(w,  , rel_4,        ,    ,  ,  , l, "memory")
__XCHG_CASE( ,  , rel_8,        ,    ,  ,  , l, "memory")
__XCHG_CASE(w, b,  mb_1, dmb ish, nop,  , a, l, "memory")
__XCHG_CASE(w, h,  mb_2, dmb ish, nop,  , a, l, "memory")
__XCHG_CASE(w,  ,  mb_4, dmb ish, nop,  , a, l, "memory")
__XCHG_CASE( ,  ,  mb_8, dmb ish, nop,  , a, l, "memory")

#undef __XCHG_CASE

#define __XCHG_GEN(sfx)                                                 \
static inline unsigned long __xchg##sfx(unsigned long x,                \
                                        volatile void *ptr,             \
                                        int size)                       \
{                                                                       \
        switch (size) {                                                 \
        case 1:                                                         \
                return __xchg_case##sfx##_1(x, ptr);                    \
        case 2:                                                         \
                return __xchg_case##sfx##_2(x, ptr);                    \
        case 4:                                                         \
                return __xchg_case##sfx##_4(x, ptr);                    \
        case 8:                                                         \
                return __xchg_case##sfx##_8(x, ptr);                    \
        default:                                                        \
		assert (0);						\
        }                                                               \
}

__XCHG_GEN()
__XCHG_GEN(_acq)
__XCHG_GEN(_rel)
__XCHG_GEN(_mb)

#undef __XCHG_GEN

#define __xchg_wrapper(sfx, ptr, x)                                     \
({                                                                      \
        __typeof__(*(ptr)) __ret;                                       \
        __ret = (__typeof__(*(ptr)))                                    \
                __xchg##sfx((unsigned long)(x), (ptr), sizeof(*(ptr))); \
        __ret;                                                          \
})

/* xchg */
#define xchg_relaxed(...)       __xchg_wrapper(    , __VA_ARGS__)
#define xchg_acquire(...)       __xchg_wrapper(_acq, __VA_ARGS__)
#define xchg_release(...)       __xchg_wrapper(_rel, __VA_ARGS__)
#define xchg(...)               __xchg_wrapper( _mb, __VA_ARGS__)


// Synchronization primitives for libFS
// This part heavily depends on APIs provided by low level APIs
// This implementation uses pthread APIs

//#include "global/global.h"
//#include <unistd.h>
//#include <pthread.h>
//#include <sys/syscall.h>
//#include <sys/types.h>
//#include <linux/futex.h>

/* Atomic exchange (of various sizes) */
static inline void *xchg_64(void *ptr, void *x)
{
/*
	__asm__ __volatile__("xchgq %0,%1"
			:"=r" ((unsigned long long) x)
			:"m" (*(volatile long long *)ptr), "0" ((unsigned long long) x)
			:"memory");
*/

	xchg_relaxed((volatile long long *)ptr, (unsigned long long)x);

	return x;
}

static inline unsigned xchg_32(void *ptr, unsigned x)
{
/*
	__asm__ __volatile__("xchgl %0,%1"
			:"=r" ((unsigned) x)
			:"m" (*(volatile unsigned *)ptr), "0" (x)
			:"memory");
*/

	xchg_relaxed((volatile unsigned *)ptr, (unsigned)x);
	return x;
}

static inline unsigned short xchg_16(void *ptr, unsigned short x)
{
/*
	__asm__ __volatile__("xchgw %0,%1"
			:"=r" ((unsigned short) x)
			:"m" (*(volatile unsigned short *)ptr), "0" (x)
			:"memory");
*/

	xchg_relaxed((volatile unsigned short *)ptr, (unsigned short)x);
	return x;
}

static inline unsigned char xchg_8(void *ptr, unsigned char x)
{
/*
	__asm__ __volatile__("xchgb %0,%1"
			:"=r" ((unsigned char) x)
			:"m" (*(volatile unsigned char*)ptr), "0" (x)
			:"memory");
*/

	xchg_relaxed((volatile unsigned char *)ptr, (unsigned char)x);
	return x;
}

/* Test and set a bit */
/*
static inline char atomic_bitsetandtest(void *ptr, int x)
{
	char out;
	__asm__ __volatile__("lock; bts %2,%1\n"
			"sbb %0,%0\n"
			:"=r" (out), "=m" (*(volatile long long *)ptr)
			:"Ir" (x)
			:"memory");

	return out;
}
*/



#ifdef __cplusplus
}
#endif

#endif /* (defined(__aarch64__) */

#endif /* _XCHG_ARM64_H_ */
