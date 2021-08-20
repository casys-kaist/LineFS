#include <time.h>
#include <math.h>
#include <signal.h>
#include <libpmem.h>
#include <sys/mman.h>
#include "time_stat.h"
#include "agent.h"

#define BLOCK_SIZE 4096
#define BLOCK_SIZE_SHIFT 12


#define IS_ALIGNED(x, a) (((x) & ((__typeof__(x))(a) - 1)) == 0)

#ifdef __cplusplus
#define ALIGN(x, a)  ALIGN_MASK((x), ((__typeof__(x))(a) - 1))
#else
#define ALIGN(x, a)  ALIGN_MASK((x), ((typeof(x))(a) - 1))
#endif
#define ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))

enum memory_type {
	MR_DRAM = 0,
	MR_NVM,
	MR_COUNT
};

volatile sig_atomic_t stop;

uint64_t BUFFER_SIZE = 8388608UL;//8 MB
uint64_t MR_SIZE = 10737418240UL;	 //1 GB

//uint64_t MR_SIZE = 268265456UL; //256 MB

int batch_size = 1;	//default - batching disabled
int msg_sync = 0;	//default - asynchronous writes
int sge_count = 1;	//default - 1 scatter/gather element

struct mr_context regions[MR_COUNT];
struct time_stats *timer;


static inline unsigned long ALIGN_FLOOR(unsigned long x, int mask)
{
	if (IS_ALIGNED(x, mask))
		return x;
	else
		return ALIGN(x, mask) - mask;
}

void inthand(int signum)
{	
	stop = 1;
}

// call this function to start a nanosecond-resolution timer
struct timespec timer_start()
{
	struct timespec start_time;
	clock_gettime(CLOCK_REALTIME, &start_time);
	return start_time;
}

// call this function to end a timer, returning nanoseconds elapsed as a long
long timer_end(struct timespec start_time)
{
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
restart:
   for (int i = 0; i < argc; i++) {
      //printf("argv[%d] = %s\n", i, argv[i]);
      if (strncmp("-b", argv[i], 2) == 0) {
         batch_size = atoi(argv[i+1]);
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 2);
	 goto restart;
      }
      else if (strncmp("-e", argv[i], 2) == 0) {
	 sge_count = atoi(argv[i+1]);
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 2);
	 goto restart;
      }
   }

   return argc;
}

uint32_t str_to_size(char* str)
{
	/* magnitude is last character of size */
	char size_magnitude = str[strlen(str)-1];
	/* erase magnitude char */
	str[strlen(str)-1] = 0;
	unsigned long file_size_bytes = strtoull(str, NULL, 0);
	switch(size_magnitude) {
		case 'g':
		case 'G':
			file_size_bytes *= 1024;
		case 'm':
		case 'M':
			file_size_bytes *= 1024;
		case '\0':
		case 'k':
		case 'K':
			file_size_bytes *= 1024;
			break;
		case 'p':
		case 'P':
			file_size_bytes *= 4;
			break;
		case 'b':
		case 'B':
         break;
		default:
			printf("incorrect size format: %s\n");
			break;
	}
	return file_size_bytes;
}

void signal_callback(struct app_context *msg)
{
	ibw_cpu_relax();
}

void test_callback(struct app_context *msg)
{
	ibw_cpu_relax();
}

int main(int argc, char **argv)
{
	char *host;
	char *portno = "12345";
	int src_mr = 0;
	int dst_mr = 0;
	int region_idx = 0;
	void *ptr;
	int sockfd;
	uint32_t iosize;
	uint32_t transfer_size;
	int iters;
	int ret;
	int isClient = 0;

	int dev = 0;

	timer = (struct time_stats*) malloc(sizeof(struct time_stats));

	argc = process_opt_args(argc, argv);

	if (argc != 6 && argc != 1) {
		fprintf(stderr, "usage: %s <peer-address> <src: dram|nvm> <dst: dram|nvm> <io-size> <iters> [-e <sge count>] [-b <batch size>] (note: run without args to use as server)\n", argv[0]);
		return 1;
	}

	if(argc > 1)
		isClient = 1;

	if(isClient) {

	 	if(!strcmp("nvm", argv[2])) {
			src_mr = MR_NVM;
		}
		else
			src_mr = MR_DRAM;

	 	if(!strcmp("nvm", argv[3])) {
			dst_mr = MR_NVM;
		}
		else
			dst_mr = MR_DRAM;
	
		iosize = str_to_size(argv[4]);
		//sge_count = atoi(argv[3]);	
		iters = atoi(argv[5]);

		time_stats_init(timer, 1);

		//allocate memory for log area
		transfer_size = iters * iosize;

		if(transfer_size > MR_SIZE) {
			printf("Insufficient memory region size; required %lu while MR_SIZE is set to %lu\n", transfer_size, MR_SIZE);
			return 1;
		}
	}

	// map dram region
	if(src_mr == MR_DRAM || !isClient) {
		printf("Mapping dram memory: size %u bytes\n", MR_SIZE);
		ret = posix_memalign(&ptr, sysconf(_SC_PAGESIZE), MR_SIZE);

		regions[region_idx].type = MR_DRAM;
		regions[region_idx].addr = (uintptr_t) ptr;
		regions[region_idx].length = MR_SIZE;

		if(ret) {
			printf("Failed to map space for dram memory region\n");
			return 1;
		}
		region_idx++;
		printf("Region - dram: addr = %p size = %u bytes\n", ptr, MR_SIZE);
	}

	// map nvm region
	if(src_mr == MR_NVM || !isClient) {
		printf("Mapping nvm memory: size %u bytes\n", MR_SIZE);
		char device[] = "/dev/dax0.X";
		device[10] = dev + '0';

		int fd = open(device, O_RDWR);

		if (fd < 0) {
			printf("Failed to open nvm device: %s\n", device);
			return 1;
		}

		printf("Opened nvm device: %s\n", device);
		ptr = (uint8_t *)mmap(NULL, MR_SIZE, PROT_READ | PROT_WRITE,
				 MAP_SHARED| MAP_POPULATE, fd, 0);
				//MAP_ANONYMOUS| MAP_SHARED, fd, 0);

		if (ptr == MAP_FAILED) {
			printf("Failed to map space for nvm memory region\n");
			return 1;
		}

		printf("Region - nvm: addr = %p size = %u bytes\n", ptr, MR_SIZE);
		regions[region_idx].type = MR_NVM;
		regions[region_idx].addr = (uintptr_t) ptr;
		regions[region_idx].length = MR_SIZE;
	}

	/*
	//allocate some additional buffers
	for(int i=1; i<BUFFER_COUNT+1; i++) {
		posix_memalign(&mem, sysconf(_SC_PAGESIZE), BUFFER_SIZE);
		regions[i].type = i;
		regions[i].addr = (uintptr_t) mem;
        	regions[i].length = BUFFER_SIZE;	
	}
	*/
		/*typedef struct rdma_metadata {
			addr_t address;
			addr_t total_len;
			int sge_count;
			struct ibv_sge sge_entries[];
		} rdma_meta_t;*/	
 	
	init_rdma_agent(portno, regions, isClient?1:MR_COUNT, 256, NULL, NULL, isClient?test_callback:signal_callback);

	// Run in server mode
	if(!isClient) {
		signal(SIGINT, inthand);

 		while(!stop) {
			sleep(1);
		}
		//sleep(1);

		for(int i=0; i<MR_COUNT; i++) {
			if(regions[i].type == MR_DRAM)
				free((void *)regions[i].addr);
			else
				munmap((void *)regions[i].addr, MR_SIZE);
		}
		return 0;
	}

	// Run in client mode
 	sockfd = add_connection(argv[1], portno, 0, 1);

	while(!rc_ready(sockfd)) {
        	asm("");
	}


	// Comment out if integrity checks aren't necessary
	// fill out dummy data
	for(int i=0 ;i<MR_SIZE; i++) {
		((char *)ptr)[i] = '0' + (i % 10);
	}

	rdma_meta_t *meta[batch_size];
	uint32_t wr_id = 0;
	uint32_t msg_bytes = iosize;
	uint32_t sge_bytes = -1;
	uint64_t remote_base_addr = mr_remote_addr(sockfd, dst_mr);
	uint64_t local_base_addr = mr_local_addr(sockfd, src_mr);
	uint64_t transferred_bytes = 0;
	uint32_t remaining_bytes = msg_bytes;
	int n = 0;

	time_stats_start(timer);

	for(int i=0; i<iters; i++) {
		meta[n] =  (rdma_meta_t *) malloc(sizeof(rdma_meta_t)
			+ sge_count * sizeof(struct ibv_sge));
		meta[n]->addr = remote_base_addr + transferred_bytes;
		meta[n]->length = msg_bytes;
		meta[n]->sge_count = sge_count;
		meta[n]->next = NULL;

		//printf("rdma-write[%d][%d]: addr %lx length %u\n", i, n, meta[n]->addr, meta[n]->length);
		for(int j=0; j<sge_count; j++) {
			if(j == sge_count - 1)
				sge_bytes = remaining_bytes;
			else
				sge_bytes = max(msg_bytes/sge_count, 1);

			meta[n]->sge_entries[j].addr = local_base_addr + transferred_bytes + (msg_bytes - remaining_bytes); 
			meta[n]->sge_entries[j].length = sge_bytes;
			//printf("sge[%d]: addr %lx length %u\n", j, meta[n]->sge_entries[j].addr, meta[n]->sge_entries[j].length);
			remaining_bytes -= sge_bytes;
		}

		//not the first operation in batch
		if(n) {
			meta[n-1]->next = meta[n];
			//printf("meta[%d]->next = meta[%d]\n", n-1, n);
		}

		remaining_bytes = msg_bytes;
		transferred_bytes += msg_bytes;
		n = (n + 1) % batch_size;

		//reached end of batch or iterations
		if(!n || i == iters-1) {
			//printf("ib_post_send\n");
			IBV_WRAPPER_WRITE_ASYNC(sockfd, meta[0], src_mr, dst_mr);
		}

		//wait for last op
		if(!msg_sync && i == iters-1) {
			printf("waiting till response received. n=%d, meta[n]->imm=%u\n", n, meta[n]->imm);
			IBV_AWAIT_PENDING_WORK_COMPLETIONS(sockfd);
		}

	}


	time_stats_stop(timer);

	//long time_elapsed_nanos = timer_end(start_time);

	//printf("RDMA SEND complete: Ops %d, runtime [ms] %f, op-latency [us] %f\n", iters,
	//		time_elapsed_nanos/1e6, time_elapsed_nanos/iters/1e3);

	time_stats_print(timer, "RDMA READ - Run Complete");

	printf("Throughput: %3.3f MB/s\n",(float)(transfer_size)
			/ (1024.0 * 1024.0 * (float) time_stats_get_avg(timer)));

	//sleep(1);

	if(src_mr == MR_DRAM)
		free(ptr);
	else
		munmap(ptr, MR_SIZE);

	return 0;
}
