#include <time.h>
#include "time_stat.h"
#include "master.h"

int BUFFER_COUNT = 0;
uint64_t LOG_SIZE =  268265456UL; //256 MB
uint64_t BUFFER_SIZE = 8388608UL; //8 MB  

// call this function to start a nanosecond-resolution timer
struct timespec timer_start(){
	struct timespec start_time;
	clock_gettime(CLOCK_REALTIME, &start_time);
	return start_time;
}

// call this function to end a timer, returning nanoseconds elapsed as a long
long timer_end(struct timespec start_time) {
	struct timespec end_time;
	long sec_diff, nsec_diff, nanoseconds_elapsed;

	clock_gettime(CLOCK_REALTIME, &end_time);

	sec_diff =  end_time.tv_sec - start_time.tv_sec;
	nsec_diff = end_time.tv_nsec - start_time.tv_nsec;

	if(nsec_diff < 0) {
		sec_diff--;
		nsec_diff += (long)1e9;
	}

	nanoseconds_elapsed = sec_diff * (long)1e9 + nsec_diff;

	return nanoseconds_elapsed;
}

double test(struct timespec start)
{
	struct timespec finish;
	clock_gettime(CLOCK_REALTIME, &finish);
 	long seconds = finish.tv_sec - start.tv_sec; 
     	long ns = finish.tv_nsec - start.tv_nsec; 
         
         if (start.tv_nsec > finish.tv_nsec) { // clock underflow 
	 	--seconds; 
	 	ns += 1000000000; 
	     }
	return (double)seconds + (double)ns/(double)1e9;
}

// Generates and prints 'count' random 
// numbers in range [lower, upper]. 
uint64_t get_random(uint64_t lower, uint64_t upper) 
{ 
	return (rand() %  (upper - lower + 1)) + lower; 
} 

void signal_callback(struct app_context *msg)
{
	printf("received the following signal: %s\n", msg->data);
}

int main(int argc, char **argv)
{
	char *host;
	char *port_no = "12345";
	struct mr_context *regions;
	void *mem;
	rdma_meta_t *meta;
	int write_size;
	int iters;
	int sge;
	int to_sync_size;
	int remaining_size;
	int sockfd;

	struct time_stats *sync_timer = (struct time_stats*) malloc(sizeof(struct time_stats));

	if (argc != 5) {
		fprintf(stderr, "usage: %s <slave-address> <write-size> <sge> <iters> \n", argv[0]);
		return 1;
	}


	write_size = atoi(argv[2]);
	sge = atoi(argv[3]);
	iters = atoi(argv[4]);

	if(write_size > LOG_SIZE) {
		printf("invalid write size; must be less than LOG_SIZE\n");
		return 1;
	}

	time_stats_init(sync_timer, iters);

	regions = (struct mr_context *) calloc(BUFFER_COUNT+1, sizeof(struct mr_context));

	//allocate memory
	posix_memalign(&mem, sysconf(_SC_PAGESIZE), LOG_SIZE);
	regions[0].type = 0;
	regions[0].addr = (uintptr_t) mem;
        regions[0].length = LOG_SIZE;	

	for(int i=1; i<BUFFER_COUNT+1; i++) {
		posix_memalign(&mem, sysconf(_SC_PAGESIZE), BUFFER_SIZE);
		regions[i].type = i;
		regions[i].addr = (uintptr_t) mem;
        	regions[i].length = BUFFER_SIZE;	
	}
		/*typedef struct rdma_metadata {
			addr_t address;
			addr_t total_len;
			int sge_count;
			struct ibv_sge sge_entries[];
		} rdma_meta_t;*/	
 	
	init_master(regions, BUFFER_COUNT+1, 256, signal_callback);
	sockfd = add_connection(argv[1], port_no);

	while(!rc_ready(sockfd)) {
        	asm("");
	}

	uint64_t local_addr[sge];
	uint64_t remote_addr = mr_remote_addr(sockfd, 0);

	//write something to the log
	meta = (rdma_meta_t *) malloc(sizeof(rdma_meta_t)
			+ sge * sizeof(struct ibv_sge));

	//meta->addr = mr_remote_addr(sockfd, 0) + 10000000UL; //some offset (< LOG_SIZE)
	meta->addr = remote_addr;
	meta->length = write_size;
	meta->sge_count = sge;

	remaining_size = write_size;
	to_sync_size = max(remaining_size/sge,1);
	for(int i=0; i<sge; i++) {
		if(to_sync_size == 0)
			break;
		local_addr[i] = regions[0].addr + (write_size - remaining_size) + 1;
		meta->sge_entries[i].addr = local_addr[i];
		meta->sge_entries[i].length = to_sync_size;
		printf("sge[%d].addr = %lx, sge[%d].length = %u\n", i, meta->sge_entries[i].addr, i, meta->sge_entries[i].length);
		remaining_size -= to_sync_size;
		if(i == sge-2)
			to_sync_size = remaining_size;
		else
			to_sync_size = min(to_sync_size, remaining_size);
	}

	//printf("rdma_entry->meta->address = %lu \n", rdma_entry->meta->address);
	//printf("rdma_entry->meta->total_len = %lu \n", (sync->intervals->n_blk << g_block_size_shift));

	//sleep(2);

	//struct timespec start_time = timer_start();

	for(int i=0; i<iters; i++) {
 		//master_sync_await(meta);
		for(int i=0; i<sge; i++) {
			meta->sge_entries[i].addr = local_addr[i] + get_random(0, LOG_SIZE - write_size);
		}

		meta->addr = remote_addr + get_random(0, LOG_SIZE - write_size);
		time_stats_start(sync_timer);
		master_sync_await(meta);
		time_stats_stop(sync_timer);
	}

	//spin_till_all_work_completed(0);

	//wait_for_n_completions(iters);

	//long time_elapsed_nanos = timer_end(start_time);
	//printf("RDMA Write complete: Ops %d, runtime [ms] %f, op-latency [us] %f\n", iters,
	//		time_elapsed_nanos/1e6, time_elapsed_nanos/iters/1e3);

	time_stats_print(sync_timer, "RDMA Write Complete");

	struct app_context *app;
	int buffer_id = rc_acquire_buffer(0, &app);

	app->id = 7;
	char* data = "finished";
	snprintf(app->data, msg_size, "%s", data);

	IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);

	//sleep(1);

	free(meta);
	free(mem);


	return 0;
}
