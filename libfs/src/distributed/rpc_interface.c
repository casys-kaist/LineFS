#include "rpc_interface.h"
#include "distributed/peer.h"
#include "distributed/replication.h"
#include "global/mem.h"
#include "storage/storage.h"

#ifdef KERNFS

#ifdef __x86_64__
#include "fs.h"
#elif __aarch64__
#include "nic/nic_fs.h"
#include "nic/limit_rate.h"
#else
#error "Unsupported architecture."
#endif

#else
#include "filesystem/fs.h"
#include "mlfs/mlfs_interface.h"
#endif

#ifdef DISTRIBUTED

#if 0
int g_sockfd_io; // for io & rsync operations
int g_sockfd_bg; // for background operations
int g_sockfd_ls; // for communicating with other master
#endif

char g_self_ip[NI_MAXHOST];
int g_self_id = -1;
int g_kernfs_id = -1;     // Coincide with index of hot_replicas in rpc_interface.h
int g_node_id = -1;       /* Used for replica node (machine) id.
                           * I.e. host kernfs and nic kernfs in the same machine
                           *      have the same g_node_id.
                           */
int g_host_node_id = -1;  // Host kernfs id. Used for NIC-offloading.
int rpc_shutdown = 0;

//struct timeval start_time, end_time;

//uint32_t msg_seq[g_n_nodes*sock_count] = {0};

#ifdef PROFILE_LOG_COPY_BW
rt_bw_stat log_copy_bw_stat = {0};
#endif

#ifdef NIC_OFFLOAD
#define TCP_PORT 3456
/*
 * To setup tcp client.
 * It is called in the client side (=SmartNIC)
 **/
int tcp_setup_client (int *cli_sock_fd)
{
    struct sockaddr_in serv_addr;

    if ((*cli_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            panic("Client: Fail to create TCP socket.\n");
    else
        mlfs_info("Client: socket(%d) created.\n", *cli_sock_fd);

    memset(&serv_addr, 0x00, sizeof(struct sockaddr_in));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(TCP_PORT);

    if (inet_aton((char*)hot_replicas[g_host_node_id].ip,
                &serv_addr.sin_addr) == 0)
        panic ("Client: Fail to get ip address.\n");

    if(connect(*cli_sock_fd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in)) < 0)
        panic ("Client: connect failed.\n");

    mlfs_info("Client: connected to server(%s). socket:%d\n", inet_ntoa(serv_addr.sin_addr), *cli_sock_fd);
    return 1;
}

/*
 * To setup tcp server.
 * It is called in the server side (=Host)
 **/
int tcp_setup_server (int *serv_sock_fd, int *cli_sock_fd)
{
    struct sockaddr_in serv_addr, cli_addr;
    struct in_addr addr;
    int len;

    if ((*serv_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            panic("Server: Fail to create TCP socket.\n");
    else
        mlfs_info("Server: socket(%d) created.\n", *serv_sock_fd);

    // To resolve bind problem.
    int true_val = 1;
    setsockopt(*serv_sock_fd, SOL_SOCKET, SO_REUSEADDR, &true_val, sizeof(int));

    memset(&serv_addr, 0x00, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(TCP_PORT);
    fetch_intf_ip(mlfs_conf.x86_net_interface_name, g_self_ip);
    mlfs_printf ("[NIC_OFFLOAD] g_self_ip: %s\n", g_self_ip);

    if (inet_aton((char*)g_self_ip, &addr) == 0)
        panic ("Server: Fail to get ip address.\n");

    serv_addr.sin_addr = addr;

    if(bind(*serv_sock_fd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in)) < 0)
        panic ("Server: bind failed.\n");

    if (listen(*serv_sock_fd, 5) < 0)
        panic ("Server: listen failed.\n");

    len = sizeof(struct sockaddr_in);
    if ((*cli_sock_fd = accept(*serv_sock_fd, (struct sockaddr*)&cli_addr, &len)) < 0)
        panic ("Server: accept failed.");

    mlfs_info("Server: client (%s) connected. serv_sock:%d cli_sock:%d\n",
            inet_ntoa(cli_addr.sin_addr), *cli_sock_fd, *serv_sock_fd);
    return 1;
}

/**
 * Send TCP response to client.
 * Params
 *      sock_fd : socket fd connected to client.
 *      buf : A buffer storing data that is sent to client.
 *      len : length of data to be sent.
 */
void tcp_send_client_resp (int sock_fd, char *buf, int len) {
    int n;
    // Send response.
    assert(len <= TCP_BUF_SIZE);
    n = write(sock_fd, buf, len);
    mlfs_info("Server: Send resp to client. Tried to send %d bytes. %d bytes have been sent.\n", len, n);
}

/**
 * Send TCP request to server.
 * Params
 *      sock_fd : socket fd connected to server.
 *      cmd : command string to be sent to the server.
 *      data : data received from server. Its size is TCP_BUF_SIZE+1.
 *      len : length of data to be received. If 0, does not receive response from server.
 */
void tcp_send_client_req (int sock_fd, char *cmd, char *data, int len) {
    int n =0, bytes_read=0, ret=0;

    // Send request.
    mlfs_info("Client: Send req to server: %s\n", cmd);
    ret = write(sock_fd, cmd, TCP_HDR_SIZE); // No need to send large size.

    if (ret < 0)
        panic("Failed to send TCP request to server.");

    // Receive data.
    if (len > 0) {
        assert(len <= TCP_BUF_SIZE);
        if (data == NULL)
            panic("data is NULL\n");
        while ((n = read (sock_fd, data, len)) > 0) {
            //mlfs_info ("Requested read size: %d Actual read size: %d\n", len, n);
            bytes_read += n;
            if (bytes_read >= len) {
                mlfs_info("Client: All %d bytes have been read.\n", len);
                break;
            } else {
                data += n;
                mlfs_info("Client: %d bytes read.\n", bytes_read);
            }
        }
    }
}

/**
 * Request ondisk superblock.
 */
void tcp_client_req_sb(int sock_fd, char *data, int dev)
{
    struct disk_superblock *sb;
    char cmd[TCP_HDR_SIZE] = {0};

    sprintf(cmd, "|sb |%d|", dev);
    assert (sizeof(cmd) <= TCP_HDR_SIZE);
    tcp_send_client_req(sock_fd, cmd, data, sizeof(struct disk_superblock));
    sb = (struct disk_superblock*)data;
    mlfs_printf("Client received resp: superblock: dev %d size %lu nblocks %lu ninodes %u nlog %lu\n"
                    "\t[inode start %lu bmap start %lu datablock start %lu log start %lu]\n",
                    dev,
                    sb->size, 
                    sb->ndatablocks, 
                    sb->ninodes,
                    sb->nlog,
                    sb->inode_start, 
                    sb->bmap_start, 
                    sb->datablock_start,
                    sb->log_start);
}

/**
 * Request ondisk root inode.
 */
void tcp_client_req_root_inode(int sock_fd, char *data)
{
    struct dinode *di;
    tcp_send_client_req(sock_fd, "|rinode |", data, sizeof(struct dinode));
    di = (struct dinode*)data;
    mlfs_info("Client received resp: dinode: "
            "\t[ itype %d nlink %d size %lu atime %ld.%06ld ctime %ld.%06ld mtime %ld.%06ld ]\n",
                    di->itype, 
                    di->nlink, 
                    di->size,
                    di->atime.tv_sec, 
                    di->atime.tv_usec, 
                    di->ctime.tv_sec, 
                    di->ctime.tv_usec, 
                    di->mtime.tv_sec, 
                    di->mtime.tv_usec);
}

/**
 * Request read bitmap block.
 * (***)
 * It is assumed that client requests are sent to server in sequential order.
 * When this assumption is broken, then we need to identify the corresponding
 * block number of a response from server. Namely, blk number should be included
 * in the response data.
 */
void tcp_client_req_bitmap_block(int sock_fd, char *data, int dev, mlfs_fsblk_t blk_nr)
{
    char cmd[TCP_HDR_SIZE] = {0};
    sprintf(cmd, "|bitmapb |%d|%lu|", dev, blk_nr);
    assert (sizeof(cmd) <= TCP_HDR_SIZE);
    tcp_send_client_req(sock_fd, cmd, data, g_block_size_bytes);
    mlfs_info("Client received resp: bitmap block [ dev:%d blk_nr:%ld ]\n", dev, blk_nr);
}

/**
 * Request device base address.
 */
void tcp_client_req_dev_base_addr(int sock_fd, char *data, int devid)
{
    char cmd[TCP_HDR_SIZE] = {0};

    sprintf(cmd, "|devba |%d|", devid);
    assert (sizeof(cmd) <= TCP_HDR_SIZE);
    tcp_send_client_req(sock_fd, cmd, data, sizeof(uint8_t *));
    mlfs_info("Client received resp: devba: dev %d base_addr:0x%lx\n", devid, *(addr_t*)data);
}

/**
 * Request server to close this connection.
 */
int tcp_client_req_close (int sock_fd)
{
    tcp_send_client_req(sock_fd, "|close |", NULL, 0);
}

void tcp_close(int sock_fd)
{
    if (close(sock_fd) < 0)
        printf ("Error: TCP socket close failed.\n");
    else
        mlfs_info("Socket(%d) closed.\n", sock_fd);
}
#endif

void print_all_connections(void)
{
	for (int i = 0; i < g_n_nodes; i++) {
		pr_setup("g_kernfs_peers[%d] ip=%s:\n", i, g_kernfs_peers[i]->ip);
		for (int j = 0; j < SOCK_TYPE_COUNT; j++) {
			pr_setup("\t sock_type[%d] = sockfd(%d)\n", j,
			       g_kernfs_peers[i]->sockfd[j]);
		}
	}
}

int init_rpc(struct mr_context *regions, int n_regions, char *listen_port, signal_cb_fn signal_callback, signal_cb_fn low_lat_signal_callback)
{
	assert(MAX_SIGNAL_BUF > MAX_REMOTE_PATH); //ensure that we can signal remote read requests (including file path)

	peer_init();

	printf("cluster settings:\n");
	//create array containing all peers
	for(int i=0; i<g_n_nodes; i++) {
		if(i<g_n_hot_rep) {
			g_peers[i] = clone_peer(&hot_replicas[i]);
			g_kernfs_peers[i] = g_peers[i];
		}
		else if(i>=g_n_hot_rep && i<(g_n_hot_rep + g_n_hot_bkp)) {
			g_peers[i] = clone_peer(&hot_backups[i - g_n_hot_rep]);
			g_kernfs_peers[i] = g_peers[i];
		}
		else if(i>=g_n_hot_rep + g_n_hot_bkp && i<(g_n_hot_rep + g_n_hot_bkp + g_n_cold_bkp)) {
			g_peers[i] = clone_peer(&cold_backups[i - g_n_hot_rep - g_n_hot_bkp]);
			g_kernfs_peers[i] = g_peers[i];
		} else {
#if MLFS_NAMESPACES
			g_peers[i] = clone_peer(&external_replicas[i - g_n_hot_rep - g_n_hot_bkp - g_n_cold_bkp]);
			g_kernfs_peers[i] = g_peers[i];
			g_kernfs_peers[i]->id = i;
			mlfs_printf("*** %s\n", "todo: register remote log");
			printf("--- EXTERNAL node %d - ip:%s\n", i, g_peers[i]->ip);
			continue;
#else
			panic("remote namespaces defined in configuration, but disabled in Makefile!\n");
#endif
		}

		g_kernfs_peers[i]->id = i;
		//g_peers[i] = g_kernfs_peers[i];
		register_peer_log(g_kernfs_peers[i], 0);

		if(!strcmp(g_kernfs_peers[i]->ip, g_self_ip)) {
			g_kernfs_id = g_kernfs_peers[i]->id;
		}

		printf("--- node %d - ip:%s\n", i, g_peers[i]->ip);
	}

	//NOTE: disable this check if we want to allow external clients (i.e. no local shared area)
	if(g_kernfs_id == -1)
		panic("local KernFS IP is not defined in src/distributed/rpc_interface.h\n");

#ifdef NIC_OFFLOAD
	// additional rdma agent for latency-sensitive channel.
	char *low_lat_port = mlfs_conf.low_latency_port;
#endif

#ifdef KERNFS
	g_self_id = g_kernfs_id;
	//set_peer_id(g_peers[g_self_id]);
	init_rdma_agent(listen_port, regions, n_regions, MAX_SIGNAL_BUF, add_peer_socket, remove_peer_socket,
			signal_callback, 0);
#ifdef NIC_SIDE
	// additional rdma agent for latency-sensitive channel (used in NIC kernfs).
	init_rdma_agent(low_lat_port, regions, n_regions, MAX_SIGNAL_BUF, add_peer_socket, remove_peer_socket,
			low_lat_signal_callback, 1);
#endif

#else
	init_rdma_agent(NULL, regions, n_regions, MAX_SIGNAL_BUF, add_peer_socket, remove_peer_socket,
			signal_callback, 0);
#ifdef NIC_OFFLOAD
	// additional rdma agent for latency-sensitive channel.
	init_rdma_agent(NULL, regions, n_regions, MAX_SIGNAL_BUF, add_peer_socket, remove_peer_socket,
			low_lat_signal_callback, 1);
#endif

#endif

#ifdef KERNFS
	// create connections to KernFS instances with i > g_self_id
	int do_connect = 0;

	// KernFS instances use a pid value of 0
	uint32_t pid = 0;
#else
	// connect to all KernFS instances
	int do_connect = 1;

	uint32_t pid = getpid();
#endif

	int num_peers = 0;
	int sock_cnt = 0;
	for(int i=0; i<g_n_nodes; i++) {
		if(!strcmp(g_kernfs_peers[i]->ip, g_self_ip)) {
                    if (!do_connect){
			do_connect = 1;
			continue;
                    }
		}

		num_peers++;

		if(do_connect) {
			printf("Connecting to KernFS instance %d [ip: %s]\n", i, g_kernfs_peers[i]->ip);

#ifdef NIC_OFFLOAD
#ifdef LIBFS
			if (is_kernfs_on_nic(i)) { // to nic kernfs
				// optimized initialization for g_n_replica == 3.
				// XXX It seems that all three connections (IO, BG, and LS)
				// are required for fast recovery from a failure..
				if (g_n_replica == 3) {
					if (is_first_kernfs(g_self_id, i)) {
						// first nic kernfs
						add_connection(g_kernfs_peers[i]->ip, low_lat_port, pid, 1, SOCK_IO, 1); // low_lat
						add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_BG, 0);
						add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_LS, 0);
						sock_cnt += 3;

					} else if (is_last_kernfs(g_self_id, i)) {
						// third nic kernfs
						add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_LS, 0);
						sock_cnt += 1;

					} else {
						// second nic kernfs
						add_connection(g_kernfs_peers[i]->ip, low_lat_port, pid, 1, SOCK_IO, 1); // low_lat
						add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_LS, 0);
						sock_cnt += 2;
					}

				} else {
					// Connect to all NIC kernfs.
					add_connection(g_kernfs_peers[i]->ip, low_lat_port, pid, 1, SOCK_IO, 1); // low_lat
					add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_BG, 0);
					add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_LS, 0);
					sock_cnt += 3;
				}

			} else {
				if (i == local_kernfs_id(g_self_id)) {
					// first host kernfs.
					add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_BG, 0); // to get publish ack.
					sock_cnt += 1;
				} else if (is_last_host_kernfs(g_self_id, i)) {
					if (mlfs_conf.persist_nvm_with_clflush) {
						add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_IO, 0); // to get fsync ack after flushing log.
						sock_cnt += 1;
					}
				} else {
					;
				}
			}
#else // KERNFS
#ifdef NIC_SIDE
			if (is_kernfs_on_nic(i)) { // to nic kernfs
				add_connection(g_kernfs_peers[i]->ip, low_lat_port, pid, 1, SOCK_IO, 1); // low_lat
				add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_BG, 0);
				add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_LS, 0);
				add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_LH, 0);
				sock_cnt += 4;
			} else if (i == nic_kid_to_host_kid(g_self_id)) { // to local host kernfs
				// SOCK_IO is required for nicrpc and getting peer's base address in init_replication.
				add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_IO, 0);
				add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_BG, 0);
				sock_cnt += 2;

			} else {  // to host kernfs on the other replicas
				// To replicate log to the NVM of the last replica.
				add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_IO, 0); // TODO change to low_lat??
				sock_cnt += 1;
			}

#else // HOST KERNFS
			if (is_kernfs_on_nic(i)) {
				if (i == host_kid_to_nic_kid(g_self_id)) {  // to local nic kernfs
					// SOCK_IO is required for nicrpc.
					add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_IO, 0);
					add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_BG, 0);
					sock_cnt += 2;
				} else { // to nic kernfs on the other replicas
					// To replicate log to the NVM of the last replica.
					add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_IO, 0);
					sock_cnt += 1;
				}
			} else { ; } // No connection between host KERNFSes.
#endif
#endif

#else // Host-only, No NIC offloading
			add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_IO, 0);
			add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_BG, 0);
			add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_LS, 0);
			add_connection(g_kernfs_peers[i]->ip, listen_port, pid, 1, SOCK_LH, 0);
			sock_cnt += 4;
#endif
		}
	}

	mlfs_debug("awaiting remote KernFS connections num_peers=%d\n", num_peers);

	//gettimeofday(&start_time, NULL);


	while(g_sock_count < sock_cnt) { // The number of add_connection() calls.
		if(rpc_shutdown)
			return 1;
		//cpu_relax();
		printf("Wait for connections established. %d/%d\n",
		       g_sock_count, sock_cnt);
		sleep(1);
	}

	printf("All connections established. %d/%d\n", g_sock_count, sock_cnt);
	print_all_connections();

	//gettimeofday(&end_time, NULL);
	//mlfs_printf("Bootstrap time: %lf\n", ((end_time.tv_sec  - start_time.tv_sec) * 1000000u + end_time.tv_usec - start_time.tv_usec) / 1.e6);

#ifdef LIBFS
	// contact the local KernFS to acquire a unique LibFS ID
	//// FIXME Duplicate LibFS IDs can be allocated to two different LibFSes
	//// if they request IDs to different KernFSes simultaneously.
	rpc_bootstrap(g_kernfs_peers[host_kid_to_nic_kid(g_kernfs_id)]->sockfd[SOCK_BG]);
#endif

	//replication is currently only initiated by LibFS processes
#if 0
	for(int i=0; i<n_regions; i++) {
		if(regions[i].type == MR_NVM_LOG) {
			// pick the node with the next (circular) id as replication target
			struct peer_id *next_replica = g_kernfs_peers[(g_kernfs_id + 1) % g_n_nodes];

			printf("init chain replication: next replica [node %d ip - %s]\n",
					next_replica->id, next_replica->ip);
			init_replication(g_self_id, g_kernfs_peers[(g_kernfs_id + 1) % g_n_nodes],
					log_begin, g_log_size, regions[i].addr, log_end);
		}
	}
#endif

#ifdef PROFILE_LOG_COPY_BW
	init_rt_bw_stat(&log_copy_bw_stat, "log_copy");
#endif

	//sleep(4);
	mlfs_printf("%s\n", "MLFS cluster initialized");

		//gettimeofday(&start_time, NULL);
}

int shutdown_rpc()
{
	rpc_shutdown = 1;
	shutdown_rdma_agent();
	return 0;
}

int mlfs_process_id()
{
	return g_self_id;
}

int peer_sockfd(int node_id, int type)
{
	assert(node_id >= 0 && node_id < (MAX_LIBFS_PROCESSES + g_n_nodes));
	return g_rpc_socks[g_kernfs_peers[node_id]->sockfd[type]]->fd;
}

#if 0
int rpc_connect(struct peer_id *peer, char *listen_port, int type, int poll_cq)
{
	time_t start = time(NULL);
	time_t current = start;
	time_t timeout = 5; // timeout interval in seconds

	int sockfd = add_connection(peer->ip, listen_port, type, poll_cq);

	//mlfs_info("Connecting to %s:%s on sock:%d\n", peer->ip, listen_port, peer->sockfd[type]);
	while(!rc_ready(sockfd)) {
		current = time(NULL);
		if(start + timeout < current)
			panic("failed to establish connection\n");
		//sleep(1);
	}
	//rpc_add_socket(peer, sockfd, type);
	return 0;
}

int rpc_listen(int sockfd, int count)
{
	int socktype = -1;
	struct peer_id *peer = NULL;

	/*
	time_t start = time(NULL);
	time_t current = start;
	time_t timeout = 10; //timeout interval in seconds
	*/

	mlfs_debug("%s", "listening for peer connections\n");

	for(int i=1; i<=count; i++) {
		while(rc_connection_count() < i || !rc_ready(rc_next_connection(sockfd))) {
			/*
			current = time(NULL);
			if(start + timeout < current)
				panic("failed to establish connection\n");
			*/

			if(rpc_shutdown)
				return 1;

			sleep(0.5);
		}

		sockfd = rc_next_connection(sockfd);
		socktype = rc_connection_type(sockfd);
		peer = find_peer(sockfd);

		if(!peer)
			panic("Unidentified peer tried to establish connection\n");

		rpc_add_socket(peer, sockfd, socktype);
	}

	mlfs_debug("%s", "found all peers\n");

	return 0;
}

int rpc_add_socket(struct peer_id *peer, int sockfd, int type)
{
	struct peer_socket *psock = mlfs_zalloc(sizeof(struct peer_socket));
	peer->sockfd[type] = sockfd;
	psock->fd = peer->sockfd[type];
	psock->seqn = 1;
	psock->type = type;
	psock->peer = peer;
	g_rpc_socks[peer->sockfd[type]] = psock;
	mlfs_info("Established connection with %s on sock:%d of type:%d\n",
			peer->ip, psock->fd, psock->type);
	//print_peer_id(peer);
}

#endif

struct rpc_pending_io * rpc_remote_read_async(char *path, loff_t offset, uint32_t io_size, uint8_t *dst, int rpc_wait)
{
	assert(g_n_cold_bkp > 0); //remote reads are only sent to reserve replicas

	//FIXME: search for cold_backup in g_peers array
	int sockfd = cold_backups[0].sockfd[SOCK_IO];
	//note: for signaling on the fast path, we write directly to the rdma buffer to avoid extra copying
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

#ifdef LIBFS
	if (enable_perf_stats)
		g_perf_stats.read_rpc_nr++;
#endif

	//setting msg->id allows us to insert a hook in the rdma driver to later wait until response received
	if(rpc_wait)
		msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]); 

	// FIXME: Use inum instead of path for read RPCs
	// temporarily trimming paths to reduce overheads
	char trimmed_path[MAX_REMOTE_PATH];
	snprintf(trimmed_path, MAX_REMOTE_PATH, "%s", path);

	snprintf(msg->data, MAX_SIGNAL_BUF, "|read |%s |%lu|%u|%lu", trimmed_path, offset, io_size, (uintptr_t)dst);
	mlfs_info("trigger async remote read: path[%s] offset[%lu] io_size[%d]\n",
			path, offset, io_size);
	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, rpc_wait);
	struct rpc_pending_io *rpc = mlfs_zalloc(sizeof(struct rpc_pending_io));
	rpc->seq_n = msg->id;
	rpc->type = RPC_PENDING_RESPONSE;
	rpc->sockfd = sockfd;

	return rpc;
}

int rpc_remote_read_sync(char *path, loff_t offset, uint32_t io_size, uint8_t *dst)
{
	assert(g_n_cold_bkp > 0); //remote reads are only sent to reserve replicas

	int sockfd = cold_backups[0].sockfd[SOCK_IO]; 
	//note: for signaling on the fast path, we write directly to the rdma buffer to avoid extra copying
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);
	uint64_t start_tsc;

#ifdef LIBFS
	if (enable_perf_stats)
		g_perf_stats.read_rpc_nr++;
#endif

	//setting msg->id allows us to insert a hook in the rdma driver to later wait until response received
	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);

	// FIXME: Use inum instead of path for read RPCs
	// temporarily trimming paths to reduce overheads
	char trimmed_path[MAX_REMOTE_PATH];
	snprintf(trimmed_path, MAX_REMOTE_PATH, "%s", path);

	snprintf(msg->data, MAX_SIGNAL_BUF, "|read |%s |%lu|%u|%lu", trimmed_path, offset, io_size, (uintptr_t)dst);
	mlfs_info("trigger sync remote read: path[%s] offset[%lu] io_size[%d]\n",
			path, offset, io_size);

	//we still send an async msg, since we want to synchronously wait for the msg response and not
	//rdma-send completion
	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);

#ifdef LIBFS
	if (enable_perf_stats)
		start_tsc = asm_rdtscp();
#endif
	//spin till we receive a response with the same sequence number
	//this is part of the messaging protocol that the remote peer should adhere to
	IBV_AWAIT_RESPONSE(sockfd, msg->id);

#ifdef LIBFS
	if (enable_perf_stats)
		g_perf_stats.read_rpc_wait_tsc += (asm_rdtscp() - start_tsc);
#endif

	return 0;
}

//send back response for read rpc
//note: responses are always asynchronous
int rpc_remote_read_response(int sockfd, rdma_meta_t *meta, int mr_local, uint64_t seq_n)
{
	mlfs_assert(0); // Not used any more
	mlfs_info("trigger remote read response for mr: %d with seq_n: %lu (%lx)\n", mr_local, seq_n, seq_n);
	//responses are sent from specified local memory region to requester's read cache
	if(seq_n) {
		meta->imm = seq_n; //set immediate to sequence number in order for requester to match it (in case of io wait)
#ifdef NIC_OFFLOAD
		IBV_WRAPPER_WRITE_WITH_IMM_ASYNC(sockfd, meta, mr_local, MR_DRAM_BUFFER);
#else
		IBV_WRAPPER_WRITE_WITH_IMM_ASYNC(sockfd, meta, mr_local, MR_DRAM_CACHE);
#endif
	}
	else
#ifdef NIC_OFFLOAD
		IBV_WRAPPER_WRITE_ASYNC(sockfd, meta, mr_local, MR_DRAM_BUFFER);
#else
		IBV_WRAPPER_WRITE_ASYNC(sockfd, meta, mr_local, MR_DRAM_CACHE);
#endif
}

int rpc_lease_change(int mid, int rid, uint32_t inum, int type, uint32_t version, addr_t blknr, int sync)
{
	uint64_t start_tsc;

	int sockfd;
#if 0
	if(!type)
		sockfd = g_peers[mid]->sockfd[SOCK_BG];
	else
		sockfd = g_peers[mid]->sockfd[SOCK_IO];
#endif
      if (type == LEASE_FREE)
		sockfd = g_peers[mid]->sockfd[SOCK_BG];
      else
		sockfd = g_peers[mid]->sockfd[SOCK_IO];

	//note: for signaling on the fast path, we write directly to the rdma buffer to avoid extra copying
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

#ifdef LIBFS
	if (enable_perf_stats) {
		start_tsc = asm_rdtscp();
		g_perf_stats.lease_rpc_nr++;
	}
#endif

	//setting msg->id allows us to insert a hook in the rdma driver to later wait until response received
	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);

	// FIXME: Use inum instead of path for read RPCs
	// temporarily trimming paths to reduce overheads
	//char trimmed_path[MAX_REMOTE_PATH];
	//snprintf(trimmed_path, MAX_REMOTE_PATH, "%s", path);

	snprintf(msg->data, MAX_SIGNAL_BUF, "|lease |%u|%u|%d|%u|%lu", rid, inum, type, version, blknr);
	mlfs_printf("\x1b[33m [L] trigger lease acquire: inum[%u] type[%d] version[%u] blknr[%lu] (%s) \x1b[0m\n",
			inum, type, version, blknr, sync?"SYNC":"ASYNC");
	//mlfs_printf("msg->data %s buffer_id %d\n", msg->data, buffer_id);

	//we still send an async msg, since we want to synchronously wait for the msg response and not
	//rdma-send completion
	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, sync);

	//spin till we receive a response with the same sequence number
	//this is part of the messaging protocol that the remote peer should adhere to
	if(sync)
		IBV_AWAIT_RESPONSE(sockfd, msg->id);

#ifdef LIBFS
	if (enable_perf_stats)
		g_perf_stats.lease_rpc_wait_tsc += (asm_rdtscp() - start_tsc);
#endif

	return 0;
}

int rpc_lease_response(int sockfd, uint64_t seq_n, int replicate)
{
	if(replicate) {
		mlfs_info("trigger lease response [+rsync] with seq_n: %lu\n", seq_n);
		//note: for signaling on the fast path, we write directly to the rdma buffer to avoid extra copying
		struct app_context *msg;
		int buffer_id = rc_acquire_buffer(sockfd, &msg);

		msg->id = seq_n;

		snprintf(msg->data, MAX_SIGNAL_BUF, "|replicate |%d", replicate);

		IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
	}
	else {
		mlfs_info("trigger lease response with seq_n: %lu\n", seq_n);
		rdma_meta_t *meta = create_rdma_ack();
		//responses are sent from specified local memory region to requester's read cache
		if(seq_n) {
			meta->imm = seq_n; //set immediate to sequence number in order for requester to match it (in case of io wait)
			(sockfd, meta, 0, 0);
		}
		else
			panic("undefined codepath\n");
	}
}

int rpc_lease_flush_response(int sockfd, uint64_t seq_n, uint32_t inum, uint32_t version, addr_t blknr)
{
	struct app_context *msg;

	//int sockfd = g_peers[id]->sockfd[SOCK_BG];
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	// convert log segment (TODO: remove these conversions)
	// TODO: describe methodology for computing log segments are computed
	//int seg = abs(g_rpc_socks[sockfd]->peer->id - g_self_id) % g_n_nodes;
	//start_digest = start_digest - g_log_size * seg;

	snprintf(msg->data, MAX_SIGNAL_BUF, "|lease |%u|%d|%u|%lu", inum, LEASE_FREE, version, blknr);

	msg->id = seq_n; //set immediate to sequence number in order for requester to match it

	mlfs_info("trigger remote revoke response: inum[%u] version[%u] blknr[%lu]\n", inum, version, blknr);
	mlfs_printf("%s\n", msg->data);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
	return 0;
}



#ifdef KERNFS
// force a LibFS to flush dirty inodes/dirents (and digest if necessary)
int rpc_lease_flush(int peer_id, uint32_t inum, int force_digest)
{
	mlfs_info("trigger lease flush %speer %d inum %u\n", force_digest?"[+rsync/digest] ":"", peer_id, inum);

	// peer has already disconnected; exit
	if(!g_peers[peer_id])
		return -1;

	int sockfd = g_peers[peer_id]->sockfd[SOCK_LS];

	//note: for signaling on the fast path, we write directly to the rdma buffer to avoid extra copying
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);
	uint64_t start_tsc;

	if (enable_perf_stats && force_digest) {
		g_perf_stats.lease_contention_nr++;
	}

	//setting msg->id allows us to insert a hook in the rdma driver to later wait until response received
	//msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
	msg->id = 0;

	snprintf(msg->data, MAX_SIGNAL_BUF, "|flush |%u|%d", inum, force_digest);

	mlfs_printf("peer send: %s\n", msg->data);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);

	//spin till we receive a response with the same sequence number
	//this is part of the messaging protocol that the remote peer should adhere to

	if(force_digest)
		IBV_AWAIT_RESPONSE(sockfd, msg->id);

	return 0;
}

// lease request invalid (usually due to changing lease manangers)
// LibFS should flush its lease state and retry sending to new lease manager
int rpc_lease_invalid(int sockfd, int peer_id, uint32_t inum, uint64_t seqn)
{
	mlfs_info("mark lease outdated for peer %d inum %u\n", peer_id, inum);

	// peer has already disconnected; exit
	if(!g_peers[peer_id])
		return -1;

	//int sockfd = g_peers[peer_id]->sockfd[SOCK_LS];

	//note: for signaling on the fast path, we write directly to the rdma buffer to avoid extra copying
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);
	uint64_t start_tsc;

	//setting msg->id allows us to insert a hook in the rdma driver to later wait until response received
	//msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
	msg->id = seqn;

	snprintf(msg->data, MAX_SIGNAL_BUF, "|error |%u", inum);

	mlfs_printf("peer send: %s\n", msg->data);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);

	return 0;
}

// force a LibFS to flush dirty inodes/dirents (and digest if necessary)
int rpc_lease_migrate(int peer_id, uint32_t inum, uint32_t kernfs_id)
{
	mlfs_info("trigger lease migration peer %d inum %u kernfs_id %u\n", peer_id, inum, kernfs_id);

	// peer has already disconnected; exit
	if(!g_peers[peer_id])
		return -1;

	int sockfd = g_peers[peer_id]->sockfd[SOCK_LS];

	//note: for signaling on the fast path, we write directly to the rdma buffer to avoid extra copying
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);
	uint64_t start_tsc;

	//setting msg->id allows us to insert a hook in the rdma driver to later wait until response received
	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);

	snprintf(msg->data, MAX_SIGNAL_BUF, "|migrate |%u|%u", inum, kernfs_id);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);

	return 0;
}
#endif

static void rpc_send_replicate_rdma_entry(peer_meta_t *peer,
	struct rdma_meta_entry *rdma_entry, int send_sockfd,
	struct rpcmsg_replicate *rep)
{
    uint32_t imm = 0;
    char msg[MAX_SOCK_BUF];
    rdma_meta_t *rdma_meta_r;
#ifdef PROFILE_LOG_COPY_BW
    uint64_t total_sent_size = 0;
#endif

    pr_rep("Replication start: dst_ip=%s remote_addr=0x%lx "
	   "len=%lu(bytes) send_sockfd=%d",
           peer->info->ip,
           rdma_entry->meta->addr,
           rdma_entry->meta->length,
           send_sockfd);
    print_rpcmsg_replicate(rep, __func__);

    rpcmsg_build_replicate(msg, rep);
    mlfs_debug("send to sock[%d]: msg=%s\n", send_sockfd, msg);
#ifdef PROFILE_LOG_COPY_BW
    total_sent_size += rdma_entry->meta->length;
#endif

    START_TIMER(evt_rep_log_copy);

    START_TIMER(evt_rep_log_copy_write);
    IBV_WRAPPER_WRITE_ASYNC(send_sockfd, rdma_entry->meta, MR_NVM_LOG, MR_NVM_LOG);
    END_TIMER(evt_rep_log_copy_write);

    if (mlfs_conf.persist_nvm_with_rdma_read) {
	START_TIMER(evt_rep_log_copy_read);
	    // Send READ RDMA to persist data. Read the last 1 byte.
	    uintptr_t last_local_addr;
	    uintptr_t last_remote_addr;
	    last_local_addr = rdma_entry->meta->sge_entries[0].addr + rdma_entry->meta->length - 1;
	    last_remote_addr = rdma_entry->meta->addr + rdma_entry->meta->length -1;

	    rdma_meta_r = create_rdma_meta(last_local_addr, last_remote_addr, 1);

	    IBV_WRAPPER_READ_ASYNC(send_sockfd, rdma_meta_r, MR_NVM_LOG, MR_NVM_LOG);
	END_TIMER(evt_rep_log_copy_read);
	END_TIMER(evt_rep_log_copy);
    }

    END_TIMER(evt_rep_critical_host); // critical path ends at replica 2 and libfs.
    START_TIMER(evt_rep_wait_req_done);
//     rpc_forward_msg_no_seqn(send_sockfd, msg);
    rpc_forward_msg_no_seqn_sync(send_sockfd, msg);
//     rpc_forward_msg_sync(send_sockfd, msg);
    END_TIMER(evt_rep_wait_req_done);

#ifdef PROFILE_LOG_COPY_BW
    check_rt_bw(&log_copy_bw_stat, total_sent_size);
#endif

    mlfs_free(rdma_meta_r);

}

/** Copy to local host NVM. **/
// static void send_rdma_entry_for_local_host_copy(peer_meta_t *peer,
//         struct rdma_meta_entry *rdma_entry, int send_sockfd)
// {
//     int local_mr_type, remote_mr_type;

//     pr_rep("Replication to local host start: dst_ip=%s remote_addr=0x%lx "
//            "len=%lu(bytes) send_sockfd=%d",
//            peer->info->ip,
//            rdma_entry->meta->addr,
//            rdma_entry->meta->length,
//            send_sockfd);

//     local_mr_type = MR_DRAM_BUFFER;
//     remote_mr_type = MR_NVM_LOG;

//     IBV_WRAPPER_WRITE_ASYNC(send_sockfd, rdma_entry->meta, local_mr_type,
//                             remote_mr_type);

//     if (mlfs_conf.persist_nvm_with_rdma_read) {
//         uint32_t wr_id;
//         wr_id = IBV_WRAPPER_READ_ASYNC(send_sockfd, rdma_entry->meta,
//                 local_mr_type, remote_mr_type);
//         // We don't need to wait local copy assuming that it finishes earlier
//         // than the remote replication.
//         // IBV_AWAIT_WORK_COMPLETION_WITH_POLLING(send_sockfd, wr_id, 0);
//         IBV_AWAIT_WORK_COMPLETION_WITH_POLLING(send_sockfd, wr_id, 1);
//     }
// }

#if 0
// Not tested. TODO
void free_rdma_entries(struct list_head *rdma_entries)
{
    mlfs_printf("IN FREE rdma_entries=%p prev=%p next=%p\n", rdma_entries, rdma_entries->prev, rdma_entries->next);
    struct rdma_meta_entry *rdma_entry, *rdma_tmp;
    list_for_each_entry_safe(rdma_entry, rdma_tmp, rdma_entries, head) {
        list_del(&rdma_entry->head);
        mlfs_free(rdma_entry->meta);
        mlfs_free(rdma_entry);
    }
}
#endif

/**
* There are two cases.
* 1) It is the first node in the chain (LibFS).
* 2) It is in the middle of the chain. I.e., Node 2 in chain; 1->2->3.
*
* In 1), rdma_entries can have multiple rdma_entry.
* In 2), rdma_entries has only one rdma_entry because every rdma_entry
* from node 1 calls this function one by one.
*
* rpc_replicate_log_initiate() implements 1).
* rpc_replicate_log_relay() implements 2).
*/
void
rpc_replicate_log_initiate (peer_meta_t *peer, struct list_head *rdma_entries)
{
    struct rdma_meta_entry *rdma_entry, *rdma_tmp;
    uint32_t imm;
    struct rpcmsg_replicate rep = {0,};

    // last peer in the chain sends back an ack to libfs.
    struct peer_id *ack_replica;
    int send_sockfd;
    int rcv_sockfd;

    uint64_t start_tsc_tmp;
    uint32_t *ack_bit_p;
#ifdef NO_BUSY_WAIT
    pthread_mutex_t rep_ack_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

    ack_bit_p = nic_slab_alloc_in_byte(sizeof(uint32_t));
    *ack_bit_p = 0;

    ack_replica = g_kernfs_peers[get_last_node_id()];
    send_sockfd = peer->info->sockfd[SOCK_IO];
    rcv_sockfd = ack_replica->sockfd[SOCK_IO];

    //if (enable_perf_stats)
    //	start_tsc_tmp = asm_rdtscp();

    rep.libfs_id = g_self_id;
    rep.persist = g_enable_rpersist;
    rep.steps = g_rsync_rf - 1;

    list_for_each_entry_safe(rdma_entry, rdma_tmp, rdma_entries, head) {
#ifdef RECORD_OPS_FOR_HYPERLOOP_EXE
	/* Record operations for Hyperloop execution. */
	// It could be larger than 1 if coalescing is enabled.
	// TODO to be deleted. It is only for checking.
	mlfs_assert(rdma_entry->meta->sge_count == 1);

        for (int i = 0; i < rdma_entry->meta->sge_count; i++) {
	    // printf("%lx,%lx,%lx,%u\n", rdma_entry->meta->sge_entries[i].addr,
	    //         g_sync_ctx[0]->base_addr,
	    //         rdma_entry->meta->sge_entries[i].addr -
	    //         g_sync_ctx[0]->base_addr,
	    //         rdma_entry->meta->sge_entries[i].length);

	    // print format: [offset,size]
            printf("%lu,%u\n",
                   rdma_entry->meta->sge_entries[i].addr -
                       g_sync_ctx[0]->base_addr,
                   rdma_entry->meta->sge_entries[i].length);
        }
#endif
        // Set replication meta.
	// Do not use imm.
        rep.common.seqn = generate_rpc_seqn(g_rpc_socks[ack_replica->sockfd[SOCK_IO]]);
        rep.n_log_blks = (ALIGN(rdma_entry->meta->length, g_block_size_bytes)) >> g_block_size_shift;
	rep.is_imm = 0;
        if(list_is_last(&rdma_entry->head, rdma_entries)) {
	    rep.ack = 1;
#ifdef NO_BUSY_WAIT
	    rep.ack_bit_p = (uintptr_t)&rep_ack_mutex;
#else
	    rep.ack_bit_p = (uintptr_t)ack_bit_p;
#endif

	} else {
	    rep.ack = 0;
	    rep.ack_bit_p = 0;
	}
	rep.end_blknr = atomic_load(g_sync_ctx[0]->end);

#if 0
        // TODO Optimize for 2 nodes && no persist case.
        if (g_rsync_rf <= 2 && !g_enable_rpersist) {
            // TODO We can use one-way RDMA.
        }
#endif
        // Send each rdma entry.
        rpc_send_replicate_rdma_entry(peer, rdma_entry, send_sockfd, &rep);

	START_TIMER(evt_rep_send_and_wait_msg);
        if(list_is_last(&rdma_entry->head, rdma_entries)) {
            // check that rdma operations are not batched (disabled for now)
            mlfs_assert(rdma_entry->meta->next == 0);
            pr_rep("Start waiting seqn=%lu", rep.common.seqn);

#ifdef NO_BUSY_WAIT
	    if (rep.ack_bit_p) {
		    // This is unlocked in signal_callback() at log.c
		    pthread_mutex_lock(&rep_ack_mutex);
		//     pthread_mutex_unlock(&rep_ack_mutex);
	    }
#else
	    // Wait for ack.
	//     IBV_AWAIT_ACK_SPINNING(rcv_sockfd, ack_bit_p);
	    IBV_AWAIT_ACK_POLLING(rcv_sockfd, ack_bit_p);
#endif
            pr_rep("Replication done: dst_ip=%s remote_addr=0x%lx "
                    "len=%lu(bytes) send_sockfd=%d rcv_sockfd=%d",
                    peer->info->ip,
                    rdma_entry->meta->addr,
                    rdma_entry->meta->length,
                    send_sockfd,
                    rcv_sockfd);
	    START_TIMER(evt_rep_critical_host2);
        }
	END_TIMER(evt_rep_send_and_wait_msg);
        // list_del(&rdma_entry->head);
        // mlfs_free(rdma_entry->meta);
        // mlfs_free(rdma_entry);
    }

    nic_slab_free(ack_bit_p);

    // free_rdma_entries(rdma_entries);

    //if (enable_perf_stats)
    //	g_perf_stats.tmp_tsc += (asm_rdtscp() - start_tsc_tmp);
}

void
rpc_replicate_log_relay (peer_meta_t *peer, struct list_head *rdma_entries,
	struct rpcmsg_replicate *rep)
{
    struct rdma_meta_entry *rdma_entry, *rdma_tmp;
    uint32_t imm;

    // Without NIC-offloading last peer in the chain sends back an ack to libfs.
    int send_sockfd = peer->info->sockfd[SOCK_IO];

    uint64_t start_tsc_tmp;

    //if (enable_perf_stats)
    //	start_tsc_tmp = asm_rdtscp();

    int count = 0;
    list_for_each_entry_safe(rdma_entry, rdma_tmp, rdma_entries, head) {
        // Currently, there is only one rdma_entry in rdma_entries.
        // If not, reconsider this function.
        mlfs_assert(count == 0);
        count++;

	if (rep->ack && list_is_last(&rdma_entry->head, rdma_entries)) {
	    // We are not sending ack to the previous node.
	} else {
	    rep->ack_bit_p = 0;
	}

        // Send each rdma entry.
        rpc_send_replicate_rdma_entry (peer, rdma_entry, send_sockfd, rep);

        if(list_is_last(&rdma_entry->head, rdma_entries)) {
            // check that rdma operations are not batched (disabled for now)
            mlfs_assert(rdma_entry->meta->next == 0);

            pr_rep("Replication done: dst_ip=%s remote_addr=0x%lx "
                    "len=%lu(bytes) send_sockfd=%d",
                    peer->info->ip,
                    rdma_entry->meta->addr,
                    rdma_entry->meta->length,
                    send_sockfd);
        }
        // list_del(&rdma_entry->head);
        // mlfs_free(rdma_entry->meta);
        // mlfs_free(rdma_entry);
    }

#ifdef NIC_SIDE
    // With NIC-offloading, if the next kernfs is the last peer in the chain,
    // ack to libfs.
    if (next_kernfs_is_last(peer->id) && rep->ack){
	END_TIMER(evt_rep_critical_nic); // replica 2

	START_TIMER(evt_handle_rpcmsg_misc);

	rpc_set_remote_bit(g_sync_ctx[peer->id]->next_rep_msg_sockfd[0],
		rep->ack_bit_p);
    }
#endif

    // free_rdma_entries(rdma_entries);

    //if (enable_perf_stats)
    //	g_perf_stats.tmp_tsc += (asm_rdtscp() - start_tsc_tmp);
}

/**
 * acknowledge of lease.
 */
int rpc_send_lease_ack(int sockfd, uint64_t seqn)
{
        pr_rpc("trigger lease ACK on sock:%d with seqn: %lu", sockfd, seqn);
	struct app_context *msg;

	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	// convert log segment (TODO: remove these conversions)
	// TODO: describe methodology for computing log segments are computed
	//int seg = abs(g_rpc_socks[sockfd]->peer->id - g_self_id) % g_n_nodes;
	//start_digest = start_digest - g_log_size * seg;

	snprintf(msg->data, MAX_SIGNAL_BUF, "|" TO_STR(RPC_LEASE_ACK) " |%d|", 1);
	msg->id = seqn;

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);

        pr_rpc("ACK sent sock=%d seqn=%lu", sockfd, seqn);
	return 0;
}



//send digest request asynchronously
struct rpc_pending_io * rpc_remote_digest_async(int peer_id, peer_meta_t *peer,
        uint32_t n_digest, uint32_t n_blk_digest, int rpc_wait)
{
	//mlfs_assert(peer->n_used_nonatm > 0);

	if(!n_digest) {
		n_digest = atomic_load(&peer->n_digest);
		mlfs_debug("nothing to digest for peer %d, returning..\n", peer->info->id);
		return NULL;
	}

#ifdef NIC_OFFLOAD
	int sockfd = g_sync_ctx[0]->next_digest_sockfd;
#else
	//int sockfd = g_kernfs_peers[peer_id]->sockfd[SOCK_BG];
	int sockfd = peer_sockfd(peer_id, SOCK_BG);
	//int sockfd = peer->info->sockfd[SOCK_IO];
#endif

	////////// JYKIM It might not be required.set_peer_digesting() also waits.
	if(peer->digesting) {
		//FIXME: optimize. blocking might be a bit extreme here
		wait_on_peer_digesting(peer);
		//mlfs_info("%s", "[L] remote digest failure: peer is busy\n");
	}
	///////////////////////////////////////////////////////////////////////
	set_peer_digesting(peer);

	addr_t digest_blkno = peer->start_digest;
	addr_t start_blkno = g_sync_ctx[0]->begin;
	addr_t end_blkno = peer->remote_end;

	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);
	// TODO: describe how log devs are computed for remote nodes
	//int log_id = abs(peer->info->id - g_self_id) % g_n_nodes;
	int log_id = g_self_id;
	snprintf(msg->data, MAX_SIGNAL_BUF, "|digest |%d|%d|%u|%lu|%lu|%lu|%u",
                        log_id, g_log_dev, n_digest, digest_blkno, start_blkno, end_blkno, n_blk_digest);

	//setting msg->id allows us to insert a hook in the rdma agent to later wait until response received
	if(rpc_wait)
		msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, rpc_wait);

	pr_rpc("peer send: %s sock=%d", msg->data, sockfd);
        pr_digest(
            "Remote digest request: log_id=%d dev_id=%d n_loghdr_digest=%u "
            "digest_start=%lu log_start=%lu log_end=%lu n_blk_digest=%u "
            "steps=%u sock=%d",
            log_id, g_log_dev, n_digest, digest_blkno, start_blkno, end_blkno,
            n_blk_digest, g_rsync_rf - 1, sockfd);

	return NULL;
}

#if 0
//send digest request asynchronously
int rpc_remote_digest_sync(int peer_id, peer_meta_t *peer, uint32_t n_digest)
{
	if(!n_digest)
		n_digest = atomic_load(&peer->n_digest);

	if(!n_digest) {
		mlfs_debug("nothing to digest for peer %d, returning..\n", peer->info->id);
		return 0;
	}

	int sockfd = g_kernfs_peers[peer_id]->sockfd[SOCK_BG];
	//int sockfd = peer->info->sockfd[SOCK_IO];

	if(peer->digesting) {
		//FIXME: optimize. blocking might be a bit extreme here
 		wait_on_peer_digesting(peer);
		//mlfs_info("%s", "[L] remote digest failure: peer is busy\n");
	}

	peer->digesting = 1;

	// TODO: describe how log devs are computed for remote nodes
	int dev = abs(peer->info->id - g_self_id) % g_n_nodes;

	addr_t digest_blkno = peer->start_digest;
	addr_t start_blkno = g_sync_ctx[0]->begin;
	addr_t end_blkno = peer->remote_end;

	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);
	//int log_id = abs(peer->info->id - g_self_id) % g_n_nodes;
	int log_id = g_self_id;
	snprintf(msg->data, MAX_SIGNAL_BUF, "|digest |%d|%d|%u|%lu|%lu|%lu|%u",
			log_id, g_log_dev+1, n_digest, digest_blkno, start_blkno, end_blkno, g_rsync_rf-1);

	//setting msg->id allows us to insert a hook in the rdma driver to later wait until response received
	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);

	mlfs_printf("peer send: %s\n", msg->data);
	mlfs_info("trigger remote digest (sync) on sock %d: dev[%d] digest_blkno[%lu] n_digest[%u] end_blkno[%lu] steps[%u]\n",
			sockfd, log_id, n_digest, digest_blkno, end_blkno, g_rsync_rf-1);

#if 0
	atomic_store(&peer->n_digest, 0);
	atomic_store(&peer->n_used, 0);
	atomic_store(&peer->n_used_blk, 0);
#endif

	//spin till we receive a response with the same sequence number
	//this is part of the messaging protocol that the remote peer should adhere to
	IBV_AWAIT_RESPONSE(sockfd, msg->id);

	/*
	else if(peer->digesting && (peer->n_used_blk_nonatm
		       	+ peer->n_unsync_blk_nonatm > g_sync_ctx->size*9/10)) {
		panic("[error] unable to rsync: remote log is full\n");
	}
	else
		mlfs_info("%s\n", "failed to trigger remote digest. remote node is busy");
	*/
	return 0;
}
#endif

//send back digest response
int rpc_remote_digest_response(int sockfd, int libfs_id, int dev, addr_t start_digest,
	int n_digested, int rotated, uint64_t seq_n,
	uint32_t n_digested_blks) {
    struct app_context *msg;

    // int sockfd = g_peers[libfs_id]->sockfd[SOCK_BG];
    int buffer_id = rc_acquire_buffer(sockfd, &msg);

    // convert log segment (TODO: remove these conversions)
    // TODO: describe methodology for computing log segments are computed
    // int seg = abs(g_rpc_socks[sockfd]->peer->id - g_self_id) % g_n_nodes;
    // start_digest = start_digest - g_log_size * seg;

    snprintf(msg->data, MAX_SIGNAL_BUF, "|complete |%d|%d|%d|%lu|%d|%d|%u", libfs_id,
	    dev, n_digested, start_digest, rotated, 0, n_digested_blks);
    // msg->id = seq_n; // set immediate to sequence number in order for requester
    //                  // to match it
    msg->id = 0; // We don't need seqn. Requester waits seeing peer->digesting.

    pr_digest("trigger remote digest response: n_digested[%d] rotated[%d]",
	    n_digested, rotated);
    pr_rpc("peer send: %s sock=%d seqn=%lu", msg->data, sockfd, seq_n);

    IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
    return 0;
}

int rpc_forward_msg(int sockfd, char* data)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
	snprintf(msg->data, MAX_SIGNAL_BUF, "%s",data);

	pr_rpc("peer send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);
	return 0;
}

/**
 * @Synopsis
 *
 * @Param sockfd
 * @Param data
 * @Param libfs_id
 *
 * @Returns  wr_id (Use it to wait its response.)
 */
uint32_t rpc_forward_msg_with_per_libfs_seqn(int sockfd, char* data, int libfs_id)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = generate_rpc_per_libfs_seqn(g_rpc_socks[sockfd], libfs_id);
	msg->libfs_id = libfs_id;
	snprintf(msg->data, MAX_SIGNAL_BUF, "%s",data);

        pr_rpc("peer send: %s seqn(per_libfs)=%lu libfs_id=%d sock=%d",
               msg->data, msg->id, msg->libfs_id, sockfd);

	// PRINT
	// mlfs_printf(ANSI_COLOR_BLUE "peer send: %s seqn(per_libfs)=%lu "
	//                             "libfs_id=%d sock=%d\n" ANSI_COLOR_RESET,
	//             msg->data, msg->id, msg->libfs_id, sockfd);

	return IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);
}

int rpc_forward_msg_sync(int sockfd, char* data)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
	snprintf(msg->data, MAX_SIGNAL_BUF, "%s",data);

	pr_rpc("peer send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_SYNC(sockfd, buffer_id, 1);
	return 0;
}

int rpc_forward_msg_no_seqn(int sockfd, char* data)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = 0;
	snprintf(msg->data, MAX_SIGNAL_BUF, "%s", data);

	pr_rpc("peer send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);
	return 0;
}

int rpc_forward_msg_no_seqn_sync(int sockfd, char* data)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = 0;
	snprintf(msg->data, MAX_SIGNAL_BUF, "%s",data);

	pr_rpc("peer send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_SYNC(sockfd, buffer_id, 0);
	return 0;
}

int rpc_forward_msg_and_wait_cq(int sockfd, char* data)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
	snprintf(msg->data, MAX_SIGNAL_BUF, "%s",data);

	pr_rpc("peer send and wait cq: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	uint32_t wr_id = IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);

        // Note, spinning within low latency channel without polling CQ may
        // result in a deadlock. This function should not be called in the CQ
        // polling thread.
	IBV_AWAIT_WORK_COMPLETION_WITH_POLLING(sockfd, wr_id, 1);

	return 0;
}

int rpc_forward_msg_with_per_libfs_seqn_and_wait_cq(int sockfd, char* data,
						    int libfs_id)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = generate_rpc_per_libfs_seqn(g_rpc_socks[sockfd], libfs_id);
	msg->libfs_id = libfs_id;
	snprintf(msg->data, MAX_SIGNAL_BUF, "%s",data);

        pr_rpc("peer send and wait cq: %s seqn(per_libfs)=%lu libfs_id=%d sock=%d",
               msg->data, msg->id, msg->libfs_id, sockfd);

        uint32_t wr_id = IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);

        // Note, spinning within low latency channel without polling CQ may
        // result in a deadlock. This function should not be called in the CQ
        // polling thread.
	IBV_AWAIT_WORK_COMPLETION_WITH_POLLING(sockfd, wr_id, 1);

	return 0;
}

// TODO to be replaced with ack_bit mechanism. Not used.
//int rpc_forward_msg_and_wait_resp(int sockfd, char* data)
//{
//	struct app_context *msg;
//	int buffer_id = rc_acquire_buffer(sockfd, &msg);
//
//	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
//	snprintf(msg->data, MAX_SIGNAL_BUF, "%s",data);
//
//	pr_rpc("peer send and wait resp: %s", msg->data);
//
//	IBV_WRAPPER_SEND_MSG_SYNC(sockfd, buffer_id, 1);
//	IBV_AWAIT_RESPONSE(sockfd, msg->id);
//	return 0;
//}

// TODO to be replaced with ack_bit mechanism.
int rpc_forward_msg_with_per_libfs_seqn_and_wait_resp(int sockfd, char* data,
						    int libfs_id)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = generate_rpc_per_libfs_seqn(g_rpc_socks[sockfd], libfs_id);
	msg->libfs_id = libfs_id;
	snprintf(msg->data, MAX_SIGNAL_BUF, "%s",data);

	pr_rpc("%lu peer send and wait resp: %s seqn(per_libfs)=%lu "
	       "libfs_id=%d sock=%d",
	       get_tid(), msg->data, msg->id, msg->libfs_id, sockfd);

	IBV_WRAPPER_SEND_MSG_SYNC(sockfd, buffer_id, 1);
	IBV_AWAIT_PER_LIBFS_RESPONSE(sockfd, msg->id, libfs_id);

	return 0;
}


/**
 * These interfaces are used to set msg->data in its caller.
 */
int rpc_setup_msg (struct app_context **app, int sockfd)
{
        struct app_context *msg;
        uint64_t seq_n;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

        // Set seq_n.
        seq_n = generate_rpc_seqn(g_rpc_socks[sockfd]);
        msg->id = seq_n;

        *app = msg;
        return buffer_id;
}

// FIXME it seems not working correctly. It resume before we get response.
int rpc_send_msg_sync (int sockfd, int buffer_id, struct app_context *msg)
{
    if (msg->data == NULL)
        panic("Error: In rpc_send_msg_sync msg->data is NULL.\n");

    mlfs_info("Send msg sync: msg->id %lu msg->data %s\n", msg->id, msg->data);

    IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);

    // spin till we receive a response with the greater or equal sequence number.
    IBV_AWAIT_RESPONSE(sockfd, msg->id);
    return 0;
}

int rpc_send_msg_and_wait_ack (int sockfd, char *data, uint64_t *ack_bit_p)
{
    struct app_context *msg;
    int buffer_id = rc_acquire_buffer(sockfd, &msg);

    // We don't need seqn. Set it to 0.
    msg->id = 0;
    snprintf(msg->data, MAX_SIGNAL_BUF, "%s",data);

    pr_rpc("peer send: %s seqn=%lu sock=%d ack_bit_p=%p", msg->data, msg->id,
	    sockfd, ack_bit_p);

    IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);

    // Wait for ack_bit (sync_bit).
    while (*ack_bit_p == 0)
	cpu_relax();

    return 0;
}

int rpc_bootstrap(int sockfd)
{
#ifdef LIBFS
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

#ifdef NIC_OFFLOAD
	libfs_rate_limit_flag = (uint64_t *)nic_slab_alloc_in_byte(sizeof(uint64_t));
	snprintf(msg->data, MAX_SIGNAL_BUF, "|bootstrap |%u|%lu", getpid(),
		 (uintptr_t)libfs_rate_limit_flag);
#else
	snprintf(msg->data, MAX_SIGNAL_BUF, "|bootstrap |%u", getpid());
#endif
	mlfs_printf("DEBUG sockfd %d g_rpc_socks[sockfd] %p, msg '%s'\n", sockfd, g_rpc_socks[sockfd], msg->data);
	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
        pr_rpc("send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);

	//spin till we receive a response with the same sequence number
	IBV_AWAIT_RESPONSE(sockfd, msg->id);
#else
	// undefined code path
	mlfs_assert(false);
#endif
}

int rpc_bootstrap_response(int sockfd, uint64_t seq_n)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	snprintf(msg->data, MAX_SIGNAL_BUF, "|bootstrap |%u", g_rpc_socks[sockfd]->peer->id);
	msg->id = seq_n; //set immediate to sequence number in order for requester to match it

	pr_rpc("send: %s seqn=%lu", msg->data, msg->id);
	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
}

///**
// * g_sync_ctx[]->peer->local_start address is passed to the libfs
// * It is used by Hyperloop to gWrite metadata.
// */
//int rpc_bootstrap_response_with_hyperloop_meta(int sockfd, uint64_t seq_n)
//{
//	struct app_context *msg;
//	int buffer_id = rc_acquire_buffer(sockfd, &msg);
//	int libfs_id;
//
//	libfs_id = g_rpc_socks[sockfd]->peer->id;
//
//	snprintf(msg->data, MAX_SIGNAL_BUF, "|bootstrap |%u|%lu",
//		libfs_id,
//		(uintptr_t)&g_sync_ctx[libfs_id]->peer->local_start);
//	msg->id = seq_n; //set immediate to sequence number in order for requester to match it
//
//	pr_rpc("send: %s seqn=%lu", msg->data, msg->id);
//	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
//}

void rpc_register_log(int sockfd, struct peer_id *peer, uint64_t *ack_bit_p)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);
	char temp_str[900];

	strncpy(temp_str, peer->ip, 900); // truncate string to work around warning.
        snprintf(msg->data, MAX_SIGNAL_BUF, "|peer |%d|%u|%lu|%s", peer->id,
                 peer->pid, (uintptr_t)ack_bit_p, temp_str);
        msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
        pr_rpc("send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);
}

void rpc_register_remote_libfs_peer(int sockfd, struct peer_id *peer,
			      uint64_t *ack_bit_p)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);
	char temp_str[900];

	strncpy(temp_str, peer->ip, 900); // truncate string to work around
					  // warning.
	snprintf(msg->data, MAX_SIGNAL_BUF,
		 "|" TO_STR(RPC_REGISTER_REMOTE_LIBFS_PEER) " |%d|%u|%lu|%s", peer->id,
		 peer->pid, (uintptr_t)ack_bit_p, temp_str);
	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
	pr_rpc("send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);
}

#ifdef NIC_SIDE
/**
 * @Synopsis  Send kernfs meta data to the next replica.
 *
 * @Param sockfd
 */
void rpc_send_pipeline_kernfs_meta_to_next_replica(int sockfd)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = 0;
	snprintf(msg->data, MAX_SIGNAL_BUF,
		 "|" TO_STR(RPC_PIPELINE_KERNFS_META) " |%lu",
		 (uintptr_t)primary_rate_limit_flag);
	pr_rpc("send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
}
#endif

int rpc_register_log_response(int sockfd, uint64_t seq_n)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	snprintf(msg->data, MAX_SIGNAL_BUF, "|peer |registered");
	msg->id = seq_n; //set immediate to sequence number in order for requester to match it
        pr_rpc("send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);

	return 0;
}

struct mlfs_reply * rpc_get_reply_object(int sockfd, uint8_t *dst, uint64_t seqn)
{
	struct mlfs_reply *reply = mlfs_zalloc(sizeof(struct mlfs_reply));
	reply->remote = 1; //signifies remote destination buffer
	reply->sockfd = sockfd;
	reply->seqn = seqn;
	reply->dst = dst;

	return reply;
}

void rpc_await(struct rpc_pending_io *pending)
{
	if(pending->type == RPC_PENDING_WC) {
#ifdef NIC_OFFLOAD
	    // TODO consider whether it uses low_lat channel. If it does,
	    // IBV_AWAIT_WORK_COMPLETION_WITH_POLLING might be required.
	    mlfs_assert("Not considered this path with NIC offloading.\n");
#endif
		IBV_AWAIT_WORK_COMPLETION(pending->sockfd, pending->seq_n);
	}
	else
		IBV_AWAIT_RESPONSE(pending->sockfd, pending->seq_n);
}

//void rpc_sync_superblock(int devid)
//{
//        char cmd[MAX_SIGNAL_BUF];
//        memset(cmd, 0, MAX_SOCK_BUF);
//        sprintf(cmd, "|sqsb |%d|", devid);
//
//        // TODO if struct data cannot be sent as a single msg, implement a function to
//        // send attributes of the struct one by one.
//        rpc_forward_msg(g_kernfs_peers[g_kernfs_id + 1]->sockfd[SOCK_BG], cmd);
//}

//void rpc_sync_inode(int inum)
//{
//	printf("WARNING %s has not been implemented. inum:%d\n", __func__, inum);
//}

/**
 * Send message and wait until getting response.
 * Args:
 *      sync: If 1, wait for response. If 0, wait for completion.
 */
int rpc_nicrpc_msg(int sockfd, char *data, bool sync)
{
        struct app_context *msg;
        uint64_t seq_n;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

        // Set seq_n.
        seq_n = generate_rpc_seqn(g_rpc_socks[sockfd]);
        msg->id = seq_n;

	snprintf(msg->data, MAX_SIGNAL_BUF, "%s", data);
        pr_rpc("peer send: %s\n", msg->data);

        if (sync) {
            // wait for response.
            IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);
            //spin till we receive a response with the same sequence number
            IBV_AWAIT_RESPONSE(sockfd, seq_n);  // Wait for decoded seq_n.
        } else {
            // wait for completion only.
            IBV_WRAPPER_SEND_MSG_SYNC(sockfd, buffer_id, 1);
            // IBV_AWAIT_WORK_COMPLETION(sockfd, seq_n);
        }
	return 0;
}

/**
 * Setup NIC RPC response.
 */
int rpc_nicrpc_setup_response(int sockfd, char *data, uint64_t seq_n)
{
        struct app_context *resp_msg;
	int buffer_id = rc_acquire_buffer(sockfd, &resp_msg);
        int len;

	snprintf(resp_msg->data, MAX_SIGNAL_BUF, "%s%lu|", data, seq_n);
        len = strlen(resp_msg->data);
        assert(len <= MAX_SIGNAL_BUF); // data size exceeds MAX_SIGNAL_BUF

	resp_msg->id = seq_n; //set immediate to sequence number in order for requester to match it

        mlfs_info("[NIC RPC] setup resp: %s, buffer_id:%d, seq_n:%lu\n", resp_msg->data, buffer_id, seq_n);
        return buffer_id;
}

/**
 * Send NIC RPC response.
 * rpc_nicrpc_setup_response should be called before calling this function.
 */
void rpc_nicrpc_send_response(int sockfd, int buffer_id)
{
        mlfs_info("[NIC RPC] send resp: buffer_id:%d\n", buffer_id);
	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
}

uint64_t *local_bit_val = 0;
/**
 * @Synopsis  Set ack bit on the remote machine using RDMA WRITE. It doesn't
 * wait WR completion.
 *
 * @Param sockfd
 * @Param remote_bit_addr
 */
void rpc_set_remote_bit(int sockfd, uintptr_t remote_bit_addr)
{
	rdma_meta_t *meta;

	if (!local_bit_val) {
		local_bit_val = nic_slab_alloc_in_byte(sizeof(uint64_t));
		*local_bit_val = 1;
	}

	// mlfs_printf("Set remote bit: remote_bit_addr=%lu(0x%lx)\n",
	// remote_bit_addr,
	//         remote_bit_addr);

	mlfs_assert(*local_bit_val == 1);

	meta = create_rdma_meta((uintptr_t)local_bit_val, remote_bit_addr,
				sizeof(uint64_t));
	START_TIMER(evt_set_remote_bit1);
	// IBV_WRAPPER_WRITE_ASYNC(sockfd, meta, MR_DRAM_BUFFER, MR_DRAM_BUFFER);
	IBV_WRAPPER_WRITE_SYNC(sockfd, meta, MR_DRAM_BUFFER, MR_DRAM_BUFFER);
	END_TIMER(evt_set_remote_bit1);

	// nic_slab_free(local_bit_val); // TODO Is it okay to free it right after
				      // ASYNC CALL?
	mlfs_free(meta);
}

/**
 * @Synopsis  Write 8byte value to remote memory via RDMA write.
 *
 * @Param sockfd
 * @Param remote_addr
 * @Param val value to be written.
 */
void rpc_write_remote_val64(int sockfd, uintptr_t remote_addr, uint64_t val)
{
	uint64_t *local_bit_val;
	rdma_meta_t *meta;

	local_bit_val = nic_slab_alloc_in_byte(sizeof(uint64_t));
	*local_bit_val = val;

	// mlfs_printf("Set remote bit: remote_addr=%lu(0x%lx)\n",
	// remote_addr, remote_addr);

	meta = create_rdma_meta((uintptr_t)local_bit_val, remote_addr,
				sizeof(uint64_t));
	IBV_WRAPPER_WRITE_ASYNC(sockfd, meta, MR_DRAM_BUFFER, MR_DRAM_BUFFER);

	nic_slab_free(local_bit_val); // TODO Is it okay to free it right after
				      // ASYNC CALL?
	mlfs_free(meta);
}

/**
 * @Synopsis  Write 8 bits value to remote memory via RDMA write.
 *
 * @Param sockfd
 * @Param remote_addr
 * @Param val value to be written.
 */
void rpc_write_remote_val8(int sockfd, uintptr_t remote_addr, uint8_t val)
{
	uint64_t *local_bit_val;
	rdma_meta_t *meta;

	local_bit_val = nic_slab_alloc_in_byte(sizeof(uint64_t));
	*local_bit_val = 0;
	*local_bit_val = val;

	// mlfs_printf("Set remote bit: remote_addr=%lu(0x%lx)\n",
	// remote_addr, remote_addr);

	meta = create_rdma_meta((uintptr_t)local_bit_val, remote_addr, 1);
	IBV_WRAPPER_WRITE_ASYNC(sockfd, meta, MR_DRAM_BUFFER, MR_DRAM_BUFFER);

	nic_slab_free(local_bit_val); // TODO Is it okay to free it right after
				      // ASYNC CALL?
	mlfs_free(meta);
}

/**
 * Alloc ack bit in DRAM_MR
 * Returns the pointer to the allocated ack_bit.
 */
uint64_t *rpc_alloc_ack_bit(void)
{
    uint64_t *ack_bit_p;

    ack_bit_p = nic_slab_alloc_in_byte(sizeof(uint64_t));
    *ack_bit_p = (uint64_t)0;

    return ack_bit_p;
}

/**
 * Wait until the ack_bit is set to 1.
 * Free ack_bit before return.
 */
void rpc_wait_ack(uint64_t *ack_bit_p)
{
    while (*ack_bit_p == 0) {
	cpu_relax();
    }

    nic_slab_free(ack_bit_p);
}

/**
 * @Param ack_bit_p
 *
 * @Returns   Returns 1 if ack arrived.
 */
int rpc_check_ack_bit(uint64_t *ack_bit_p)
{
	return *ack_bit_p;
}

void rpc_free_ack_bit(uint64_t *ack_bit_p)
{
	nic_slab_free(ack_bit_p);
}

int rpc_request_host_memcpy_buffer_addr(int sockfd)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
	snprintf(msg->data, MAX_SIGNAL_BUF,
		 "|" TO_STR(RPC_REQ_MEMCPY_BUF) " |");

	pr_rpc("peer send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);

	// spin till we receive a response with the greater or equal sequence
	// number.
	IBV_AWAIT_RESPONSE(sockfd, msg->id);

	return 0;
}

#ifdef NO_BUSY_WAIT
void rpc_send_replicate_ack(int sockfd, uint64_t ack_bit_addr) {
	char msg[MAX_SOCK_BUF];
	sprintf(msg, "|" TO_STR(RPC_REPLICATE_ACK) " |%lu|", ack_bit_addr);
	rpc_forward_msg_no_seqn(sockfd, msg);
}
#endif


int rpc_send_host_memcpy_buffer_addr(int sockfd, uintptr_t buf_addr)
{
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = generate_rpc_seqn(g_rpc_socks[sockfd]);
	snprintf(msg->data, MAX_SIGNAL_BUF,
		 "|" TO_STR(RPC_MEMCPY_BUF_ADDR) " |%lu|", buf_addr);

	pr_rpc("peer send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);

	return 0;
}

void rpc_send_heartbeat(int sockfd, uint64_t seqn) {
	struct app_context *msg;
	int buffer_id = rc_acquire_buffer(sockfd, &msg);

	msg->id = 0;
	snprintf(msg->data, MAX_SIGNAL_BUF,
		 "|" TO_STR(RPC_HEARTBEAT) " |%lu|", seqn);

	pr_rpc("peer send: %s seqn=%lu sock=%d\n", msg->data, msg->id,
		    sockfd);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);
}




//void rpc_send_memcpy_response(int sockfd, uint64_t seqn, int libfs_id)
//{
//	struct app_context *msg;
//	int buffer_id = rc_acquire_buffer(sockfd, &msg);
//
//	msg->id = seqn;
//	msg->libfs_id = libfs_id;
//	sprintf(msg->data, "|" TO_STR(RPC_MEMCPY_COMPLETE) " |");
//
//	pr_rpc("peer send: %s seqn=%lu sock=%d", msg->data, msg->id, sockfd);
//	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 1);
//}



void print_peer_id(struct peer_id *peer)
{
    if (!peer){
	pr_setup("%s", "---- Peer is NULL ----");
    } else {
	pr_setup("---- Peer %d Info ----", peer->id);
	pr_setup("IP: %s\n", peer->ip);
	for(int j=0; j<SOCK_TYPE_COUNT; j++) {
		pr_setup("SOCKFD[%d] = %d", j, peer->sockfd[j]);
	}
	pr_setup("%s", "---------------------");
    }
}

void print_rpc_setup(void)
{
    pr_setup("%s", "*************************************");
#ifdef NIC_SIDE
    pr_setup("myIntf=%s", mlfs_conf.arm_net_interface_name);
#else
    pr_setup("myIntf=%s", mlfs_conf.x86_net_interface_name);
#endif
    pr_setup("g_self_ip=%s", g_self_ip);
    pr_setup("g_node_id=%d", g_node_id);
    pr_setup("g_host_node_id=%d", g_host_node_id);
    pr_setup("g_self_id=%d", g_self_id);
    pr_setup("g_kernfs_id=%d", g_kernfs_id);
    pr_setup("%s", "*************************************");
}

#endif
