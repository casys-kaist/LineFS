#ifndef _PEER_MLFS_H_
#define _PEER_MLFS_H_

#ifdef DISTRIBUTED

#include "replication.h"

// peer type
enum peer_type {
	UNDEF_PEER = 0, //undefined peer - bootstrapping
	LIBFS_PEER,
	KERNFS_PEER,
        KERNFS_NIC_PEER
};

// role played by peer (only applicable to KERNFS_PEER)
enum peer_role {
	HOT_REPLICA = 1,	// Hot replicas can perform read/mutation fs operations
	HOT_BACKUP,		// Hot backups are passive
	COLD_BACKUP,		// Cold backups differ in that they can (optionally) cache cold data
	LOCAL_NODE, 		// Namespace for local replica group
	EXTERNAL_NODE		// Namespace for external replica group
};

//type or purpose of connection
enum sock_type {
	SOCK_IO = 0,	//IO operations
	SOCK_BG,	//Background Events
	SOCK_LS,	//Lease protocol
	SOCK_LH,	//Sending log headers
	SOCK_TYPE_COUNT
};

//registered memory region identifiers
enum mr_type {
	MR_NVM_LOG,	//NVM log
	MR_NVM_SHARED,  //NVM shared area
	MR_DRAM_BUFFER, //DRAM buffer (slab)
	MR_DRAM_CACHE,  //DRAM read cache
        MR_END          //for count. Not a type.
};

enum rpc_pending_type {
	RPC_PENDING_WC = 0,	//Pending Work Completion
	RPC_PENDING_RESPONSE	//Pending RPC Response
};

struct peer_id {
	int id;
	char ip[NI_MAXHOST];
	int pid;
	int type;
	//char port[NI_MAXSERV];
	int role;
	int sockfd[SOCK_TYPE_COUNT]; //socket descriptors
	int sockcount;
	struct log_superblock *log_sb;
	char namespace_id[DIRSIZ];
	uint32_t inum_prefix;
};

struct peer_socket {
	int fd;
	int type;
	uint64_t seqn;
	struct peer_id *peer;
	pthread_spinlock_t seqn_lock;
	uint64_t per_libfs_seqns[MAX_LIBFS_PROCESSES + g_n_nodes]; // To match index of g_sync_ctx.
};

/**
 * @brief Global fetch seqn. Used by LibFS.
 */
struct fetch_seqn {
	uint64_t n;
	pthread_spinlock_t fetch_seqn_lock;
};

struct rpc_pending_io {
	int sockfd;
	int type;
	uint64_t seq_n;
	struct list_head l;
};



#ifndef KERNFS
#define peer_bitmap_size (g_n_nodes)
#define sock_bitmap_size peer_bitmap_size * SOCK_TYPE_COUNT
#else
#define peer_bitmap_size (MAX_LIBFS_PROCESSES + g_n_nodes)
#define sock_bitmap_size peer_bitmap_size * SOCK_TYPE_COUNT
#endif

extern int g_peer_count;
extern int g_sock_count;

extern struct peer_id *g_kernfs_peers[g_n_nodes];
extern struct peer_id *g_peers[peer_bitmap_size];
extern struct peer_socket *g_rpc_socks[sock_bitmap_size];
extern struct fetch_seqn g_fetch_seqn;

void peer_init();
void lock_peer_access();
void unlock_peer_access();
void add_peer_socket(int sockfd, int sock_type);
void remove_peer_socket(int sockfd, int low_lat);
struct peer_id * clone_peer(struct peer_id *input);
int local_kernfs_id(int id);
int get_next_peer_id(int id);
struct peer_id * find_peer(int sockfd);
struct peer_id * _find_peer(char* ip, uint32_t pid);
void register_peer(struct peer_id *peer, int find_id);
void register_peer_log(struct peer_id *peer, int find_id);
void unregister_peer_log(struct peer_id *peer);
void set_peer_id(struct peer_id *peer);
void clear_peer_id(struct peer_id *peer);
void print_g_peers(void);
void print_g_kernfs_peers(void);
bool is_kernfs_on_nic (int id);
int nic_kid_to_host_kid(int nic_kernfs_id);
int host_kid_to_nic_kid (int host_kernfs_id);
bool cur_kernfs_is_last(int starting_kernfs, int target_kernfs);
bool next_kernfs_is_last(int libfs_id);
int get_next_kernfs(int cur_kernfs_id);
int get_prev_kernfs(int cur_kernfs_id);
int get_last_node_id(void);
int nodeid_to_nic_kid (int node_id);
int first_kernfs_id(int libfs_id);
bool is_first_kernfs (int libfs_id, int current_nic_kernfs_id);
bool is_last_kernfs (int libfs_id, int current_nic_kernfs_id);
bool is_middle_in_rep_chain(int libfs_id, int current_nic_kernfs_id);
bool is_local_kernfs(int libfs_id, int current_nic_kernfs_id);
bool is_last_host_kernfs(int libfs_id, int cur_host_kernfs_id);
#endif /* DISTRIBUTED */
#endif
