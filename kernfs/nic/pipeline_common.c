#include "mlfs/mlfs_user.h"
#include "pipeline_common.h"
#include "global/util.h"

host_heartbeat_t g_host_heartbeat = {0};
uint64_t *true_bit = 0;
uint64_t *false_bit = 0;

void init_pipeline_common(void) {
	if (!true_bit) {
		true_bit = (uint64_t *)nic_slab_alloc_in_byte(sizeof(uint64_t));
		*true_bit = (uint64_t)1;
	}
	if (!false_bit) {
		false_bit =
			(uint64_t *)nic_slab_alloc_in_byte(sizeof(uint64_t));
		*false_bit = (uint64_t)0;
	}
}

/**
 * @Synopsis  Set fetch_done flag of previous replica (or primary) by
 * RDMA write. It is to free loghdr_buf of previous replica (or primary).
 *
 * @Param fetch_done_addr
 * @Param sockfd
 */
void set_fetch_done_flag(int libfs_id, uint64_t seqn, uintptr_t fetch_done_addr,
			 int sockfd)
{
	// uint64_t *done_flag;
	rdma_meta_t *done_meta;

	// done_flag = (uint64_t *)nic_slab_alloc_in_byte(sizeof(uint64_t));
	// *done_flag = 1; // Set to 1.

	pr_pipe("Set fetch_done_flag libfs_id=%d seqn=%lu remote_addr=0x%lx",
		libfs_id, seqn, fetch_done_addr);

	// TODO to be deleted.
	mlfs_assert(*true_bit == 1);

	done_meta = create_rdma_meta((uintptr_t)true_bit, fetch_done_addr,
				     sizeof(uint64_t));

	IBV_WRAPPER_WRITE_ASYNC(sockfd, done_meta, MR_DRAM_BUFFER,
				MR_DRAM_BUFFER);
	mlfs_free(done_meta);
}

void set_fetch_done_flag_to_val(int libfs_id, uint64_t seqn,
				 uintptr_t fetch_done_addr, uint64_t val,
				 int sockfd)
{
	uint64_t *src_val;
	rdma_meta_t *done_meta;

	src_val = (uint64_t *)nic_slab_alloc_in_byte(sizeof(uint64_t));
	*src_val = val;

	pr_pipe("Set fetch_done_flag libfs_id=%d seqn=%lu remote_addr=0x%lx "
		"val=0x%lx",
		libfs_id, seqn, fetch_done_addr, val);

	done_meta = create_rdma_meta((uintptr_t)src_val, fetch_done_addr,
				     sizeof(uint64_t));

	IBV_WRAPPER_WRITE_ASYNC(sockfd, done_meta, MR_DRAM_BUFFER,
				MR_DRAM_BUFFER);
	mlfs_free(done_meta);
	nic_slab_free(src_val); // XXX Is it okay to free right after ASYNC call?
}

void print_thread_init(int th_num, char *th_name)
{
	mlfs_printf("THREAD_INIT %30s %d\n", th_name, th_num);
}

void update_host_heartbeat(uint64_t arrived_seqn) {
	if (arrived_seqn > g_host_heartbeat.seqn){
		g_host_heartbeat.seqn = arrived_seqn;
		clock_gettime(CLOCK_MONOTONIC, &g_host_heartbeat.last_recv_time);
		// mlfs_printf("Update heartbeat info: seqn=%lu\n",
		//             g_host_heartbeat.seqn);
	} else {
		mlfs_printf("Stale heartbeat arrived. Ignore it. seqn=%lu "
		       "current=%lu\n",
		       arrived_seqn, g_host_heartbeat.seqn);
	}
}

/**
 * @Returns 1 if its okay. 0 if we haven't received a heartbeat for one second.
 */
static int check_host_heartbeat(void)
{
	struct timespec cur;
	double dur;

	clock_gettime(CLOCK_MONOTONIC, &cur);
	dur = get_duration(&g_host_heartbeat.last_recv_time, &cur);

	// mlfs_printf("heartbeat duration: %.02f seconds.\n", dur);

	return dur <= 1.0;
}

// Check host is alive every 1 sec.
void check_heartbeat_worker(void *arg)
{
	while (1) {
		g_host_heartbeat.alive = check_host_heartbeat();
		// printf("Host is %d\n", g_host_heartbeat.alive);
		sleep(1);
	}
}