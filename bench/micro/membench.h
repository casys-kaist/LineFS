#ifndef _MEMBENCH_H_
#define _MEMBENCH_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#include <iostream>
#include <string>
#include <vector>

#include "global/util.h"

typedef uint64_t        addr_t;
typedef uint64_t        offset_t;


#define ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))

#endif
