#ifndef _MLFS_INTERFACE_H_
#define _MLFS_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

void init_fs(void);
void shutdown_fs(void);

//logs
void read_log_headers(void);
int mlfs_process_id(void);
unsigned int make_digest_request_sync(int percent);
int make_digest_request_async(int percent);
void wait_on_digesting();
/*
static void install_log_group(struct logheader *loghdr,
		addr_t hdr_blkno);
*/
void replicate_log_by_digestion(void);
void replicate_log_by_fsync(void);

void request_publish_remains(void);
extern unsigned char initialized;

//utils
int bms_search(char *txt, char *pat);

#ifdef __cplusplus
}
#endif

#endif
