#include <time.h>
#include "slave.h"

uint64_t LOG_SIZE =  268265456UL; //256 MB

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
	int read_size;
	int iters;
	int sge;
	int to_sync_size;
	int remaining_size;

	if (argc != 4) {
		fprintf(stderr, "usage: %s <read-size> <sge> <iters> \n", argv[0]);
		return 1;
	}


	read_size = atoi(argv[1]);
	sge = atoi(argv[2]);
	iters = atoi(argv[3]);

	posix_memalign(&mem_reg, sysconf(_SC_PAGESIZE), LOG_SIZE);
		/*typedef struct rdma_metadata {
			addr_t address;
			addr_t total_len;
			int sge_count;
			struct ibv_sge sge_entries[];
		} rdma_meta_t;*/	
 	
	init_slave((uintptr_t) mem_reg, LOG_SIZE, 256, port_no, signal_callback);

	while(!rc_ready()); //await client connection & metadata transfer

	meta = (rdma_meta_t *) malloc(sizeof(rdma_meta_t)
			+ sge * sizeof(struct ibv_sge));

	meta->address = rc_get_peer_start_addr() + 10000000UL; //some offset (< LOG_SIZE)
	meta->total_len = read_size;
	meta->sge_count = sge;

	remaining_size = read_size;
	to_sync_size = max(remaining_size/sge,1);
	for(int i=0; i<sge; i++) {
		if(to_sync_size == 0)
			break;
		meta->sge_entries[i].addr = (uintptr_t) mem_reg + (read_size - remaining_size) + 1; 
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

	struct timespec start_time = timer_start();

	for(int i=0; i<iters; i++) {
 		slave_sync_await(meta);
	}
	//wait_for_n_completions(iters);

	long time_elapsed_nanos = timer_end(start_time);
	printf("RDMA Write complete: Ops %d, runtime [ms] %f, op-latency [us] %f\n", iters,
			time_elapsed_nanos/1e6, time_elapsed_nanos/iters/1e3);

	sleep(2);

	free(meta);
	free(mem_reg);


	return 0;
}
