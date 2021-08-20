#ifndef _XCHG_X86_H_
#define _XCHG_X86_H_


#if (defined(__i386__) || defined(__x86_64__))

#ifdef __cplusplus
extern "C" {
#endif

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
	__asm__ __volatile__("xchgq %0,%1"
			:"=r" ((unsigned long long) x)
			:"m" (*(volatile long long *)ptr), "0" ((unsigned long long) x)
			:"memory");

	return x;
}

static inline unsigned xchg_32(void *ptr, unsigned x)
{
	__asm__ __volatile__("xchgl %0,%1"
			:"=r" ((unsigned) x)
			:"m" (*(volatile unsigned *)ptr), "0" (x)
			:"memory");

	return x;
}

static inline unsigned short xchg_16(void *ptr, unsigned short x)
{
	__asm__ __volatile__("xchgw %0,%1"
			:"=r" ((unsigned short) x)
			:"m" (*(volatile unsigned short *)ptr), "0" (x)
			:"memory");

	return x;
}

static inline unsigned char xchg_8(void *ptr, unsigned char x)
{
	__asm__ __volatile__("xchgb %0,%1"
			:"=r" ((unsigned char) x)
			:"m" (*(volatile unsigned char*)ptr), "0" (x)
			:"memory");

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

#endif /* (defined(__i386__) || defined(__x86_64__)) */

#endif /* _XCHG_X86_H_ */
