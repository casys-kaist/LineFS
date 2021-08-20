//#include <linux/export.h>
//#include <linux/bitops.h>
//#include "../../../bitops.h"
#include "../bitops.h"
#include <asm/types.h>

/**
 * hweightN - returns the hamming weight of a N-bit word
 * @x: the word to weigh
 *
 * The Hamming Weight of a number is the total number of bits set in it.
 */


// ARM64 specific macro settings.
#undef __HAVE_ARCH_SW_HWEIGHT
#ifndef CONFIG_ARCH_HAS_FAST_MULTIPLIER
#define CONFIG_ARCH_HAS_FAST_MULTIPLIER
#endif
#ifndef BITS_PER_LONG
#define BITS_PER_LONG == 64
#endif

#ifndef __HAVE_ARCH_SW_HWEIGHT
unsigned int __sw_hweight32(unsigned int w);
#endif

unsigned int __sw_hweight16(unsigned int w);

unsigned int __sw_hweight8(unsigned int w);

#ifndef __HAVE_ARCH_SW_HWEIGHT
unsigned long __sw_hweight64(__u64 w);
#endif
