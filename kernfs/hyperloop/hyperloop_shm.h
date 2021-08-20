#ifndef _HYPERLOOP_SHM_H_
#define _HYPERLOOP_SHM_H_
#include <sys/types.h>

void* create_hyperloop_server_shm(size_t size);
void destroy_hyperloop_server_shm(void *addr, size_t size);

#endif
