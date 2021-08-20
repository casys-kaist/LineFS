// for dummy/wrapper implementation
// TODO: call this wrapper api to libc or kernel api
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

//For stack backtrace in panic
#include <execinfo.h>

#include "mlfs/mlfs_user.h"
#include "mlfs/mlfs_interface.h"

#include "global/global.h"
#include "global/defs.h"
#include "util.h"

void pipeclose(struct pipe *p, int writable)
{
	return;
}

int pipewrite(struct pipe *p, char *addr, int n)
{
	return -1;
}

int piperead(struct pipe *p, char *addr, int n)
{
	return -1;
}

void _panic()
{
#if 0
	void *array[20];
	char **bt_syms;
	size_t bt_size, i;

	// Print stack infomation
	// get void*'s for all entries on the stack
	bt_size = backtrace(array, 20);
	bt_syms = backtrace_symbols(array, bt_size);

	printf("%d\n", bt_size);

	for (i = 0; i < bt_size; i++) {
		//size_t len = strlen(bt_syms[i]);
		fprintf(stderr, "%s\n", bt_syms[i]);
	}
#endif

	fflush(stdout);
	fflush(stderr);

	exit(-1);
}

// String utils
/////////////////////////////////////////////////////////////////////

int max(int a, int b) { return (a > b)? a: b; }

//BMS search (code from geeksforgeeks.org).
#define NO_OF_CHARS 256
// The preprocessing function for Boyer Moore's bad character heuristic
void bad_char_heuristic(char *str, int size, int badchar[NO_OF_CHARS])
{
	int i;

	// Initialize all occurrences as -1
	for (i = 0; i < NO_OF_CHARS; i++)
		badchar[i] = -1;

	// Fill the actual value of last occurrence of a character
	for (i = 0; i < size; i++)
		badchar[(int) str[i]] = i;
}

/* A pattern searching function that uses Bad Character Heuristic of
   Boyer Moore Algorithm.
   return: the index of the first occurrence.
*/
int bms_search(char *txt, char *pat)
{
	int m = strlen(pat);
	int n = strlen(txt);

	int badchar[NO_OF_CHARS];

	/* Fill the bad character array by calling the preprocessing
	   function badCharHeuristic() for given pattern */
	bad_char_heuristic(pat, m, badchar);

	int s = 0;  // s is shift of the pattern with respect to text
	while(s <= (n - m)) {
		int j = m-1;

		/* Keep reducing index j of pattern while characters of
		   pattern and text are matching at this shift s */
		while(j >= 0 && pat[j] == txt[s+j])
			j--;

		/* If the pattern is present at current shift, then index j
		   will become -1 after the above loop */
		if (j < 0) {
			//printf("\n pattern occurs at shift = %d", s);
			return s;

			/* Shift the pattern so that the next character in text
			   aligns with the last occurrence of it in pattern.
			   The condition s+m < n is necessary for the case when
			   pattern occurs at the end of text */
			s += (s+m < n)? m-badchar[(int) txt[s+m]] : 1;
		} else
			/* Shift the pattern so that the bad character in text
			   aligns with the last occurrence of it in pattern. The
			   max function is used to make sure that we get a positive
			   shift. We may get a negative shift if the last occurrence
			   of bad character in pattern is on the right side of the
			   current character. */
			s += max(1, j - badchar[(int) txt[s+j]]);
	}

	return -1;
}

void hexdump(void *mem, unsigned int len)
{
	unsigned int i, j;

	/*  print column numbers */
	printf("          ");
	for(i = 0; i < HEXDUMP_COLS; i++) {
		if(i < len)
			printf("%02x ", i);
		else
			printf("	");
	}

	printf("\n");

	for(i = 0; i < len + ((len % HEXDUMP_COLS) ?
				(HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++) {
		/* print offset */
		if(i % HEXDUMP_COLS == 0) {
			printf("0x%06x: ", i);
		}

		/* print hex data */
		if(i < len) {
			printf("%02x ", 0xFF & ((char*)mem)[i]);
		} else {/* end of block, just aligning for ASCII dump */
			printf("	");
		}

		/* print ASCII dump */
		if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1)) {
			for(j = i - (HEXDUMP_COLS - 1); j <= i; j++) {
				if(j >= len) { /* end of block, not really printing */
					printf(" ");
				} else if(isprint(((char*)mem)[j])) { /* printable char */
					printf("%c",(0xFF & ((char*)mem)[j]));
				} else {/* other char */
					printf(".");
				}
			}
			printf("\n");
		}
	}
}

int is_power_of_two(unsigned long x)
{
	return ((x != 0) && ((x & (~x + 1)) == x));
}

unsigned int get_rand_interval(unsigned int min, unsigned int max)
{
	int r;
	const unsigned int range = 1 + max - min;
	const unsigned int buckets = RAND_MAX / range;
	const unsigned int limit = buckets * range;

	/* Create equal size buckets all in a row, then fire randomly towards
	 * the buckets until you land in one of them. All buckets are equally
	 * likely. If you land off the end of the line of buckets, try again. */
	do
	{
		r = rand();
	} while (r >= limit);

	return min + (r / buckets);
}

float get_cpu_clock_speed(void)
{
	FILE* fp;
	char buffer[1024], dummy[64];
	size_t bytes_read;
	char* match;
	float clock_speed;

	/* Read the entire contents of /proc/cpuinfo into the buffer.  */
	fp = fopen ("/proc/cpuinfo", "r");
	bytes_read = fread (buffer, 1, sizeof (buffer), fp);
	fclose (fp);

	/* Bail if read failed or if buffer isn't big enough.  */
	if (bytes_read == 0)
		return 0;

	/* NUL-terminate the text.  */
	buffer[bytes_read] = '\0';

	/* Locate the line that starts with "cpu MHz".  */
	match = strstr(buffer, "cpu MHz");
	if (match == NULL) 
		return 0;

	match = strstr(match, ":");

	/* Parse the line to extrace the clock speed.  */
	sscanf (match, ": %f", &clock_speed);
	return clock_speed;
}

int sockaddr_cmp(struct sockaddr *x, struct sockaddr *y)
{
#define CMP_ST(a, b) if (a != b) return a < b ? -1 : 1

	CMP_ST(x->sa_family, y->sa_family);

	if (x->sa_family == AF_UNIX) {
		struct sockaddr_un *xun = (void*)x, *yun = (void*)y;
		int r = strcmp(xun->sun_path, yun->sun_path);
		if (r != 0)
		    return r;
	} else if (x->sa_family == AF_INET) {
		struct sockaddr_in *xin = (void*)x, *yin = (void*)y;
		CMP_ST(ntohl(xin->sin_addr.s_addr), ntohl(yin->sin_addr.s_addr));
		CMP_ST(ntohs(xin->sin_port), ntohs(yin->sin_port));
	} else if (x->sa_family == AF_INET6) {
		struct sockaddr_in6 *xin6 = (void*)x, *yin6 = (void*)y;
		int r = memcmp(xin6->sin6_addr.s6_addr, yin6->sin6_addr.s6_addr, sizeof(xin6->sin6_addr.s6_addr));
		if (r != 0)
		    return r;
		CMP_ST(ntohs(xin6->sin6_port), ntohs(yin6->sin6_port));
		CMP_ST(xin6->sin6_flowinfo, yin6->sin6_flowinfo);
		CMP_ST(xin6->sin6_scope_id, yin6->sin6_scope_id);
	} else {
		assert(!"unknown sa_family");
	}

#undef CMP_ST
	return 0;
}

int get_cpuid() {

    unsigned cpu;
    if (syscall(__NR_getcpu, &cpu, NULL, NULL) < 0) {
        return -1;
    } else {
        return (int) cpu;
    }
}

int fetch_intf_ip(char* intf, char* host)
{
	struct ifaddrs *ifaddr, *ifa;
	int family, s;
	socklen_t addr_len;

	if (getifaddrs(&ifaddr) == -1) 
		panic("getifaddrs\n");

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) 
	{
		if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
			continue;  

		s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

		//printf("ifa->ifa_name: %s host %s family %d\n",
		//		ifa->ifa_name, host, ifa->ifa_addr->sa_family);

		if((strcmp(ifa->ifa_name,intf)==0)&&(ifa->ifa_addr->sa_family==AF_INET))
		{
			if (s != 0) {
				printf("error returned: %s\n", gai_strerror(s));
				panic("getnameinfo() failed\n");
			}
			else {
				freeifaddrs(ifaddr);
				return 0;
			}
		}
	}

	freeifaddrs(ifaddr);
	return 0;
}
