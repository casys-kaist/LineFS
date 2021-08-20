#include <time.h>
#include "time_stat.h"
#include "master.h"

int msg_sync = 0;

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

/* Returns new argc */
static int adjust_args(int i, char *argv[], int argc, unsigned del)
{
   if (i >= 0) {
      for (int j = i + del; j < argc; j++, i++)
         argv[i] = argv[j];
      argv[i] = NULL;
      return argc - del;
   }
   return argc;
}

int process_opt_args(int argc, char *argv[])
{
   int dash_d = -1;

   for (int i = 0; i < argc; i++) {
	   if (strncmp("-s", argv[i], 2) == 0) {
		   msg_sync = 1;
		   dash_d = i;
	   }
   }

   return adjust_args(dash_d, argv, argc, 1);
}

void signal_callback(struct app_context *msg)
{
	//printf("received msg[%d] with the following body: %s\n", msg->id, msg->data);
}

int main(int argc, char **argv)
{
	char *host;
	char *port_no = "12345";
	struct mr_context *regions;
	void *mem;
	int sockfd;
	int iters;
	
	struct time_stats *rpc_timer = (struct time_stats*) malloc(sizeof(struct time_stats));

	argc = process_opt_args(argc, argv);

	if (argc != 3) {
		fprintf(stderr, "usage: %s <slave-address> <iters> [-s sync]\n", argv[0]);
		return 1;
	}

	iters = atoi(argv[2]);

	time_stats_init(rpc_timer, iters);

	regions = (struct mr_context *) calloc(BUFFER_COUNT+1, sizeof(struct mr_context));

	//allocate memory for log area
	posix_memalign(&mem, sysconf(_SC_PAGESIZE), LOG_SIZE);
	regions[0].type = 0;
	regions[0].addr = (uintptr_t) mem;
        regions[0].length = LOG_SIZE;	

	//allocate some additional buffers
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
 	
	init_master(regions, BUFFER_COUNT+1, 128, signal_callback);
	sockfd = add_connection(argv[1], port_no);

	while(!rc_ready(sockfd)) {
        	asm("");
	}

	//printf("rdma_entry->meta->address = %lu \n", rdma_entry->meta->address);
	//printf("rdma_entry->meta->total_len = %lu \n", (sync->intervals->n_blk << g_block_size_shift));

	//sleep(2);

	//struct timespec start_time = timer_start();

	//wait_for_n_completions(iters);

	struct app_context *app;
	int buffer_id;
	int seqn;

	for(int i=0; i<iters; i++) {
		seqn = 1 + i; //app_ids must start at 1
		buffer_id = rc_acquire_buffer(0, &app);
		app->id = seqn;
		char* data = "this is a request with seqn:";
		snprintf(app->data, msg_size, "%s %d", data, seqn);

		//if(i==iters-1) //for the last msg, wait for a response
		//	msg_sync |= 1;

		time_stats_start(rpc_timer);
		IBV_WRAPPER_SEND_MSG_ASYNC(sockfd, buffer_id, 0);

		if(msg_sync)
			spin_till_receive(sockfd);
 			//spin_till_response_received(sockfd, seqn);

		time_stats_stop(rpc_timer);
	}

	//long time_elapsed_nanos = timer_end(start_time);

	//printf("RDMA SEND complete: Ops %d, runtime [ms] %f, op-latency [us] %f\n", iters,
	//		time_elapsed_nanos/1e6, time_elapsed_nanos/iters/1e3);

	time_stats_print(rpc_timer, "RDMA RPC - Run Complete");

	sleep(2);
	free(mem);

	return 0;
}
