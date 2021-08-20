#include <time.h>
#include <signal.h>
#include "master.h"

volatile sig_atomic_t stop;
uint64_t LOG_SIZE =  268265456UL; //256 MB

void inthand(int signum)
{	
	stop = 1;
}

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

void signal_callback(char* data)
{
	printf("received the following signal: %s\n", data);
}

int main(int argc, char **argv)
{
	char *host;
	char *port_no = "12345";
	void *mem_reg;
	rdma_meta_t *meta;
	int write_size;
	int iters;
	int sge;
	int to_sync_size;
	int remaining_size;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <slave-address>\n", argv[0]);
		return 1;
	}

	posix_memalign(&mem_reg, sysconf(_SC_PAGESIZE), LOG_SIZE);
		/*typedef struct rdma_metadata {
			addr_t address;
			addr_t total_len;
			int sge_count;
			struct ibv_sge sge_entries[];
		} rdma_meta_t;*/

	init_master((uintptr_t) mem_reg, LOG_SIZE, 256, argv[1], port_no, signal_callback);

 	signal(SIGINT, inthand);

 	while(!stop) {
		sleep(1);
	}
#if 0
	meta = (rdma_meta_t *) malloc(sizeof(rdma_meta_t)
			+ sge * sizeof(struct ibv_sge));

	meta->address = get_peer_start_addr() + 20000000UL;
	meta->total_len = write_size;
	meta->sge_count = sge;

	remaining_size = write_size;
	to_sync_size = max(remaining_size/sge,1);
	for(int i=0; i<sge; i++) {
		if(to_sync_size == 0)
			break;
		meta->sge_entries[i].addr = (uintptr_t) mem_reg + (write_size - remaining_size) + 1; 
		meta->sge_entries[i].length = to_sync_size;
		remaining_size -= to_sync_size;
		if(i == sge-2)
			to_sync_size = remaining_size;
		else
			to_sync_size = min(to_sync_size, remaining_size);
	}

	//printf("rdma_entry->meta->address = %lu \n", rdma_entry->meta->address);
	//printf("rdma_entry->meta->total_len = %lu \n", (sync->intervals->n_blk << g_block_size_shift));

	struct timespec start_time = timer_start();

	for(int i=0; i<iters; i++) {
		//struct timespec start_time = timer_start();
 		master_sync_await(meta);
		//long time_elapsed_nanos = timer_end(start_time);
		//printf("optime [us] %f\n", time_elapsed_nanos/1e3);
	}
	//wait_for_n_completions(iters);

	long time_elapsed_nanos = timer_end(start_time);
	printf("RDMA Write complete: Ops %d, runtime [ms] %f, op-latency [us] %f\n", iters,
			time_elapsed_nanos/1e6, time_elapsed_nanos/iters/1e3);

	free(meta);
	free(mem_reg);

#endif
	return 0;
}
