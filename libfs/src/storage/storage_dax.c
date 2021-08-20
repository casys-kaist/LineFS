#ifndef NIC_SIDE
// to use O_DIRECT flag
//
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <libpmem.h>
#include <numa.h>

#include "global/global.h"
#include "global/util.h"
#include "mlfs/mlfs_user.h"
#include "storage/storage.h"
#include "concurrency/synchronization.h"

#ifdef __cplusplus
extern "C" {
#endif

// Set CPU frequency correctly
#define _CPUFREQ 2600LLU /* MHz */

#define NS2CYCLE(__ns) (((__ns) * _CPUFREQ) / 1000)
#define CYCLE2NS(__cycles) (((__cycles) * 1000) / _CPUFREQ)

#define BANDWIDTH_MONITOR_NS 10000
#define SEC_TO_NS(x) (x * 1000000000UL)

#define ENABLE_PERF_MODEL
#define ENABLE_BANDWIDTH_MODEL

/////////////////////////////////////////////////////////////// IOAT DMA ENGINE
#if defined(KERNFS) && !defined(NIC_SIDE)
#include "spdk/ioat.h"
#include "spdk/env.h"
#ifdef IOAT_INTERRUPT_KERNEL_MODULE
#include "storage/kernel-ioat-dma.h"
#endif

#define DMA_QUEUE_DEPTH 32

#define DMA_MAX_CHANNELS 8

int g_socket_id = 1; //socket id of core executing main()
/* ioat vars */
int n_ioat_chan = 0;
int ioat_attached = 0;
int last_ioat_socket = -1;
int copy_done[DMA_MAX_CHANNELS][DMA_QUEUE_DEPTH];
int chan_alloc = 0;
__thread int chan_id = -1;
int *ioat_pending; // Array of the number of pending queue entries in a channel.
struct spdk_ioat_chan **ioat_chans;
pthread_mutex_t ioat_mutex[DMA_MAX_CHANNELS];

/*  ioat forward declarations */
int ioat_init();
int ioat_register(int dev);
int ioat_read(uint8_t dev, uint64_t *buf, addr_t addr, uint32_t io_size);
int ioat_write(uint8_t dev, uint8_t *buf, addr_t addr, uint32_t io_size);

static void async_op_done(void* arg) {
	//printf("[ASYNC-COMPL] operation complete\n");
	*(bool*)arg = true;
}
#endif
///////////////////////////////////////////////////////////////////////////////


// performance parameters
/* SCM read extra latency than DRAM */
uint32_t SCM_EXTRA_READ_LATENCY_NS = 150;
// We assume WBARRIER LATENCY is 0 since write back queue can hide this even in 
// power failure.
// https://software.intel.com/en-us/blogs/2016/09/12/deprecate-pcommit-instruction
uint32_t SCM_WBARRIER_LATENCY_NS = 0;

/* SCM write bandwidth */
uint32_t SCM_BANDWIDTH_MB = 8000;
/* DRAM system peak bandwidth */
uint32_t DRAM_BANDWIDTH_MB = 63000;

uint64_t *bandwidth_consumption;
static uint64_t monitor_start = 0, monitor_end = 0, now = 0;

pthread_mutex_t mlfs_nvm_mutex;

#if (defined(__i386__) || defined(__x86_64__))
static inline void PERSISTENT_BARRIER(void)
{
	asm volatile ("sfence\n" : : );
}
#elif (defined(__aarch64__))
static inline void PERSISTENT_BARRIER(void)
{
	asm volatile ("dsb st" : : : "memory");
}
#else
#error "Not supported architecture."
#endif

///////////////////////////////////////////////////////

uint8_t *dax_addr[g_n_devices + 1];
static size_t mapped_len[g_n_devices + 1];

static inline void emulate_latency_ns(int ns)
{
	uint64_t cycles, start, stop;

	start = asm_rdtscp();
	cycles = NS2CYCLE(ns);
	//printf("cycles %lu\n", cycles);

	do {
		/* RDTSC doesn't necessarily wait for previous instructions to complete
		 * so a serializing instruction is usually used to ensure previous
		 * instructions have completed. However, in our case this is a desirable
		 * property since we want to overlap the latency we emulate with the
		 * actual latency of the emulated instruction.
		 */
		stop = asm_rdtscp();
	} while (stop - start < cycles);
}

#if 0 //Aerie.
static void perfmodel_add_delay(int read, size_t size)
{
#ifdef ENABLE_PERF_MODEL
    uint32_t extra_latency;
#endif

#ifdef ENABLE_PERF_MODEL
	if (read) {
		extra_latency = SCM_EXTRA_READ_LATENCY_NS;
	} else {
#ifdef ENABLE_BANDWIDTH_MODEL
		// Due to the writeback cache, write does not have latency
		// but it has bandwidth limit.
		// The following is emulated delay when bandwidth is full
		extra_latency = (int)size * 
			(1 - (float)(((float) SCM_BANDWIDTH_MB)/1000) /
			 (((float)DRAM_BANDWIDTH_MB)/1000)) / (((float)SCM_BANDWIDTH_MB)/1000);
#else
		//No write delay.
		extra_latency = 0;
#endif
	}

    emulate_latency_ns(extra_latency);
#endif

    return;
}
#else // based on https://engineering.purdue.edu/~yiying/nvmmstudy-msst15.pdf
static void perfmodel_add_delay(int read, size_t size)
{
	static int warning = 0;
#ifdef ENABLE_PERF_MODEL
	uint32_t extra_latency;
	uint32_t do_bandwidth_delay;

	// Only allowed for mkfs.
	if (!bandwidth_consumption) {
		if (!warning)  {
			printf("\033[31m WARNING: Bandwidth tracking variable is not set."
					" Running program must be mkfs \033[0m\n");
			warning = 1;
		}
		return ;
	}

	now = asm_rdtscp();

	if (now >= monitor_end) {
		monitor_start = now;
		monitor_end = monitor_start + NS2CYCLE(BANDWIDTH_MONITOR_NS);
		*bandwidth_consumption = 0;
	} 

	if (__sync_add_and_fetch(bandwidth_consumption, size) >=
			((SCM_BANDWIDTH_MB << 20) / (SEC_TO_NS(1UL) / BANDWIDTH_MONITOR_NS)))
		do_bandwidth_delay = 1;
	else
		do_bandwidth_delay = 0;

	if (read) {
		extra_latency = SCM_EXTRA_READ_LATENCY_NS;
	} else
		extra_latency = SCM_WBARRIER_LATENCY_NS;

	// bandwidth delay for both read and write.
	if (do_bandwidth_delay) {
		// Due to the writeback cache, write does not have latency
		// but it has bandwidth limit.
		// The following is emulated delay when bandwidth is full
		extra_latency += (int)size *
			(1 - (float)(((float) SCM_BANDWIDTH_MB)/1000) /
			 (((float)DRAM_BANDWIDTH_MB)/1000)) / (((float)SCM_BANDWIDTH_MB)/1000);
		pthread_mutex_lock(&mlfs_nvm_mutex);
		emulate_latency_ns(extra_latency);
		pthread_mutex_unlock(&mlfs_nvm_mutex);
	} else
		emulate_latency_ns(extra_latency);

#endif
	return;
}
#endif



////////////////////////////////////////////////////////////////// IOAT utility
#if defined(KERNFS) && !defined(NIC_SIDE)
bool probe_cb(void* cb_ctx, struct spdk_pci_device* pci_device)
{
	uint32_t numa_id = spdk_pci_device_get_socket_id(pci_device);
	printf("detected ioat device %u on NUMA socket %u\n", n_ioat_chan, numa_id);

	//FIXME: only using channels from a single socket. Is that ideal?
	if(numa_id != g_socket_id)
	//if(n_ioat_chan > n_threads - 1 || numa_id == last_ioat_socket)
	//if(n_ioat_chan > n_threads - 1)
	    return false;

	last_ioat_socket = numa_id;

	//spdk_ioat_chan* chan = *(spdk_ioat_chan**)(cb_ctx);

	return true;
}

void attach_cb(void* cb_ctx, struct spdk_pci_device* pci_device, struct spdk_ioat_chan* ioat)
{
	// Check if that device/channel supports the copy operation
	if (spdk_ioat_get_dma_capabilities(ioat) & SPDK_IOAT_ENGINE_COPY_SUPPORTED) {
		ioat_chans[n_ioat_chan] = ioat;
		mlfs_printf("attaching to ioat device: %d\n", n_ioat_chan);
		n_ioat_chan++;
	}
}

int all_copies_done(int id)
{
	for(int i=0; i<ioat_pending[id]; i++) {
		if(!copy_done[id][i])
			return 0;
	}

	return 1;
}

int reset_copy_buffers(int id)
{
	for(int i=0; i<DMA_QUEUE_DEPTH; i++) {
		copy_done[id][i] = 0;
	}
}
#endif
/////////////////////////////////////////////////////////////////

// ------------------
// ------------------
//

static void set_core_affinity(void) {
	struct bitmask *node_mask;
	node_mask = numa_allocate_nodemask();

	if (mlfs_conf.numa_node < 0) { // All numa nodes.
		numa_bitmask_setall(node_mask);
	} else {
		numa_bitmask_setbit(node_mask, mlfs_conf.numa_node);
	}
	
	printf("NUMA node mask=%lx\n", *node_mask->maskp);
	numa_bind(node_mask);
	numa_free_nodemask(node_mask);
}

uint8_t *dax_init(uint8_t dev, char *dev_path)
{
	int fd;
	int is_pmem;
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&mlfs_nvm_mutex, &attr);

	fd = open(dev_path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "cannot open dax device %s\n", dev_path);
		exit(-1);
	}

#if defined(KERNFS) && !defined(NIC_SIDE)
	if (mlfs_conf.ioat_memcpy_offload) {
	    // FIXME: make sure this is called only once (relevant if we use multiple dax devices)
	    if (!ioat_attached) {
#ifdef IOAT_INTERRUPT_KERNEL_MODULE
		    kernel_ioat_init();
#else
		    ioat_init();
#endif
		    set_core_affinity();
	    }
	}
#endif

#if 0
	if(dev >= 4) {
		for(int i=5; i<(g_n_devices+1); i++)
			size += dev_size[i];
	}

	printf("Total Size (debug): %lu\n", size);

	if(dev > 4) {
		uint8_t* first_log_addr = (uint8_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, 
				MAP_SHARED| MAP_POPULATE, fd, offset);
		for(int i=dev; i>4; i--)
			offset += dev_size[i];
		dax_addr[dev] = first_log_addr + offset;
		printf("first_log_addr: %lu\n", (intptr_t)first_log_addr);
	}
	else
		dax_addr[dev] = (uint8_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, 
				MAP_SHARED| MAP_POPULATE, fd, offset);

	printf("dev[%d] with offset %lu has base addr: %lu\n", dev, offset, (intptr_t)dax_addr[dev]);
#endif

 	 dax_addr[dev] = (uint8_t *)mmap(NULL, dev_size[dev], PROT_READ | PROT_WRITE,
		                        MAP_SHARED| MAP_POPULATE, fd, 0);

	if (dax_addr[dev] == MAP_FAILED) {
		perror("cannot map file system file");
		exit(-1);
	}

	// FIXME: for some reason, when mmap the Linux dev-dax, dax_addr is not accessible
	// up to the max dev_size (last 550 MB is not accessible).
	// dev_size[dev] -= (550 << 20);

	printf("dev-dax engine is initialized: dev_path %s size %lu MB\n", 
			dev_path, dev_size[dev] >> 20);

#if defined(KERNFS) && !defined(NIC_SIDE)
	if (mlfs_conf.ioat_memcpy_offload) {
#ifdef IOAT_INTERRUPT_KERNEL_MODULE
		kernel_ioat_register(dev);
#else
		ioat_register(dev);
#endif
	}
#endif

	return dax_addr[dev];
}

int dax_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
	memmove(buf, dax_addr[dev] + (blockno * g_block_size_bytes), io_size);

	//perfmodel_add_delay(1, io_size);

	mlfs_muffled("read (aligned) dev %u, block number %lu, address 0x%lx, size %u\n",
			dev, blockno, blockno * g_block_size_bytes, io_size);

	return io_size;
}

int dax_read_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
		uint32_t io_size)
{
	//copy and flush data to pmem.
	memmove(buf, dax_addr[dev] + (blockno * g_block_size_bytes) + offset,
			io_size);

	//perfmodel_add_delay(1, io_size);

	mlfs_muffled("read (unaligned) dev %u, block number %lu, offset=%u address 0x%lx size %u\n",
			dev, blockno, offset, (blockno * g_block_size_bytes) + offset, io_size);

	return io_size;
}

/* optimization: instead of calling sfence every write,
 * dax_write just does memory copy. When libmlfs commits transaction,
 * it call dax_commit to drain changes (like pmem_memmove_persist) */
int dax_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
	addr_t addr = (addr_t)dax_addr[dev] + (blockno << g_block_size_shift);

	//copy and flush data to pmem.
#ifdef NO_NON_TEMPORAL_WRITE
	memcpy((void*)addr, buf, io_size);
#else
	pmem_memmove_persist((void *)addr, buf, io_size);
	PERSISTENT_BARRIER();
#endif

	//memmove(dax_addr[dev] + (blockno * g_block_size_bytes), buf, io_size);
	//perfmodel_add_delay(0, io_size);

	mlfs_muffled("write block number %lu, address %lu size %u\n", 
			blockno, (blockno * g_block_size_bytes), io_size);

	return io_size;
}

int dax_write_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset, 
		uint32_t io_size)
{
	addr_t addr = (addr_t)dax_addr[dev] + (blockno << g_block_size_shift) + offset;

	//copy and flush data to pmem.
#ifdef NO_NON_TEMPORAL_WRITE
	memcpy((void*)addr, buf, io_size);
#else
	pmem_memmove_persist((void *)addr, buf, io_size);
	//PERSISTENT_BARRIER();
#endif
	//memmove(dax_addr[dev] + (blockno * g_block_size_bytes) + offset, buf, io_size);
	//perfmodel_add_delay(0, io_size);

	mlfs_muffled("write block number %lu, address %lu size %u\n", 
			blockno, (blockno * g_block_size_bytes) + offset, io_size);

	return io_size;
}


/****************************************************
 *
 *    I/OAT io functions (memcpy offload)
 * 
 *****************************************************/

//FIXME: luuid (dependency for I/OAT) requires GLIBC >= 2.25; disabling for MLFS

/////////////////////////////////////////////////////////////// IOAT DMA ENGINE
#if defined(KERNFS) && !defined(NIC_SIDE)
int ioat_init()
{
	struct spdk_env_opts opts;

	// DPDK default values are loaded. Core mask 0x1 is included.
	spdk_env_opts_init(&opts);
	spdk_env_init(&opts);

	ioat_pending = (int*) mlfs_zalloc(DMA_MAX_CHANNELS * sizeof(int));

	for(int i=0 ; i<DMA_MAX_CHANNELS; i++)
		ioat_pending[i] = 0;

	n_ioat_chan = 0;
	ioat_chans = (struct spdk_ioat_chan **)spdk_dma_zmalloc(DMA_MAX_CHANNELS *sizeof(struct spdk_ioat_chan*),
		    0, NULL);

	if (!ioat_chans) {
		panic("ioat channels spdk_dma_zmalloc() failed");
	}

	//mlfs_printf("ioat chans: %p count %u\n", ioat_chans, 8);

	// Probe available devices.
	// - 'probe_cb' is called for each device found.
	// - 'attach_cb' is then called if 'probe_cb' returns true
	int ret = spdk_ioat_probe(NULL, probe_cb, attach_cb);

	if (ret != 0) {
		perror("unable to attach to ioat devices");
		exit(-1);
	}

	//if (n_ioat_chan < n_threads) {
	//	printf("thread count must be <= # of ioat channels. given: %d required <= %d\n",
	//			n_threads, n_ioat_chan);
	//	exit(-1);
	//}

	for(int i=0; i < DMA_MAX_CHANNELS; i++)
		pthread_mutex_init(&ioat_mutex[i], NULL);

	ioat_attached = 1;

	mlfs_printf("%s", "I/OAT devices initialized\n");


}

int ioat_register(int dev)
{
	spdk_mem_register(dax_addr[dev], dev_size[dev]);

	//for(int i=0; i<DMA_QUEUE_DEPTH; i++)
	//	copy_done[i] = 0;

	printf("dev %d memory registered for DMAs\n", dev);

	return 0;
}

int ioat_read(uint8_t dev, uint64_t *buf, addr_t addr, uint32_t io_size)
{
	bool copy_done = false;

	//pthread_mutex_lock(&ioat_mutex[dev]);

	// Submit the copy. async_op_done is called when the copy is done
	spdk_ioat_submit_copy(
	    ioat_chans[dev % n_ioat_chan],
	    &copy_done,
	    async_op_done,
	    buf,
	    (void*)addr,
	    io_size);

	// We wait for 'copy_done' to have been set to true by 'req_cb'
	do
	{
	    spdk_ioat_process_events(ioat_chans[dev % n_ioat_chan]);
	} while (!copy_done);

	//pthread_mutex_unlock(&ioat_mutex[dev]);

	return 0;
}

int ioat_write(uint8_t dev, uint8_t *buf, addr_t addr, uint32_t io_size)
{
	//bool copy_done = false;

	if(chan_id < 0) {
		// FIXME: atomic!
		chan_id = chan_alloc;
		chan_alloc++;

		for(int i=0; i< DMA_QUEUE_DEPTH; i++)
			copy_done[chan_id][i] = 0;
	}


	if (!ioat_chans) {
	    panic("ioat_chan is NULL.\n");
	}
	//pthread_mutex_lock(&ioat_mutex[dev]);
	// Submit the copy. async_op_done is called when the copy is done
	spdk_ioat_submit_copy(
	    ioat_chans[chan_id % n_ioat_chan],
	    &copy_done[chan_id][ioat_pending[chan_id]],
	    async_op_done,
	    (void*)addr,
	    buf,
	    io_size);

	ioat_pending[chan_id]++;

	//printf("[ASYNC-SUBMITTED] operation %u submitted\n", ioat_pending[dev]);

	if(ioat_pending[chan_id] == DMA_QUEUE_DEPTH) {
		// We wait for 'copy_done' to have been set to true by 'async_op_done'
		do
		{
		    //printf("waiting for DMA (current pending = %u)\n", ioat_pending[dev]);
		    spdk_ioat_process_events(ioat_chans[chan_id % n_ioat_chan]);
		    //hexdump(mem_base_addr + (blockno * block_size_bytes), 256);
		} while (!all_copies_done(chan_id));

		    reset_copy_buffers(chan_id);
		    ioat_pending[chan_id] = 0;
	}

	//pthread_mutex_unlock(&ioat_mutex[dev]);
	//if(!nont)
	//	pmem_persist((void *)(mem_base_addr + offset), io_size);

	return 0;
}

int ioat_drain(uint8_t dev)
{
	//pthread_mutex_lock(&ioat_mutex[dev]);

	if(chan_id < 0)
		return 0;

	//assert(chan_id >= 0);

	// We wait for 'copy_done' to have been set to true by 'async_op_done'
	do
	{
	    //printf("waiting for DMA (current pending = %u)\n", ioat_pending[dev]);
	    spdk_ioat_process_events(ioat_chans[chan_id % n_ioat_chan]);
	    //hexdump(mem_base_addr + (blockno * block_size_bytes), 256);
	} while (!all_copies_done(chan_id));

	reset_copy_buffers(chan_id);
	ioat_pending[chan_id] = 0;

	//pthread_mutex_unlock(&ioat_mutex[dev]);
}

void ioat_exit(int dev)
{
	spdk_mem_unregister(dax_addr[dev], dev_size[dev]);
	spdk_ioat_detach(ioat_chans[dev]);
}
#endif
///////////////////////////////////////////////////////////////////////////////

int dax_write_opt(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
	addr_t addr = (addr_t)dax_addr[dev] + (blockno << g_block_size_shift);
	// mlfs_printf("write_opt dev=%hhu src=%p dst=0x%lx len=%u\n", dev, buf, addr, io_size);

	START_MP_PER_TASK_TIMER(evt_raw_memcpy);
#if defined(KERNFS) && !defined(NIC_SIDE)
	if(mlfs_conf.ioat_memcpy_offload && io_size >= 4096) {
#ifdef IOAT_INTERRUPT_KERNEL_MODULE
		kernel_ioat_write(dev, (addr_t) buf, addr, io_size);
#else
		ioat_write(dev, buf, addr, io_size);
#endif
	} else
#endif
	{
		//copy and flush data to pmem.
#ifdef NO_NON_TEMPORAL_WRITE
		memcpy((void*)addr, buf, io_size);
#else
		pmem_memmove_persist((void *)addr, buf, io_size);
		PERSISTENT_BARRIER();
#endif
	}
	//memmove(dax_addr[dev] + (blockno * g_block_size_bytes), buf, io_size);
	//perfmodel_add_delay(0, io_size);
	END_MP_PER_TASK_TIMER_AND_INC_DATA_SIZE(evt_raw_memcpy, (uint64_t)io_size);

	mlfs_muffled("write(opt) dev=%hhu blknr=%lu, address=%lu size=%u\n",
		     dev, blockno, (blockno * g_block_size_bytes), io_size);

	return io_size;
}

int dax_write_opt_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
		uint32_t io_size)
{
	addr_t addr = (addr_t)dax_addr[dev] + (blockno << g_block_size_shift) + offset;
	// mlfs_printf("write_opt_unaligned dev=%hhu src=%p dst=0x%lx len=%u\n",
	//             dev, buf, addr, io_size);

#if defined(KERNFS) && !defined(NIC_SIDE)
	if(mlfs_conf.ioat_memcpy_offload && io_size >= 4096) {
#ifdef IOAT_INTERRUPT_KERNEL_MODULE
		kernel_ioat_write(dev, (addr_t)buf, addr, io_size);
#else
		ioat_write(dev, buf, addr, io_size);
#endif
	} else
#endif
	{
		//copy and flush data to pmem.
#ifdef NO_NON_TEMPORAL_WRITE
		memcpy((void*)addr, buf, io_size);
#else
		pmem_memmove_persist((void *)addr, buf, io_size);
		//PERSISTENT_BARRIER();
#endif
	}

	//memmove(dax_addr[dev] + (blockno * g_block_size_bytes) + offset, buf, io_size);
	//perfmodel_add_delay(0, io_size);

	mlfs_muffled("write(opt) dev=%hhu blknr=%lu offset=%u address=%lu "
		     "size=%u\n",
		     dev, blockno, offset,
		     (blockno * g_block_size_bytes) + offset, io_size);

	return io_size;
}

#if 0
int dax_commit(uint8_t dev, addr_t blockno, uint32_t offset, uint32_t io_size, int flags)
{
	assert(flags != 0);

	addr_t addr = (addr_t)dax_addr[dev] + (blockno << g_block_size_shift) + offset;

	if(flags & PMEM_PERSIST_FLUSH & PMEM_PERSIST_DRAIN) {
		mlfs_debug("dax_commit[dev:%u] with flush+drain: blknr %u iosize %u\n", dev, blockno, io_size);
		pmem_persist((void *)addr, io_size);
	}
	else if(flags & PMEM_PERSIST_FLUSH) {
		mlfs_debug("dax_commit[dev:%u] with flush: blknr %u iosize %u\n", dev, blockno, io_size);
		pmem_flush((void *)addr, io_size);
	}
	else if(flags & PMEM_PERSIST_DRAIN) {
		mlfs_debug("dax_commit[dev:%u] with drain: blknr %u iosize %u\n", dev, blockno, io_size);
		pmem_drain();
	}

	//if(flags & PMEM_PERSIST_ORDERED)
	//	PERSISTENT_BARRIER();

	return 0;
}
#endif

/**
 * Persist data by flushing data from cache to NVM.
 * It is used on replication.
 */
int dax_persist(uint8_t dev, addr_t blockno, uint32_t offset, uint32_t io_size)
{
	addr_t addr = (addr_t)dax_addr[dev] + (blockno << g_block_size_shift) + offset;
        pmem_persist((void *)addr, io_size);
        return 0;
}

int dax_commit(uint8_t dev)
{
#if defined(KERNFS) && !defined(NIC_SIDE)
    if (mlfs_conf.ioat_memcpy_offload) {
	m_barrier();
	ioat_drain(dev);
    }
#endif
}

int dax_erase(uint8_t dev, addr_t blockno, uint32_t io_size)
{
	memset(dax_addr[dev] + (blockno * g_block_size_bytes), 0, io_size);

	//perfmodel_add_delay(0, io_size);
}

void dax_exit(uint8_t dev)
{
#if defined(KERNFS) && !defined(NIC_SIDE)
    if (mlfs_conf.ioat_memcpy_offload) {
	ioat_exit(dev);
    }
#endif
    munmap(dax_addr[dev], dev_size[dev]);
    return;
}

#ifdef __cplusplus
}
#endif
#endif /* !NIC_SIDE */
