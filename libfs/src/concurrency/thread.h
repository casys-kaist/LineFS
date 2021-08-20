#ifndef _THREAD_H_
#define _THREAD_H_

struct logheader_meta *get_loghdr_meta(void);

unsigned long mlfs_create_thread(void *(*entry_point)(void *), 
		volatile int*);
#endif
