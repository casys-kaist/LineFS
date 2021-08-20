#include <sys/mman.h>

#include "global/global.h"
#include "global/util.h"
#include "distributed/rpc_interface.h"
#include "io/device.h"
#include "storage/storage.h"

ncx_slab_pool_t *nic_slab_pool;
struct mr_context *remote_mrs;  // MRs of host. It is required in NRRG/NRRGU request.
int remote_n_regions;

#define LOG2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X)) - 1))
static int ceiling_log2(unsigned int v)
{
    return LOG2(v-1) + 1;
}

/*
static int next_power_of_2 (int v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    // v |= v >> 32;
    v++;
    return v;
}
*/

void nic_slab_init(uint64_t pool_size)
{
// #ifdef NIC_SLAB_NCX
	uint8_t *pool_space;

	// Transparent huge page allocation.
	pool_space = (uint8_t *)mmap(NULL, pool_size, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);

	mlfs_assert(pool_space);

	if(madvise(pool_space, pool_size, MADV_HUGEPAGE) < 0)
		panic("cannot do madvise for huge page\n");

	nic_slab_pool = (ncx_slab_pool_t *)pool_space;
	nic_slab_pool->addr = pool_space;
	nic_slab_pool->min_shift = 3;
	nic_slab_pool->end = pool_space + pool_size;

	ncx_slab_init(nic_slab_pool);
// #else
//         ;
// #endif
}

void *nic_slab_alloc_in_blk(uint64_t block_cnt)
{
	uint64_t size = block_cnt << 12;
#ifdef NIC_SLAB_NCX
	void* ptr;
	ptr = ncx_slab_alloc(nic_slab_pool, size);

	// mlfs_printf("[SLAB] alloc: %p - %p %lu bytes\n", ptr, ptr + size -1,
	//         size);
	// nic_slab_print_stat();

	return ptr;
	// return ncx_slab_alloc(nic_slab_pool, size);
#else
	return mlfs_alloc(size);
#endif
}

void *nic_slab_alloc_in_byte(uint64_t bytes) {
#ifdef NIC_SLAB_NCX
	void* ptr; size_t blk_cnt;
	// FIXME slab bug allocating small size.
	if (bytes < 4096) {
		ptr = ncx_slab_alloc(nic_slab_pool, 4096);
		// mlfs_printf("[SLAB] alloc: %p - %p %d bytes\n", ptr, ptr+4096 -1, 4096);
	} else {
		// Allocate at 4K granularity.
		// if (bytes % 4096 == 0)
		//         blk_cnt = (bytes >> g_block_size_shift);
		// else
		//         blk_cnt = (bytes >> g_block_size_shift)+1;
		// ptr = ncx_slab_alloc(nic_slab_pool, blk_cnt);
		// mlfs_printf("[SLAB] alloc: %p - %p %lu bytes\n", ptr, ptr+blk_cnt -1, blk_cnt << g_block_size_shift);

		ptr = ncx_slab_alloc(nic_slab_pool, bytes);
		// mlfs_printf("[SLAB] alloc: %p - %p %lu bytes\n", ptr, ptr+bytes -1, bytes);
	}

	// mlfs_printf("[SLAB] alloc: %p - %p %lu bytes\n", ptr, ptr+bytes -1, bytes);
	// nic_slab_print_stat();

	return ptr;
	// return ncx_slab_alloc(nic_slab_pool, bytes);
#else
	return mlfs_alloc(bytes);
#endif
}

void nic_slab_free(void *p)
{
#ifdef NIC_SLAB_NCX
	// mlfs_printf("[SLAB] free: %p\n", p);
	ncx_slab_free(nic_slab_pool, p);

	// nic_slab_print_stat();
#else
	mlfs_free(p);
#endif
}

struct timespec slab_profile_start = {0};
struct timespec slab_profile_end = {0};
void nic_slab_print_stat(void) {
#ifdef NIC_SLAB_NCX
	clock_gettime(CLOCK_MONOTONIC, &slab_profile_end);

	if (get_duration(&slab_profile_start, &slab_profile_end) > 1.0) {
		clock_gettime(CLOCK_MONOTONIC, &slab_profile_start);

		ncx_slab_stat_t stat;
		ncx_slab_stat(nic_slab_pool, &stat);

		printf("##### NIC_SLAB ####\n");
		printf("# pool_size: %lu\n", stat.pool_size);
		printf("# used_size: %lu\n", stat.used_size);
		printf("NIC_SLAB used: %lu %%\n", stat.used_pct);
		printf("# page_small: %lu\n", stat.p_small);
		printf("# page_exact: %lu\n", stat.p_exact);
		printf("# page_big: %lu\n", stat.p_big);
		printf("# page_page: %lu\n", stat.p_page);
		printf("# byte_small: %lu\n", stat.b_small);
		printf("# byte_exact: %lu\n", stat.b_exact);
		printf("# byte_big: %lu\n", stat.b_big);
		printf("# byte_page: %lu\n", stat.b_page);
		printf("# max_free_pages: %lu\n", stat.max_free_pages);
	}
#else
#endif
}

size_t get_nic_slab_used_pct(void)
{
#ifdef NIC_SLAB_NCX
	ncx_slab_stat_t stat;
	ncx_slab_stat(nic_slab_pool, &stat);
	return stat.used_pct;
#else
#endif
	return 0;

}

/**
 * nic_rpc storage engine is used for SmartNIC kernfs to request storage operations
 * to its host kernfs.
 */
uint8_t* nic_rpc_init (uint8_t dev, char *dev_path)
{

}

void set_remote_mr (addr_t start_addr, uint64_t len, int region_id)
{
    remote_mrs[region_id].type = region_id;
    remote_mrs[region_id].addr = (uint64_t) start_addr;
    remote_mrs[region_id].length = len;
}

static int get_type_with_addr(addr_t addr)
{
    int i = 0;
    addr_t start_addr;
    addr_t end_addr;
    uint64_t len;
    for (i = 0; i < remote_n_regions; i++) {
        if (i == MR_DRAM_BUFFER)        // No access to the remote MR_DRAM_BUFFER.
            continue;
        if ((addr >= remote_mrs[i].addr) &&
                (addr < remote_mrs[i].addr + remote_mrs[i].length))
            return i;
    }
    printf("Unknown MR type with address: 0x%lx\n", addr);
    panic("Unknown MR type.");
    return -1;
}

/************************************************************************************************
 * Read from host storage and get the data.
 * Data is written to dst_addr of NIC kernfs via RDAM_write_imm from host kernfs.
 ************************************************************************************************/
int nic_rpc_read_and_get (uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
    struct app_context *msg;
    char req_msg_data[MAX_SIGNAL_BUF];
    char cmd_hdr[20];
    char pref[1000];
    int len;   // pref length.
    int j;
    uint32_t host_kernfs_id;
    uintptr_t dst_addr;

    assert (io_size <= g_block_size_bytes); // MR_MEM_BUFFER slot size is 4096 bytes.
    dst_addr = (uintptr_t)nic_slab_alloc_in_blk(1);
    host_kernfs_id = nic_kid_to_host_kid(g_kernfs_id);

    // Get src address. It is an address of host.
    uintptr_t src_addr;
    src_addr = (uintptr_t)(g_bdev[dev]->map_base_addr + (blockno << g_block_size_shift));

    mlfs_info("Client: (before) nrrg src_addr 0x%lx src_blknr=%lu dst_addr 0x%lx "
	      "*dst_addr 0x%016lx buf(addr) %p *buf 0x%016lx\n",
	      src_addr, blockno, dst_addr, *(uint64_t *)dst_addr, buf,
	      *(uint64_t *)buf);
    rdma_meta_t *meta = create_rdma_meta(dst_addr, src_addr, io_size);  // dst_addr:local addr, src_addr:remote addr.
    IBV_WRAPPER_READ_SYNC(g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_IO], meta, MR_DRAM_BUFFER, get_type_with_addr(src_addr));

    // FIXME If buf is already in MR, we can reduce 1 memcpy by set src_addr to buf directly.
    memmove(buf, (uint8_t *)dst_addr, io_size);     // one memcpy from read buffer to buf.
    mlfs_info("Client: (after) nrrg src_addr 0x%lx dst_addr 0x%lx *dst_addr 0x%016lx buf(addr) %p *buf 0x%016lx\n",
            src_addr,
            dst_addr,
            *(uint64_t*)dst_addr,
            buf,
            *(uint64_t*)buf);

    nic_slab_free((void*)dst_addr);
    return io_size;
}

int nic_rpc_read_and_get_unaligned (uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
		uint32_t io_size)
{
    struct app_context *msg;
    char req_msg_data[MAX_SIGNAL_BUF];
    char cmd_hdr[20];
    char pref[1000];
    int len;   // pref length.
    int j;
    uint32_t host_kernfs_id;
    uintptr_t dst_addr;

    assert (io_size <= g_block_size_bytes); // MR_MEM_BUFFER slot size is 4096 bytes.
    dst_addr = (uintptr_t)nic_slab_alloc_in_blk(1);
    host_kernfs_id = nic_kid_to_host_kid(g_kernfs_id);

    // Get src address. It is an address of host.
    uintptr_t src_addr;
    src_addr = (uintptr_t)(g_bdev[dev]->map_base_addr + (blockno << g_block_size_shift) + offset);

    mlfs_info("Client: (before) nrrgu src_addr 0x%lx src_blknr=%lu dst_addr 0x%lx "
	      "*dst_addr 0x%016lx buf(addr) %p *buf 0x%016lx\n",
	      src_addr, blockno, dst_addr, *(uint64_t *)dst_addr, buf,
	      *(uint64_t *)buf);

    rdma_meta_t *meta = create_rdma_meta(dst_addr, src_addr, io_size);  // dst_addr:local addr, src_addr:remote addr.
    IBV_WRAPPER_READ_SYNC(g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_IO], meta, MR_DRAM_BUFFER, get_type_with_addr(src_addr));

    // FIXME If buf is already in MR, we can reduce 1 memcpy by set src_addr to buf directly.
    memmove(buf, (uint8_t *)dst_addr, io_size);     // one memcpy from read buffer to buf.
    mlfs_info("Client: (after) nrrgu src_addr 0x%lx dst_addr 0x%lx *dst_addr 0x%016lx buf(addr) %p *buf 0x%016lx\n",
            src_addr,
            dst_addr,
            *(uint64_t*)dst_addr,
            buf,
            *(uint64_t*)buf);
    nic_slab_free((void*)dst_addr);
    return io_size;
}

#if 0
// FIXME Do we need these functions?
/**
 * Read to the host local buffer.
 */
int nic_rpc_read_local (uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
    // TODO Send rpc to host.
    // TODO Receive ack from host.
    // TODO Return received io_size.
}

int nic_rpc_read_local_unaligned (uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
		uint32_t io_size)
{
    // TODO Send rpc to host.
    // TODO Receive ack from host.
    // TODO Return received io_size.
}
#endif

/************************************************************************************************
 * Write from host local to host local.
 * Data is not sent from kernfs on NIC.
 ************************************************************************************************/

#if 0
int nic_rpc_write_local (uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
    // TODO Send rpc to host.
    // TODO Receive ack from host.
    // TODO Return received io_size.
}

int nic_rpc_write_local_unaligned (uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
		uint32_t io_size)
{
    // TODO Send rpc to host.
    // TODO Receive ack from host.
    // TODO Return received io_size.
}
#endif

// Used for: data (digestion)
/*****************************************************************************************
 * write_opt functions are used in digestion.
 * TODO Do asynchronous write. Coalescing and lease make async write possible by providing
 * exclusiveness. Additional mechanism is required to prevent CQ overrun. (Ex. threshold)
 *****************************************************************************************/
int nic_rpc_write_opt_local (uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
    struct app_context *msg;
    char req_msg_data[MAX_SIGNAL_BUF];
    char cmd_hdr[20];
    uint32_t host_kernfs_id;

    // Send rpc to host.
    // NRWOL: Nic_Rpc_Write_Opt_Local
    sprintf(req_msg_data, "|nrwol |%u|%lu|%lu|%u|",
            dev, (unsigned long)buf, blockno, io_size);
    host_kernfs_id = nic_kid_to_host_kid(g_kernfs_id);
    // rpc_nicrpc_msg(g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_BG], req_msg_data, false); // async. TODO To make it async, Host KernFS should not send response.
    rpc_nicrpc_msg(g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_BG], req_msg_data, true); // sync.

    // Parse msg and data sent from host.
    // sscanf(msg->data, "|%s", cmd_hdr);

    return io_size;
}
int nic_rpc_write_opt_local_unaligned (uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
		uint32_t io_size)
{
    struct app_context *msg;
    char req_msg_data[MAX_SIGNAL_BUF];
    char cmd_hdr[20];
    uint32_t host_kernfs_id;

    // Send rpc to host.
    // NRWOLU: Nic_Rpc_Write_Opt_Local_Unaligned
    sprintf(req_msg_data, "|nrwolu |%u|%lu|%lu|%u|%u|",
            dev, (unsigned long)buf, blockno, offset, io_size);
    host_kernfs_id = nic_kid_to_host_kid(g_kernfs_id);
    // rpc_nicrpc_msg(g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_BG], req_msg_data, false); // async. TODO To make it async, Host KernFS should not send response.
    rpc_nicrpc_msg(g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_BG], req_msg_data, true); // sync.

    // Parse msg and data sent from host.
    // sscanf(msg->data, "|%s", cmd_hdr);

    return io_size;
}

/************************************************************************************************
 * Send data from kernfs on NIC to host and write to host storage.
 ************************************************************************************************/

// Used for: inode (write_ondisk_inode)
//           migrate --> it would be better to call nic_rpc_write_local() not using IOAT because
//                       migration might move data between different media(ex. NVM to SSD).
int nic_rpc_send_and_write (uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
    struct app_context *msg;
    char req_msg_data[MAX_SIGNAL_BUF];
    char cmd_hdr[20];
    int len;    // prefix length.
    int j;
    uint32_t host_kernfs_id;
    uintptr_t src_addr, dst_addr;   // Used for RDMA write.

    // Copy data to MR_DRAM_BUFFER.
    // FIXME If buf is already in MR, we can reduce 1 memcpy by set src_addr to buf directly.
    assert (io_size <= g_block_size_bytes); // MR_MEM_BUFFER slot size is 4096 bytes.
    src_addr = (uintptr_t)nic_slab_alloc_in_blk(1);
    memmove((uint8_t *)src_addr, buf, io_size);

    // Setup RDMA.
    dst_addr = (uintptr_t) (g_bdev[g_root_dev]->map_base_addr + (blockno << g_block_size_shift));
    mlfs_info("Client (before) nrsw RDMA write: map_base_addr %p blknr=%lu "
	      "offset 0x%lx(%lu)\n",
	      g_bdev[g_root_dev]->map_base_addr, blockno,
	      blockno << g_block_size_shift, blockno);
    rdma_meta_t *meta = create_rdma_meta(src_addr, dst_addr, io_size);

    mlfs_info("Client (before) nrsw RDMA write: buf %p *buf %016lx src 0x%lx *src 0x%016lx dst 0x%lx io_size %u\n",
            buf,
            *(uint64_t*)buf,
            src_addr,
            *(uint64_t*)src_addr,
            dst_addr,
            io_size);
    host_kernfs_id = nic_kid_to_host_kid(g_kernfs_id);

    IBV_WRAPPER_WRITE_SYNC(g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_IO], meta, MR_DRAM_BUFFER, MR_NVM_SHARED);  // sync.
    mlfs_info("Client (after) nrsw RDMA write: buf %p *buf %016lx src 0x%lx *src 0x%016lx dst 0x%lx io_size %u\n",
            buf,
            *(uint64_t*)buf,
            src_addr,
            *(uint64_t*)src_addr,
            dst_addr,
            io_size);
    nic_slab_free((void*)src_addr);
    mlfs_free(meta);

    return io_size;
}

int nic_rpc_send_and_write_unaligned (uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset, uint32_t io_size)
{
    uint32_t host_kernfs_id;
    int j;
    uintptr_t src_addr, dst_addr;   // Used for RDMA write.

    // Copy data to MR_DRAM_BUFFER.
    // FIXME If buf is already in MR, we can reduce 1 memcpy by set src_addr to buf directly.
    assert (io_size <= g_block_size_bytes); // MR_MEM_BUFFER slot size is 4096 bytes.
    src_addr = (uintptr_t)nic_slab_alloc_in_blk(1);
    memmove((uint8_t *)src_addr, buf, io_size);

    // Setup RDMA.
    dst_addr = (uintptr_t) (g_bdev[g_root_dev]->map_base_addr + (blockno << g_block_size_shift) + offset);
    mlfs_info("Client (before) nrswu RDMA write: map_base_addr %p blknr=%lu "
	      "offset 0x%lx(%lu) + 0x%x(%u) = 0x%lx\n",
	      g_bdev[g_root_dev]->map_base_addr, blockno,
	      blockno << g_block_size_shift, blockno, offset, offset,
	      (blockno << g_block_size_shift) + offset);
    rdma_meta_t *meta = create_rdma_meta(src_addr, dst_addr, io_size);

    mlfs_info("Client (before) nrswu RDMA write: buf %p *buf %016lx src 0x%lx *src 0x%016lx dst 0x%lx offset %x io_size %u\n",
            buf,
            *(uint64_t*)buf,
            src_addr,
            *(uint64_t*)src_addr,
            dst_addr,
            offset,
            io_size);
    host_kernfs_id = nic_kid_to_host_kid(g_kernfs_id);

    IBV_WRAPPER_WRITE_SYNC(g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_IO], meta, MR_DRAM_BUFFER, MR_NVM_SHARED);  // sync.
    mlfs_info("Client (after) nrswu RDMA write: buf %p *buf %016lx src 0x%lx *src 0x%016lx dst 0x%lx offset %x io_size %u\n",
            buf,
            *(uint64_t*)buf,
            src_addr,
            *(uint64_t*)src_addr,
            dst_addr,
            offset,
            io_size);
    nic_slab_free((void*)src_addr);
    mlfs_free(meta);

    return io_size;
}

// mlfs_commit
int nic_rpc_commit (uint8_t dev)
{
    char req_msg_data[MAX_SIGNAL_BUF];
    int host_kernfs_id;

    // Send rpc to host.
    sprintf(req_msg_data, "|nrcommit |%u", dev);
    host_kernfs_id = nic_kid_to_host_kid(g_kernfs_id);
    rpc_nicrpc_msg(g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_IO], req_msg_data, true);
}

int nic_rpc_erase (uint8_t dev, addr_t blockno, uint32_t io_size)
{
    assert(false); // It seems not to be used.

    char req_msg_data[MAX_SIGNAL_BUF];
    int host_kernfs_id;

    // Send rpc to host.
    sprintf(req_msg_data, "|nrerase |");
    host_kernfs_id = nic_kid_to_host_kid(g_kernfs_id);
    rpc_nicrpc_msg(g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_IO], req_msg_data, true);
}

void nic_rpc_exit (uint8_t dev)
{
    // exit.
}
