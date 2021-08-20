#ifndef _HYPERLOOP_CLIENT_H_
#define _HYPERLOOP_CLIENT_H_

void hyperloop_client_init(char *buf, uint64_t buf_size,
                           char *meta_start_addr, int meta_size);
void shutdown_hyperloop_client(void);
void do_hyperloop_gwrite_data(void);
void do_hyperloop_gwrite_meta(void);

#endif
