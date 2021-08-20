#include <time.h>
#include <signal.h>
#include "slave.h"

volatile sig_atomic_t stop;

int BUFFER_COUNT = 5;
uint64_t LOG_SIZE =  268265456UL; //256 MB
uint64_t BUFFER_SIZE = 8388608UL; //8 MB  

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

void signal_callback(struct app_context *msg)
{
	printf("received msg[%d] with the following body: %s\n", msg->id, msg->data);
#if 0
	struct app_context *app;
	int buffer_id = rc_acquire_buffer(0, &app);
	app->id = msg->id; //set this to same id of received msg (to act as a response)
	char* data = "answering your message";
	snprintf(app->data, msg_size, "%s", data);
	IBV_WRAPPER_SEND_MSG_ASYNC(msg->sockfd, buffer_id, 0);
#else
	rdma_meta_t *meta = (rdma_meta_t*) malloc(sizeof(rdma_meta_t) + sizeof(struct ibv_sge));
	meta->addr = 0;
	meta->length =0;
	meta->sge_count = 0;
	//meta->sge_entries[0].addr = NULL
	//meta->sge_entries[0].length = 0;
	meta->imm = msg->id; //set immediate to sequence number in order for requester to match it (in case of io wait)
	IBV_WRAPPER_WRITE_WITH_IMM_ASYNC(msg->sockfd, meta, 0, 0);
#endif
}

int main(int argc, char **argv)
{
	char *host;
	char *port_no = "12345";
	struct mr_context *regions;
	void *mem;
	int sockfd;
	int iters;
	int sync;

	if (argc != 1) {
		fprintf(stderr, "usage: %s\n", argv[0]);
		return 1;
	}

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
 
	init_slave(regions, BUFFER_COUNT+1, 256, port_no, signal_callback);

 	signal(SIGINT, inthand);

 	while(!stop) {
		sleep(1);
	}
	//sleep(1);
	free(mem);

	return 0;
}
