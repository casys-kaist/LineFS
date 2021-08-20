#include "mlfs/mlfs_user.h"
#include "storage/storage.h"
#include "limit_rate.h"
#include "distributed/rpc_interface.h"

rt_bw_stat prefetch_rt_bw = {0};

atomic_uint rate_limit_on;
uint64_t *primary_rate_limit_flag;
uint64_t primary_rate_limit_addr = 0;

void init_prefetch_rate_limiter(void)
{
	init_rt_bw_stat(&prefetch_rt_bw, "prefetch");
}

void init_rate_limiter(void)
{
	atomic_init(&rate_limit_on, 0);

	if (!primary_rate_limit_flag) {
		primary_rate_limit_flag =
			(uint64_t *)nic_slab_alloc_in_byte(sizeof(uint64_t));
		*primary_rate_limit_flag = (uint64_t)0;
	}
}

// TODO when is it called?
void destroy_rate_limiter(void)
{
	if (true_bit)
		nic_slab_free(true_bit);

	if (false_bit)
		nic_slab_free(false_bit);

	if (primary_rate_limit_flag)
		nic_slab_free(primary_rate_limit_flag);
}

int nic_slab_full(void)
{
	size_t used_pct;
	used_pct = get_nic_slab_used_pct();

	// if (used_pct < 100 && // Filtering noise... ncx slab has some bug.
	if (used_pct < 99 && // Filtering noise... ncx slab has some bug.
	    used_pct > (uint32_t)mlfs_conf.nic_slab_threshold_high) {
		mlfs_printf("nic_slab_full! used=%lu%% high_thres=%u%%\n",
			    used_pct, mlfs_conf.nic_slab_threshold_high);
		return 1;
	} else
		return 0;
}

int nic_slab_empty(void)
{
	size_t used_pct;
	used_pct = get_nic_slab_used_pct();

	if (used_pct < (uint32_t)mlfs_conf.nic_slab_threshold_low) {
		mlfs_printf("nic_slab_empty! used_pct=%lu%% low_thres=%u%%\n",
			    used_pct, mlfs_conf.nic_slab_threshold_low);

		return 1;
	} else
		return 0;
}

static void set_rate_limit_flag(uintptr_t target_addr, int sockfd)
{
	rdma_meta_t *set_rate_limit_meta;
	mlfs_printf(ANSI_COLOR_RED "Set rate limit flag. "
				   "addr=0x%lx" ANSI_COLOR_RESET "\n",
		    target_addr);

	mlfs_assert(*true_bit == 1);

	set_rate_limit_meta = create_rdma_meta((uintptr_t)true_bit, target_addr,
					       sizeof(uint64_t));

	IBV_WRAPPER_WRITE_ASYNC(sockfd, set_rate_limit_meta, MR_DRAM_BUFFER,
				MR_DRAM_BUFFER);
	mlfs_free(set_rate_limit_meta); // TODO Is it okay?
}

static void unset_rate_limit_flag(uintptr_t target_addr, int sockfd)
{
	rdma_meta_t *unset_rate_limit_meta;
	mlfs_printf(ANSI_COLOR_RED "Clear rate limit flag. "
				   "addr=0x%lx" ANSI_COLOR_RESET "\n",
		    target_addr);

	mlfs_assert(*false_bit == 0);
	unset_rate_limit_meta = create_rdma_meta((uintptr_t)false_bit,
						 target_addr, sizeof(uint64_t));

	IBV_WRAPPER_WRITE_ASYNC(sockfd, unset_rate_limit_meta, MR_DRAM_BUFFER,
				MR_DRAM_BUFFER);
	mlfs_free(unset_rate_limit_meta); // TODO Is it okay?
}

// It is called only at Primary.
void limit_all_libfs_req_rate(void)
{
	int sockfd;
	uintptr_t dst;
	uint32_t flag;
	int limit_on = 0;

	for (int i = g_n_nodes; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		if (!g_sync_ctx[i] || !is_first_kernfs(i, g_kernfs_id))
			continue;

		mlfs_printf("i=%d g_sync_ctx[%d]=%p\n", i, i, g_sync_ctx[i]);
		set_rate_limit_flag(g_sync_ctx[i]->libfs_rate_limit_addr,
				    g_peers[i]->sockfd[SOCK_BG]);
		limit_on = 1;
	}

	if (limit_on)
		atomic_store(&rate_limit_on, 1);
}

void unlimit_all_libfs_req_rate(void)
{
	int sockfd;
	int limit_off = 0;

	for (int i = g_n_nodes; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		if (!g_sync_ctx[i] || !is_first_kernfs(i, g_kernfs_id))
			continue;

		mlfs_printf("i=%d g_sync_ctx[%d]=%p\n", i, i, g_sync_ctx[i]);
		unset_rate_limit_flag(g_sync_ctx[i]->libfs_rate_limit_addr,
				      g_peers[i]->sockfd[SOCK_BG]);
		limit_off = 1;
	}
	if (limit_off)
		atomic_store(&rate_limit_on, 0);
}

/**
 * @Synopsis  Rate the limit of libfs prefetch request.
 *
 * @Param arg
 */
void rate_limit_worker(void *arg)
{
	struct timespec start, end;
	int limited;
	double dur;

	while (1) {
		limited = atomic_load(&rate_limit_on);
		if (!limited && nic_slab_full()) {
			// A machine can be primary and replica at the same time.
			limit_all_libfs_req_rate(); // to libfs
			limit_req_rate_of_primary(); // to primary

		} else if (limited) {
			clock_gettime(CLOCK_MONOTONIC, &end);

			if (nic_slab_empty()) {
				unlimit_all_libfs_req_rate(); // to libfs.
				unlimit_req_rate_of_primary(); // to primary.
				clock_gettime(CLOCK_MONOTONIC, &start);
			} else {
				dur = get_duration(&start, &end);
				if (dur > 1.0) {
					printf("LibFSes's rates are limited. Used: "
					       "%lu%%\n",
					       get_nic_slab_used_pct());
					nic_slab_print_stat();
					// printf("%s","LibFSes's rates are limited.\n");
					clock_gettime(CLOCK_MONOTONIC, &start);
				}
			}
		}
		// else if (limited && nic_slab_empty()) {
		//         clock_gettime(CLOCK_MONOTONIC, &end);
		//         dur = get_duration(&start, &end);
		//         if (dur > 1.0) {
		//                 // printf("LibFSes's rates are limited. Used: "
		//                 //        "%lu%%\n",
		//                 //        get_nic_slab_used_pct());
		//                 printf("%s","LibFSes's rates are limited.\n");
		//                 clock_gettime(CLOCK_MONOTONIC, &start);
		//         }

		//         unlimit_all_libfs_req_rate(); // to libfs.
		//         unlimit_req_rate_of_primary(); // to primary.
		// }
		// To previous kernfs. (Mostly Primary)


		usleep(10); // TODO proper value??
	}
}

/**
 * @Synopsis  This function is called by Replica 1.
 * Note, Primary or Replica 1 becomes a bottleneck.
 *
 * @Param libfs_id
 */
static void limit_req_rate_of_primary(void)
{
	int prev_kernfs_id;
	int sockfd;
	uintptr_t dst;

	prev_kernfs_id = get_prev_kernfs(g_kernfs_id);
	sockfd = g_peers[prev_kernfs_id]->sockfd[SOCK_BG];
	dst = primary_rate_limit_addr;

	// No libfs on the primary yet. Note that all machines can be primary.
	if (!dst)
		return;

	set_rate_limit_flag(dst, sockfd);

	atomic_store(&rate_limit_on, 1);
}

static void unlimit_req_rate_of_primary(void)
{
	int prev_kernfs_id;
	int sockfd;
	uintptr_t dst;

	prev_kernfs_id = get_prev_kernfs(g_kernfs_id);
	sockfd = g_peers[prev_kernfs_id]->sockfd[SOCK_BG];
	dst = primary_rate_limit_addr;

	// No libfs on the primary yet. Note that all machines can be primary.
	if (!dst)
		return;

	unset_rate_limit_flag(dst, sockfd);

	atomic_store(&rate_limit_on, 0);
}

/**
 * @brief Update and retrieve the amount of data sent during this epoch (1
 * second).
 *
 * @param stat
 * @param sent_bytes
 * @return uint64_t Time to sleep if the current bandwidth usage exceeds a given
 * data cap. Otherwise, 0.
 */
static uint64_t update_prefetch_bw_usage(rt_bw_stat *stat, uint64_t sent_bytes)
{
	uint64_t ret = 0;
	uint64_t cur_usage = 0;
	uint64_t time_elapsed_ns = 0;
	const uint64_t one_second = 1000000000;

	if (!stat) {
		printf("[Warn] Real time bandwidth stat is not allocated.\n");
		return 0;
	}

	pthread_spin_lock(&stat->lock);

	// First run.
	if (stat->start_time.tv_sec == 0) {
		clock_gettime(CLOCK_MONOTONIC, &stat->start_time);
	}

	clock_gettime(CLOCK_MONOTONIC, &stat->end_time);
	time_elapsed_ns =
		get_duration_in_ns(&stat->start_time, &stat->end_time);

	// Reset after an epoch ends.
	if (time_elapsed_ns > one_second) {
		printf("[RT_BW %s]: %lu MB/s\n", stat->name,
		       stat->bytes_until_now >> 20);

		// Reset stat.
		clock_gettime(CLOCK_MONOTONIC, &stat->start_time);
		stat->bytes_until_now = 0;
	}

	stat->bytes_until_now += sent_bytes;
	cur_usage = stat->bytes_until_now;

	pthread_spin_unlock(&stat->lock);

	// printf("cur_usage=%lu threshold=%lu\n", cur_usage,
	// (uint64_t)mlfs_conf.prefetch_data_cap * (1024 * 1024));

	if (cur_usage >
	    (uint64_t)mlfs_conf.prefetch_data_cap * (1024 * 1024) /* MB to Bytes */) {
		// Get the remaining time within an epoch in nanoseconds.
		ret = one_second - time_elapsed_ns;
		printf(ANSI_COLOR_RED "[PREFETCH_RATE_LIMIT] cur_usage(MB)=%lu "
				      "remaining_time=%lf\n" ANSI_COLOR_RESET,
		       cur_usage / (1024 * 1024),
		       ((double)ret) / (double)one_second);
	}

	return ret;
}

/**
 * @brief Limit the rate of fetching data from local PM in Primary (Prefetch).
 * It is required due to the difference between PCIe (15.75GB/s) and network
 * bandwidths (25Gbps, BlueField1).
 *
 * @param libfs_id
 * @param seqn
 */
void limit_prefetch_rate(uint64_t sent_bytes)
{
	uint64_t sleep_time_ns;

	sleep_time_ns = update_prefetch_bw_usage(&prefetch_rt_bw, sent_bytes);

	if (sleep_time_ns) {
		// Sleep for the remaining period of this epoch.
		struct timespec time_to_sleep = {0, sleep_time_ns};
		if (nanosleep(&time_to_sleep, NULL)) {
			printf("[Error] nanosleep failed.\n");
		}
	}
}
