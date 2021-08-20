#include "dmabench.h"
#include <err.h>
#include <errno.h>
#include <iostream>
#include <random>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <list>
#include <assert.h>
#include <libpmem.h>
#include <stdio.h>
#include <string.h>
#include <numaif.h>
#include <numa.h>
#include <utmpx.h>

#ifndef MLFS
#include "spdk/ioat.h"
#include "spdk/env.h"
#endif

#include "time_stat.h"
#include "thread.h"

#ifdef MLFS
#include <mlfs/mlfs_interface.h>	
#endif

#ifdef MLFS
const char *test_dir_prefix = "/mlfs/";
#else
const char *test_dir_prefix = "./test/";
#endif

//pcie-ssd
//const char test_dir_prefix[] = "./ssd";
//sata-ssd
//const char test_dir_prefix[] = "/data";
//harddisk
//const char test_dir_prefix[] = "/backup";

//#define LAT_BENCH

#define N_TESTS 1
#define MAX_THREADS 100
#define MIN_MAP_SIZE 1 << 30
#define OFFSET_TABLE_SIZE 524288
#define PRINT_PER_THREAD_STATS 0
#define DMA_QUEUE_DEPTH 32
#define ITERS   (file_size_bytes / io_size)

#define VERIFY

typedef enum {SEQ_WRITE, SEQ_READ, SEQ_WRITE_READ, RAND_WRITE, RAND_READ, NONE} test_t;

typedef enum {SYSCALL, MEMCPY, IOAT, NVMe} test_mode_t;

typedef enum {FS, DRAM, NVM, SSD} test_storage_t;

typedef unsigned long addr_t;

int g_socket_id; //socket id of core executing main()

#ifndef MLFS
/* ioat vars */
int n_ioat_chan;
int last_ioat_socket = -1;
int copy_done[MAX_THREADS][DMA_QUEUE_DEPTH];
int *ioat_pending;
struct spdk_ioat_chan **ioat_chans;
#endif

/* input args */
test_mode_t test_mode;
int n_threads;
uint64_t **io_buf;
unsigned long file_size_bytes;
unsigned long total_size_bytes;
unsigned long io_size;

/* non-FS optional args */
int nont = 0;		// by default, writes are non-temporal
int nodrain = 0;	// by default, writes are sfenced
int ioat = 0;	 	// by default, disable memcpy offload
int remote_numa = 0;	// by default, do local socket transfers

/* FS optional args */
int do_fsync = 0;	// by default, disable fsyncs
int do_zero = 0;	// by default, do not memset memory to zero

uint8_t *mem_base_addr;
static pthread_barrier_t tsync;
struct random_data *rnd_bufs;
char *rnd_statebufs;

/* statistics */
double wr_bps[MAX_THREADS];
double rd_bps[MAX_THREADS];

static void hexdump(void *mem, unsigned int len);

#ifndef MLFS
bool probe_cb(void* cb_ctx, spdk_pci_device* pci_device);
void attach_cb(void* cb_ctx, spdk_pci_device* pci_device, spdk_ioat_chan* ioat);
#endif

namespace utils {
	static void escape(void* p)
	{
	    asm volatile("" : : "g"(p) : "memory");
	}

	static void clobber()
	{
	    asm volatile("" : : : "memory");
	}

	static void async_op_done(void* arg) {
		*(bool*)arg = true;
	}
}

/****************************************************
 *
 *    Memory io functions
 * 
 *****************************************************/

int mem_init(int dev, ssize_t map_size)
{

	//map at least 1 GB (otherwise, we experience failures with devdax)
	if(map_size < MIN_MAP_SIZE)
		map_size = MIN_MAP_SIZE;
#ifndef MLFS
	struct spdk_env_opts opts;
	spdk_env_opts_init(&opts);
	spdk_env_init(&opts);
#endif

	if(dev == NVM) {
#if 1
		string device = "/dev/dax" + to_string(remote_numa?!g_socket_id:g_socket_id)
			+ "." + to_string(0);

		cout << "opening device: " << device << endl;
		int fd = open(device.c_str(), O_RDWR);
		if (fd < 0) {
			perror("cannot open nvm device");
			exit(-1);
		}
		cout << device << " opened" << endl;

		mem_base_addr = (uint8_t *)mmap(NULL, map_size, PROT_READ | PROT_WRITE,
				//MAP_SHARED | MAP_POPULATE | MAP_HUGETLB, fd, 0);
				MAP_SHARED | MAP_POPULATE, fd, 0);

#else
   		mem_base_addr = (uint8_t*) pmem_map_file(device.c_str(), map_size, PMEM_FILE_CREATE, 0644, NULL, NULL);

#endif
	}
	else {

#ifdef MLFS
		mem_base_addr = (uint8_t *)mmap(NULL, map_size, PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS| MAP_PRIVATE| MAP_POPULATE, -1, 0);
				//MAP_ANONYMOUS| MAP_PRIVATE, -1, 0);
#else
		mem_base_addr = (uint8_t*)spdk_dma_zmalloc_socket(map_size, 2*1024*1024, nullptr,
				remote_numa?!g_socket_id:g_socket_id);
	    			//SPDK_ENV_SOCKET_ID_ANY); 
#endif
		/*
		int fd = open("/dev/mem", O_RDWR|O_SYNC);
		if (fd == -1) {
			printf("\n Error opening /dev/mem");
			return 0;
		}
		mem_base_addr = (uint8_t*)mmap(NULL, map_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x32160000000);
		*/

		if (!mem_base_addr) {
			cout << "spdk_dma_zmalloc() failed" << endl;
			exit(-1);
		}
	}

	printf("mem_base_addr: %lu\n", mem_base_addr);
	if (mem_base_addr == MAP_FAILED) {
		perror("cannot map memory area area\n");
		exit(-1);
	}

	cout << "mapped " << map_size << " bytes" << endl;

	if(do_zero) {
		printf("zeroing memory at (addr: %lu size: %lu)\n", mem_base_addr, map_size);
		// set all lines to zero
		if(dev == NVM)
			pmem_memset_persist(mem_base_addr, 0, map_size);
		else
			memset(mem_base_addr, 0, map_size);
	}
	return 0;
}

int mem_read(uint8_t dev, uint64_t *buf, uint64_t offset, uint32_t io_size)
{
	memcpy(buf, mem_base_addr + offset, io_size);
	//perfmodel_add_delay(1, io_size);

	return 0;
}

int mem_write(uint8_t dev, uint64_t *buf, uint64_t offset, uint32_t io_size)
{
	//copy and flush data to pmem.

	//printf("src_addr: %lx dst_addr: %lx (offset: %lx) io_size: %u\n", (uintptr_t)buf ,(uintptr_t)mem_base_addr + offset, offset, io_size);
	if(nont)
		memmove(mem_base_addr + offset, buf, io_size);
	else {
		pmem_memmove_persist(mem_base_addr + offset, buf, io_size);
		//pmem_memset_persist(mem_base_addr + offset, dev, io_size);
		//pmem_memmove_nodrain(mem_base_addr + offset, buf, io_size);
	}

	//perfmodel_add_delay(0, io_size);

	return 0;
}

void mem_exit(uint8_t dev, ssize_t map_size)
{
	if(map_size < MIN_MAP_SIZE)
		map_size = MIN_MAP_SIZE;

	munmap(mem_base_addr, map_size);

	return;
}

/****************************************************
 *
 *    I/OAT io functions (memcpy offload)
 * 
 *****************************************************/

#ifndef MLFS
//FIXME: luuid (dependency for I/OAT) requires GLIBC >= 2.25; disabling for MLFS

int ioat_init(int dev, ssize_t map_size)
{
	if(map_size < MIN_MAP_SIZE)
		map_size = MIN_MAP_SIZE;

	//initialize memory first
	mem_init(dev, map_size);

	ioat_pending = (int*) calloc(n_threads, sizeof(int));

	n_ioat_chan = 0;
	ioat_chans = (spdk_ioat_chan**)spdk_dma_zmalloc(n_threads*sizeof(struct spdk_ioat_chan*),
		    0, nullptr);

	if (!ioat_chans) {
		cout << "ioat channels spdk_dma_zmalloc() failed" << endl;
		exit(-1);
	}

	cout << "ioat chans: " << ioat_chans << " count: " << n_threads << endl;

	// Probe available devices.
	// - 'probe_cb' is called for each device found.
	// - 'attach_cb' is then called if 'probe_cb' returns true
	auto ret = spdk_ioat_probe(NULL, probe_cb, attach_cb);

	if (ret != 0) {
		perror("unable to attach to ioat devices");
		exit(-1);
	}

	// TODO: allow threads to share channels
	if (n_ioat_chan < n_threads) {
		printf("thread count must be <= # of ioat channels. given: %d required <= %d\n",
				n_threads, n_ioat_chan);
		exit(-1);
	}

	cout << "I/OAT devices initialized" << endl;

	spdk_mem_register(mem_base_addr, map_size);

	cout << "memory registered for DMAs" << endl;

	return 0;
}

// -- ioat utility --
// ------------------

bool probe_cb(void* cb_ctx, spdk_pci_device* pci_device)
{
	uint32_t numa_id = spdk_pci_device_get_socket_id(pci_device);
	printf("detected ioat device %u on NUMA socket %u\n", n_ioat_chan, numa_id);

	//FIXME: only using channels from a single socket. Is that ideal?
	if(n_ioat_chan > n_threads - 1 || numa_id != g_socket_id)
	//if(n_ioat_chan > n_threads - 1 || numa_id == last_ioat_socket)
	//if(n_ioat_chan > n_threads - 1)
	    return false;

	last_ioat_socket = numa_id;

	//spdk_ioat_chan* chan = *(spdk_ioat_chan**)(cb_ctx);

	return true;
}

void attach_cb(void* cb_ctx, spdk_pci_device* pci_device, spdk_ioat_chan* ioat)
{
	// Check if that device/channel supports the copy operation
	if (spdk_ioat_get_dma_capabilities(ioat) & SPDK_IOAT_ENGINE_COPY_SUPPORTED) {
		ioat_chans[n_ioat_chan] = ioat;
		cout << "attaching to ioat device" << n_ioat_chan << " obj[" << ioat_chans[n_ioat_chan] << "]" << endl;
		n_ioat_chan++;
	}
}

int all_copies_done(int id)
{
	for(int i=0; i<ioat_pending[id]; i++) {
		if(!copy_done[id][i])
			return false;
	}

	return true;
}

int reset_copy_buffers(int id)
{
	for(int i=0; i<DMA_QUEUE_DEPTH; i++) {
		copy_done[id][i] = 0;
	}
}

// ------------------
// ------------------

int ioat_drain(uint8_t dev)
{
	//printf("waiting for %u ops on dev %u\n", ioat_pending[dev], dev);
	// We wait for 'copy_done' to have been set to true by 'async_op_done'
	do
	{
	    //printf("waiting for DMA (current pending = %u)\n", ioat_pending[dev]);
	    spdk_ioat_process_events(ioat_chans[dev % n_ioat_chan]);
	    //hexdump(mem_base_addr + (blockno * block_size_bytes), 256);
	} while (!all_copies_done(dev));

	reset_copy_buffers(dev);
	ioat_pending[dev] = 0;
}

int ioat_read(uint8_t dev, uint64_t *buf, uint64_t offset, uint32_t io_size)
{
	bool copy_done = false;
	// Submit the copy. async_op_done is called when the copy is done
	spdk_ioat_submit_copy(
	    ioat_chans[dev % n_ioat_chan],
	    &copy_done,
	    utils::async_op_done,
	    buf,
	    mem_base_addr + offset,
	    io_size);

	// We wait for 'copy_done' to have been set to true by 'req_cb'
	do
	{
	    spdk_ioat_process_events(ioat_chans[dev % n_ioat_chan]);
	} while (!copy_done);

	return 0;
}

int ioat_write(uint8_t dev, uint64_t *buf, uint64_t offset, uint32_t io_size)
{
	//bool copy_done = false;
	// Submit the copy. async_op_done is called when the copy is done
	spdk_ioat_submit_copy(
	    ioat_chans[dev % n_ioat_chan],
	    &copy_done[dev][ioat_pending[dev]],
	    utils::async_op_done,
	    mem_base_addr + offset,
	    buf,
	    io_size);

	ioat_pending[dev]++;


	if(ioat_pending[dev] == DMA_QUEUE_DEPTH) {
#if 0
		// We wait for 'copy_done' to have been set to true by 'req_cb'
		do
		{
		    //printf("waiting for DMA (current pending = %u)\n", ioat_pending[dev]);
		    spdk_ioat_process_events(ioat_chans[dev % n_ioat_chan]);
		    //hexdump(mem_base_addr + (blockno * block_size_bytes), 256);
		} while (!all_copies_done(dev));

		reset_copy_buffers(dev);
		ioat_pending[dev] = 0;
#endif
		ioat_drain(dev);
	}

	//if(!nont)
	//	pmem_persist((void *)(mem_base_addr + offset), io_size);

	return 0;
}

void ioat_exit(int dev, ssize_t map_size)
{
	if(map_size < MIN_MAP_SIZE)
		map_size = MIN_MAP_SIZE;

	spdk_mem_unregister(mem_base_addr, map_size);
	mem_exit(dev, map_size);
	spdk_ioat_detach(ioat_chans[dev]);
}
#else

int ioat_init(uint8_t dev, ssize_t map_size)
{
	return 0;
}

int ioat_read(uint8_t dev, uint64_t *buf, addr_t blockno, uint32_t io_size)
{
	return 0;
}

int ioat_write(uint8_t dev, uint64_t *buf, addr_t blockno, uint32_t io_size)
{
	return 0;
}

void ioat_exit(uint8_t dev, ssize_t map_size)
{
	return;
}

#endif

/****************************************************
 *
 *    NVMe io functions
 * 
 *****************************************************/

int nvme_init(uint8_t dev, ssize_t map_size)
{
	return 0;
}

int nvme_read(uint8_t dev, uint64_t *buf, addr_t blockno, uint32_t io_size)
{
	return 0;
}

int nvme_write(uint8_t dev, uint64_t *buf, addr_t blockno, uint32_t io_size)
{
	return 0;
}

void nvme_exit(uint8_t dev, ssize_t map_size)
{
	return;
}

//TODO: remove SSD read/write API
/*
int nvme_init(uint8_t dev, ssize_t map_size)
{
    nvme_async_io_init();
}

int nvme_read(uint8_t dev, uint64_t *buf, addr_t blockno, uint32_t io_size)
{
	return spdk_async_io_read((unsigned char *)buf, blockno, io_size,
			NULL, NULL);
}

int nvme_write(uint8_t dev, uint64_t *buf, addr_t blockno, uint32_t io_size)
{
	return spdk_async_io_write((unsigned char *)buf, blockno, io_size,
			NULL, NULL);
}

void nvme_exit(void)
{
	spdk_async_io_exit();
	return;
}
*/
/****************************************************
 *
 *    mem_bench class. each function (init, read, write) contains a switch
 *    to call the correct io device
 * 
 *****************************************************/
class mem_bench : public CThread 
{
	public:
		mem_bench(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
				test_t _test_type, test_mode_t _test_mode, int do_fsync);

		int id, fsfd, do_fsync;
		addr_t start_addr;
		unsigned long file_size_bytes;
		unsigned int io_size;
		test_t test_type;
		test_mode_t test_mode;
		//uint64_t *buf;
		uint64_t *offsets;
		struct time_stats stats;
#ifdef LAT_BENCH
		struct time_stats opstats1;
		struct time_stats opstats2;
#endif

		ssize_t offset;

		//std::list<uint64_t> io_list;

		void prepare(void);
		
		//int io_init(void);
		ssize_t io_read(int fd, void *buf, size_t count);
		ssize_t io_write(int fd, void *buf, size_t count);
		off_t io_lseek(int fd, off_t offset, int whence);
		void io_exit(uint8_t dev, ssize_t map_size);

		void do_read(void);
		void do_write(void);
		void do_exit(void);

		// Thread entry point.
		void Run(void);

		// util methods
		static unsigned long str_to_size(char* str);
		static test_t get_test_type(char *);
		static test_storage_t get_test_storage(char *);
		static test_mode_t get_test_mode(char *, int offload);
		static void show_usage(const char *prog);
};

mem_bench::mem_bench(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
		test_t _test_type, test_mode_t _test_mode, int _do_fsync) 
	: id(_id), file_size_bytes(_file_size_bytes), io_size(_io_size), 
	test_type(_test_type), test_mode(_test_mode), do_fsync(_do_fsync)
{
	start_addr = id * file_size_bytes;
	offset = 0;
}

ssize_t mem_bench::io_read(int fd, void *buf, size_t count)
{
	int ret;

#ifdef LAT_BENCH
	time_stats_start(&opstats1);
#endif
	switch (test_mode) {
		case SYSCALL: 
			ret = read(fsfd, buf, count);
			if (ret < 0) 
				err(1, "read err\n");
			count = ret;
			break;
		case MEMCPY: 
			mem_read(id, (uint64_t *)buf, start_addr + offset, count);
			//offset += count;
			break;
		case IOAT: 
			ioat_read(id, (uint64_t *)buf, start_addr + offset, count);
			//offset += count;
			break;
		case NVMe: 
			ret = nvme_read(id, (uint64_t *)buf, start_addr + offset, count);
			//if (ret > 0)
			//	offset += ret;
			count = ret;
			break;
	}

#ifdef LAT_BENCH
	time_stats_stop(&opstats1);
#endif

	/* trick the compiler to avoid optimizing memory copies */
	utils::clobber();

	return count;
}

ssize_t mem_bench::io_write(int fd, void *buf, size_t count)
{
	int ret;

#ifdef LAT_BENCH
	time_stats_start(&opstats1);
#endif
	switch (test_mode) {
		case SYSCALL: 
			ret = write(fsfd, buf, count);
			if (do_fsync) {
#ifdef LAT_BENCH
				time_stats_start(&opstats2);
#endif
				fsync(fsfd);
#ifdef LAT_BENCH
				time_stats_stop(&opstats2);
#endif
			}
			count = ret;
			break;
		case MEMCPY:
		        //printf("write - addr %lu size %lu\n", start_addr + offset, count);
			mem_write(id, (uint64_t *)buf, start_addr + offset, count);
			//offset += count;
			break;
		case IOAT: 
			ioat_write(id, (uint64_t *)buf, start_addr + offset, count);
			//offset += count;
			break;
		case NVMe:
		  	//we need the return value of the call
			ret  = nvme_write(id, (uint64_t *)buf, start_addr + offset, (uint32_t)count);
			//if (ret > 0)
			//	offset += ret;
			count = ret;
			break;
	}

#ifdef LAT_BENCH
	time_stats_stop(&opstats1);
#endif

	/* trick the compiler to avoid optimizing memory copies */
	utils::clobber();

	return count;
}

off_t mem_bench::io_lseek(int fd, off_t _offset, int whence)
{
	switch (test_mode) {
		case SYSCALL: 
			lseek(fsfd, _offset, whence);
			break;
		case MEMCPY: 
			offset = _offset;
			break;
		case IOAT: 
			offset = _offset;
			break;
		case NVMe: 
			offset = _offset;
			break;
	}
	return 0;
}

void mem_bench::io_exit(uint8_t dev, ssize_t map_size)
{
	switch (test_mode) {
		case SYSCALL: 
			break;
		case MEMCPY: 
			mem_exit(dev, map_size);
		case IOAT: 
			ioat_exit(dev, map_size);
		case NVMe: 
			nvme_exit(dev, map_size);
			break;
	}
}

void mem_bench::prepare(void)
{
	int ret;
	int numa_node;

	offsets = (uint64_t*) malloc((OFFSET_TABLE_SIZE) * sizeof(uint64_t));

	if (test_mode == SYSCALL) {
		char file_path[256];
		unsigned int len = sprintf(file_path,"%s/file0-%d", test_dir_prefix, id);

		ret = mkdir(test_dir_prefix, 0777);

		if (ret < 0 && errno != EEXIST) { 
			perror("mkdir\n");
			exit(-1);
		}

		fsfd = open(file_path, O_RDWR | O_CREAT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

		if (fsfd < 0) {
			err(1, "open");
		}

		printf("thread %d opened file[%d]: %s\n", id, fsfd, file_path);
	} 

#ifndef MLFS
	if(test_mode == IOAT) {
		io_buf[id] = (uint64_t*)spdk_dma_zmalloc_socket(io_size, 2*1024*1024, nullptr,
				remote_numa?!g_socket_id:g_socket_id);

		if (!io_buf[id]) {
			cout << "buffer spdk_dma_zmalloc_socket() failed" << endl;
			exit(-1);
		}
	}
	else
#endif
	{
		//io_buf = new uint64_t[file_size_bytes/8];
		io_buf[id] = (uint64_t *)calloc(io_size/8, sizeof(*io_buf[id]));
	}

	get_mempolicy(&numa_node, NULL, 0, (void*)io_buf[id], MPOL_F_NODE | MPOL_F_ADDR);
	//printf("read/write buffer initialized on socket %d\n", numa_node);

	if (test_type == RAND_WRITE || test_type == RAND_READ) {
		for(uint64_t i = 0; i < OFFSET_TABLE_SIZE; i++) {
			int r;
			random_r(&rnd_bufs[id], &r);
			r = r % ITERS;
			offsets[i] = (uint64_t)r * io_size; 
		}
#if 0
		std::random_device rd;
		std::mt19937 mt(rd());
		//std::mt19937 mt;
		std::uniform_int_distribution<uint64_t> dist(0, file_size_bytes - 1);
		
		for(uint64_t i = 0; i < file_size_bytes * N_TESTS; i += io_size) {
			io_list.push_back(ALIGN_FLOOR((dist(mt)), (io_size)));
			//io_list.push_back(ALIGN((dist(mt)), (4 << 10)));
		}
#endif
	}
	// Sequential
	else {
		for(uint64_t i = 0; i < OFFSET_TABLE_SIZE; i++) {
			offsets[i] = i * io_size; 
		}
	}	
}

void mem_bench::do_write(void)
{
	int fd = 0;
	unsigned int size;
	unsigned long cur_offset;
	char *buf;

	//for (unsigned long i = 0; i < file_size_bytes/8; i++) 
	//	buf[i] = '0' + (i % 10);

	time_stats_init(&stats, 1);

#ifdef LAT_BENCH
	time_stats_init(&opstats1, ITERS * N_TESTS);
	time_stats_init(&opstats2, ITERS * N_TESTS);
#endif

	pthread_barrier_wait(&tsync);

	/* start timer */
	time_stats_start(&stats);

	//printf("ITERS: %u\n", ITERS);
	for (int test = 0 ; test < N_TESTS ; test++) {

		// start of tests loop; set offset to 0
		io_lseek(fd, 0, SEEK_SET);

		/**
		 *  Do (size/rw-int) write operations
		 */
		int bytes_written;
		for (unsigned long i = 0; i < ITERS; i++) {
			//if(test_type == RAND_WRITE) {
				io_lseek(fd, offsets[(test + i) % OFFSET_TABLE_SIZE], SEEK_SET);
				//io_lseek(fd, io_list.back(), SEEK_SET);
				//io_list.pop_back();
			//}

			/*	
			if ((i+1) * io_size > file_size_bytes)
				size = file_size_bytes - i * io_size;
			else
				size = io_size;
			*/
			

		ISSUE_WRITE:

#ifdef VERIFY
			buf = (char *)io_buf[id];
			for (int j = 0; j < io_size; j++) {
				cur_offset = i * io_size + j;	
				buf[j] = '0' + (cur_offset % 10);
			}
#endif
			//hexdump(buf, 256);

			bytes_written = io_write(fd, (uint8_t *)io_buf[id], io_size);

#ifdef VERIFY
			// Note: for DMAs, verify only works if we're performing dmas synchronously
			// verify buffer

			if(test_mode == IOAT)
				ioat_drain(id);

			buf = (char *)mem_base_addr;
			for (unsigned int j = 0; j < io_size; j++) {
				cur_offset = i * io_size + j;
				if (buf[i * io_size + j] != '0' + (cur_offset % 10)) {
					hexdump(buf + i * io_size, 256);
					printf("read data mismatch at %lu\n", j);
					printf("expected %c read %c\n", (int)('0' + (cur_offset % 10)), buf[i * io_size + j]);
					exit(-1);
					break;
				}
			}
#endif
		
			/*
			if(test_mode == NVMe) {
				//EBUSY when IO queues are full.
				if (bytes_written == -1 && errno == EBUSY) {
					while(!spdk_process_completions(0));
					goto ISSUE_WRITE;

				}
				if (bytes_written == -1 && errno == EFBIG) {
					printf("SPDK error, total io divided by io unit is larger than the command queue\n");
					exit(1);
				}
			}
			*/

			if (bytes_written != io_size) {
				printf("write request %u received len %d\n",
						io_size, bytes_written);
				errx(1, "write");
			}
		}

		/**
		 *  NVMe FLUSH
		 */
		if(test_mode == NVMe) {
		  //spdk_wait_completions(1);
		}
		
		/**
		 * FS FLUSH
		 */
		if (test_mode == SYSCALL) {
			fsync(fsfd);
		}

		/**
		 * clean up after testing and quit
		 */
		if(test_mode == SYSCALL) {
			printf("thread %d closing file[%d]\n", id, fsfd);
			close(fsfd);
		}

		//end of tests loop
	}

	time_stats_stop(&stats);

	wr_bps[id] = file_size_bytes / time_stats_get_avg(&stats) * N_TESTS;

#ifdef LAT_BENCH
	time_stats_print(&opstats1, (char *)"---------------");

	if(test_mode == SYSCALL && do_fsync) {
		double avg = time_stats_get_avg(&opstats2); 
		printf("\tfsync-avg: %.3f msec (%.2f usec)\n", avg * 1000.0, avg * 1000000.0);
	}
#else
	if(PRINT_PER_THREAD_STATS) {
		time_stats_print(&stats, (char *)"---------------");
		printf("Thread %d - Throughput: %3.3f MB/s\n", id, (float)(wr_bps[id] / (float)(1024.0 * 1024.0)));
	}
#endif


	return ;
}

void mem_bench::do_read(void)
{
	int fd;
	unsigned int size;
	unsigned long i;
	unsigned long cur_offset;
	char *buf;
	int ret;

	//memset((void *)buf, 0, file_size_bytes);

	time_stats_init(&stats, 1);

#ifdef LAT_BENCH
	time_stats_init(&opstats1, ITERS * N_TESTS);
#endif

	pthread_barrier_wait(&tsync);

	time_stats_start(&stats);

	for (int test = 0 ; test < N_TESTS ; test++) {

		// start of tests loop; set offset to 0
		io_lseek(fd, 0, SEEK_SET);

		for (unsigned long i = 0; i < ITERS; i++) { 
			//if(test_type == RAND_READ) {
				io_lseek(fd, offsets[(test + i) % OFFSET_TABLE_SIZE], SEEK_SET);
				//io_lseek(fd, io_list.back(), SEEK_SET);
				//io_list.pop_back();
			//}

			/*
			if ((i+1) * io_size > file_size_bytes)
				size = file_size_bytes - i * io_size;
			else
				size = io_size;
			*/
			
	ISSUE_READ:
#ifdef VERIFY
			buf = (char*)io_buf[id];
			memset(buf, 0, io_size);

#endif
			ret = io_read(fd, (uint8_t *)io_buf, io_size);

#ifdef VERIFY
			// Note: for DMAs, verify only works if we're performing dmas synchronously
			// verify buffer
			for (unsigned int j = 0; j < io_size; j++) {
				cur_offset = i * io_size + j;
				if (buf[j] != '0' + (cur_offset % 10)) {
					hexdump(buf, 256);
					printf("read data mismatch at %lu\n", j);
					printf("expected %c read %c\n", (int)('0' + (cur_offset % 10)), buf[j]);
					exit(-1);
					break;
				}
			}
#endif
			
			if (ret != io_size) {
				printf("read size mismatch: return %d, request %lu\n",
					ret, io_size);
			}

			if(test_mode == NVMe) {
#if 1
				/* EBUSY when IO queues are full. */
				if (ret == -1 && errno == EBUSY) {
					//while(!spdk_process_completions(1));
					goto ISSUE_READ;
				}
#else
				// wait on every read completion
				while(!spdk_process_completions());
#endif

				if (ret == -1 && errno == EFBIG) {
					printf("SPDK error, total io divided by io unit is larger than the command queue\n");
					exit(1);
				}
			}
		}

		if(test_mode == NVMe) {
			printf("waiting outstanding IO completion\n");
			//spdk_wait_completions(1);
		}
	}

	time_stats_stop(&stats);

	/*
	   for (unsigned long i = 0; i < file_size_bytes; i++) {
	   int bytes_read = read(fd, buf+i, size + 100);

	   if (bytes_read != size) {
	   printf("read too far: length %d\n", bytes_read);
	   }
	   }
	   */

#if 0
	if (test_mode == SYSCALL || test_mode == NVMe) {
		// Read data integrity check.
		for (unsigned long i = 0; i < file_size_bytes/8; i++) {
			if (buf[i] != '0' + (i % 10)) {
				//hexdump(buf + i, 256);
				printf("read data mismatch at %lu\n", i);
				printf("expected %c read %c\n", (int)('0' + (i % 10)), buf[i]);
				exit(-1);
			}
		}
		printf("Read data matches\n");
	}
#endif

	rd_bps[id] = file_size_bytes / time_stats_get_avg(&stats) * N_TESTS;

#ifdef LAT_BENCH
	time_stats_print(&opstats1, (char *)"---------------");
#else
	if(PRINT_PER_THREAD_STATS) {
		time_stats_print(&stats, (char *)"---------------");
		printf("Throughput: %3.3f MB/s\n",(float)(rd_bps[id] / (float)(1024.0 * 1024.0)));
	}
#endif

	if (test_mode == SYSCALL) 
		close(fsfd);

	return ;
}

void mem_bench::do_exit()
{
	switch (test_mode) {
		case SYSCALL: 
		case MEMCPY: 
		case NVMe: 
			delete io_buf;
			break;
#ifndef MLFS
		case IOAT:
			spdk_free(io_buf);
			break;
#endif
	}
}

void mem_bench::Run(void)
{

	/* trick the compiler to avoid optimizing memory copies */
	utils::escape(io_buf);

	if (test_type == SEQ_READ || test_type == RAND_READ)
		this->do_read();
	else 
		this->do_write();

	if (test_type == SEQ_WRITE_READ)
		this->do_read();

	this->do_exit();

	return;
}

/*
   void mem_bench::Join(void)
   {
   cout << CThread::done << endl;
   CThread::Join();
   }
*/

unsigned long mem_bench::str_to_size(char* str)
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
			std::cout << "incorrect size format " << str << endl;
			break;
	}
	return file_size_bytes;
}

test_t mem_bench::get_test_type(char *test_type)
{
	/**
	 * Check the mode to bench: read or write and type
	 */
	if (!strcmp(test_type, "sr")){
		return SEQ_READ;
	}
	else if (!strcmp(test_type, "sw")) {
		return SEQ_WRITE;
	}
	else if (!strcmp(test_type, "wr")) {
		return SEQ_WRITE_READ;
	}
	else if (!strcmp(test_type, "rw")) {
		return RAND_WRITE;
	}
	else if (!strcmp(test_type, "rr")) {
		return RAND_READ;
	}
	else { 
		show_usage("iobench");
		cerr << "unsupported test type" << test_type << endl;
		exit(-1);
	}
}


test_mode_t mem_bench::get_test_mode(char *test_mode, int offload)
{
	test_mode_t mode = SYSCALL;
	/**
	 *  Test mode, SYSCALL, MEMCPY, IOAT, or NVMe
	 */
	if (!strcmp(test_mode, "fs")) {
		mode = SYSCALL;
	}
	else if (!strcmp(test_mode, "dram") || !strcmp(test_mode, "nvm")) {
		if(!offload)
			mode = MEMCPY;
		else
			mode = IOAT;

	}
	else if (!strcmp(test_mode, "ssd")) {
		mode = NVMe;
	}
	else {
		show_usage("iobench");
		cerr << "unsupported test mode " << test_mode << endl;
		exit(-1);
	}

	return mode;
}

test_storage_t mem_bench::get_test_storage(char *test_storage)
{
	test_storage_t storage = FS;

	if (!strcmp(test_storage, "fs"))
		storage = FS;
	else if (!strcmp(test_storage, "dram"))
		storage = DRAM;
	else if (!strcmp(test_storage, "nvm"))
		storage = NVM;
	else if (!strcmp(test_storage, "ssd"))
		storage = SSD;
	else { 
		show_usage("iobench");
		cerr << "unsupported test storage " << test_storage << endl;
		exit(-1);
	}

	return storage;
}


#define HEXDUMP_COLS 8
void hexdump(void *mem, unsigned int len)
{
	unsigned int i, j;

	for(i = 0; i < len + ((len % HEXDUMP_COLS) ?
				(HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++) {
		/* print offset */
		if(i % HEXDUMP_COLS == 0) {
			printf("0x%06x: ", i);
		}

		/* print hex data */
		if(i < len) {
			printf("%02x ", 0xFF & ((char*)mem)[i]);
		} else {/* end of block, just aligning for ASCII dump */
			printf("	");
		}

		/* print ASCII dump */
		if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1)) {
			for(j = i - (HEXDUMP_COLS - 1); j <= i; j++) {
				if(j >= len) { /* end of block, not really printing */
					printf(" ");
				} else if(isprint(((char*)mem)[j])) { /* printable char */
					printf("%c",(0xFF & ((char*)mem)[j]));
				} else {/* other char */
					printf(".");
				}
			}
			printf("\n");
		}
	}
}

void mem_bench::show_usage(const char *prog)
{
	std::cerr << "usage: " << prog  
		<< " <wr/sr/sw/rr/rw> <fs/nvm/dram/ssd>" 
		<< " <size: X{G,M,K,P}, eg: 100M> <IO size, e.g.: 4K> <# of threads>"
		<< " [-nont|-numa|-zero|-ioat]"
		<< endl;
}

void print_aggregate_stats(double *worker_tput, int n_workers, char *msg)
{
	double total_bps = 0;
	for(int i=0; i < n_workers; i++) {
		total_bps += worker_tput[i];
	}

	printf("%s Aggregate throughput: %3.3f GB/s\n",msg, (float)(total_bps / (float)(1024.0 * 1024.0 * 1024.0)));
}

int io_init(test_mode_t test_mode, test_storage_t test_storage)
{
	//per storage initialization
	switch (test_mode) {
		case SYSCALL: 
			break;
		case MEMCPY: 
			mem_init(test_storage, total_size_bytes);
			break;
		case IOAT:
			ioat_init(test_storage, total_size_bytes);
		case NVMe: 
			//async_tx = (async_tx_t *)malloc(sizeof(async_tx_t));
			//spdk_async_io_init();
			break;
	}
	return 0;
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
      if (strncmp("-nont", argv[i], 5) == 0) {
         nont = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-nodrain", argv[i], 8) == 0) {
	 nodrain = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-ioat", argv[i], 4) == 0) {
	 ioat = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-numa", argv[i], 5) == 0) {
	 remote_numa = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      }
      else if (strncmp("-zero", argv[i], 5) == 0) {
	 do_zero = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      } 
      else if (strncmp("-fsync", argv[i], 6) == 0) {
	 do_fsync = 1;
         dash_d = i;
	 argc = adjust_args(dash_d, argv, argc, 1);
	 goto restart;
      } 
   }

   return argc;
}

int main(int argc, char *argv[])
{
	int i;
	std::vector<mem_bench *> io_workers;

	int fd, ret;
	struct timeval t_start,t_end,t_elap;
	float sec;
	struct stat f_stat;

	ssize_t (*write_fn)(int, const void*, size_t);
	off_t (*seek_fn)(int, off_t, int);

	argc = process_opt_args(argc, argv);

	if (argc != 6) {
		mem_bench::show_usage(argv[0]);
		exit(-1);
	}

	g_socket_id = numa_node_of_cpu(sched_getcpu());
	n_threads = std::stoi(argv[5]);

	total_size_bytes = mem_bench::str_to_size(argv[3]);
	io_size = mem_bench::str_to_size(argv[4]);
	file_size_bytes = ALIGN_FLOOR((total_size_bytes / n_threads), (io_size));
	
	std::cout << "io size: " << io_size << " B" << endl
		<< "file size: " << io_size << " B" << endl
		<< "# of threads: " << n_threads << endl
		<< "io per thread: " << (file_size_bytes >> 20) << " MB" << endl
		<< "# of repetitions: " << N_TESTS << endl
		<< "aggregate size: " << ((total_size_bytes * N_TESTS) / (double)BYTES_IN_GB) << " GB" << endl;

	test_t test_type = mem_bench::get_test_type(argv[1]);
	test_mode_t test_mode = mem_bench::get_test_mode(argv[2], ioat);
	test_storage_t test_storage = mem_bench::get_test_storage(argv[2]);

	if(test_type == SEQ_WRITE || test_type == RAND_WRITE || test_type == SEQ_WRITE_READ) {
		std::cout << "store optype: " << (nont?"temporal":"non-temporal") << endl;
	//		<< "store fences: " << (nodrain?"disabled":"enabled") << endl;
	}

	pthread_barrier_init(&tsync, NULL, n_threads);

	io_buf = (uint64_t**) calloc(n_threads, sizeof(uint64_t*));

	rnd_bufs = (struct random_data*)calloc(n_threads, sizeof(struct random_data));
	rnd_statebufs = (char*)calloc(n_threads, 32);

#ifdef MLFS
	if(test_storage == FS)
		init_fs();
#endif

	for (i = 0; i < n_threads; i++) {
		initstate_r(random(), &rnd_statebufs[i], 32, &rnd_bufs[i]);
		io_workers.push_back(new mem_bench(i, 
					file_size_bytes,
					io_size,
					test_type,
					test_mode,
					do_fsync));
	}

	io_init(test_mode, test_storage);

	for (auto it : io_workers) 
		it->prepare();

	cout << "start experiment!" << endl;

	for (auto it : io_workers) 
		it->Start();

	for (auto it : io_workers) 
		it->Join();

	if (test_type == SEQ_READ || test_type == RAND_READ)
		print_aggregate_stats(rd_bps, n_threads, (char*)"[Read]");
	else 
		print_aggregate_stats(wr_bps, n_threads, (char*)"[Write]");

	if (test_type == SEQ_WRITE_READ) {
		print_aggregate_stats(rd_bps, n_threads, (char*)"[Read]");
	}

#ifdef MLFS
	if(test_storage == FS)
		shutdown_fs();
#endif

	free(rnd_bufs);
	free(rnd_statebufs);
	
	fflush(stdout);
	fflush(stderr);

	return 0;
}
