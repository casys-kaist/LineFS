#include "leases.h"
#include "distributed/rpc_interface.h"
//#include "filesystem/fs.h"
//#include "filesystem/file.h"
//#include "posix/posix_interface.h"
#include "mlfs/mlfs_interface.h"
#include "filesystem/stat.h"
#include "concurrency/thread.h"

#ifdef KERNFS

#ifdef __x86_64__
#include "fs.h"
#elif __aarch64__
#include "nic/nic_fs.h"
#else
#error "Unsupported architecture."
#endif

#include "concurrency/thpool.h"
#else
#include "filesystem/fs.h"
#include "log/log.h"
#endif

#if MLFS_LEASE

double revoke_sec=0;
//struct timeval start_time, end_time;

static SharedGuts *SharedGuts_create(void *base, size_t size)
{
	SharedGuts *guts = base;
	//pthread_mutexattr_t attr;
	//int rc = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	//assert(rc == 0);
	int ret = pthread_spin_init(&guts->mutex, PTHREAD_PROCESS_SHARED);
	assert(ret == 0);
	guts->base = base;
	guts->size = size;
	guts->next_allocation = (char *)(guts + 1);
	guts->hash = NULL;
	return guts;
}

static SharedGuts *SharedGuts_mock()
{
	SharedGuts *guts = mlfs_zalloc(sizeof(SharedGuts));
	int ret = pthread_spin_init(&guts->mutex, PTHREAD_PROCESS_PRIVATE);
	assert(ret == 0);
	return guts;
}

SharedTable *SharedTable_create(const char *name, size_t arena_size)
{
	SharedTable *t;

	t = mlfs_zalloc(sizeof(SharedTable));
	t->fd = shm_open(name, O_CREAT | O_RDWR, 0666);
	assert(t->fd > 0);

	t->size = arena_size;
	if (ftruncate(t->fd, t->size) == -1)
	    panic("ftruncate for SharedTable creation failed.");

	t->base = mmap(0, t->size, PROT_READ | PROT_WRITE, MAP_SHARED, t->fd, 0);
	assert(t->base != MAP_FAILED);

	memset(t->base, 0, t->size);
	t->guts = SharedGuts_create(t->base, t->size);

	return t;
}

SharedTable *SharedTable_mock()
{
	SharedTable *t = mlfs_zalloc(sizeof(SharedTable));
	t->guts = SharedGuts_mock();

}

SharedTable *SharedTable_subscribe(const char *name)
{
	SharedTable *t = mlfs_zalloc(sizeof(SharedTable));
	t->fd = shm_open(name, O_CREAT | O_RDWR, 0666);
	assert(t->fd > 0);
	SharedGuts *guts = mmap(NULL, sizeof (SharedGuts), PROT_READ | PROT_WRITE, MAP_SHARED, t->fd, 0);
	assert(guts != MAP_FAILED);

	pthread_spin_lock(&guts->mutex);

	//void *where = guts->base;
	void *where = 0;
	t->size = guts->size;
	munmap(guts, sizeof (SharedGuts));
	t->base = mmap(where, t->size, PROT_READ | PROT_WRITE, MAP_SHARED, t->fd, 0);
	t->guts = t->base;
	//printf("---- DEBUGM --- t->guts->base %p t->base %p\n", t->guts->base, t->base);
	//assert(t->guts->base == t->base);
	assert(t->guts->size == t->size);

	pthread_spin_unlock(&t->guts->mutex);
	return t;
}

void SharedTable_unsubscribe(SharedTable *t)
{
	munmap(t->base, t->size);
	close(t->fd);
}

static void *SharedTable_malloc(SharedTable *t, size_t size)
{
	void *ptr = t->guts->next_allocation;
	if ((ptr - t->base) + size <= t->size) {
		t->guts->next_allocation += size;
	} else {
		assert(0 && "out of memory");
		ptr = NULL;
	}
	return ptr;
}

// TODO: implement frees for SharedTable
static void SharedTable_free(SharedTable *t, void *p)
{
	// no-op
}

static inline mlfs_lease_t *lcache_find(uint32_t inum)
{
	mlfs_lease_t *ls;

	//pthread_spin_lock(&lcache_spinlock);

	HASH_FIND(hash_handle, lease_table->guts->hash, &inum,
        		sizeof(uint32_t), ls);

	//pthread_spin_unlock(&lcache_spinlock);

	return ls;
}

static inline mlfs_lease_t *lcache_alloc_add(uint32_t inum)
{
	mlfs_lease_t *ls;

#ifndef LEASE_OPT

#ifdef __cplusplus
	ls = static_cast<mlfs_lease_t *>(mlfs_zalloc(sizeof(mlfs_lease_t)));
#else
	ls = mlfs_zalloc(sizeof(mlfs_lease_t));
#endif
	pthread_spin_init(&ls->mutex, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ls->mutex_revoke, PTHREAD_PROCESS_SHARED);
	//pthread_mutex_init(&ls->mutex, NULL);

	if (!ls)
		panic("Fail to allocate lease\n");

	ls->inum = inum;

	HASH_ADD(hash_handle, lease_table->guts->hash, inum,
	 		sizeof(uint32_t), ls);

#else
#undef uthash_malloc
#define uthash_malloc(sz) SharedTable_malloc(lease_table, sz)
	ls = SharedTable_malloc(lease_table, sizeof(mlfs_lease_t));

	pthread_spin_init(&ls->mutex, PTHREAD_PROCESS_SHARED);
	pthread_spin_init(&ls->mutex_revoke, PTHREAD_PROCESS_SHARED);
	//pthread_mutexattr_t att;
	//pthread_mutexattr_init(&att);
	//pthread_mutexattr_setpshared(&att, PTHREAD_PROCESS_SHARED);
	//pthread_mutex_init(&ls->mutex, &att);

	if (!ls)
		panic("Fail to allocate lease\n");

	ls->inum = inum;

	HASH_ADD(hash_handle, lease_table->guts->hash, inum,
	 		sizeof(uint32_t), ls);
#undef uthash_malloc

#endif

	return ls;
}

static inline mlfs_lease_t *lcache_add(mlfs_lease_t *ls)
{
	uint32_t inum = ls->inum;

	pthread_spin_lock(&lease_table->guts->mutex);

#ifndef LEASE_OPT
	HASH_ADD(hash_handle, lease_table->guts->hash, inum,
	 		sizeof(uint32_t), ls);
#else
#undef uthash_malloc
#define uthash_malloc(sz) SharedTable_malloc(lease_table, sz)
	HASH_ADD(hash_handle, lease_table->guts->hash, inum,
	 		sizeof(uint32_t), ls);
#undef uthash_malloc
#endif

	pthread_spin_unlock(&lease_table->guts->mutex);

	return ls;
}

static inline int lcache_del(mlfs_lease_t *ls)
{

	pthread_spin_lock(&lease_table->guts->mutex);
#ifndef LEASE_OPT
	HASH_DELETE(hash_handle, lease_table->guts->hash, ls);
	free(ls);
#else
#undef uthash_malloc
#define uthash_malloc(sz) SharedTable_malloc(lease_table, sz)
	HASH_DELETE(hash_handle, lease_table->guts->hash, ls);

	SharedTable_free(lease_table, ls);
#undef uthash_malloc
#endif
	pthread_spin_unlock(&lease_table->guts->mutex);
	return 0;
}

void lupdate(mlfs_lease_t *ls)
{
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
mlfs_lease_t* lget(uint32_t inum)
{
	mlfs_lease_t *ls;

	pthread_spin_lock(&lease_table->guts->mutex);

	ls = lcache_find(inum);

	if (ls) {
		pthread_spin_unlock(&lease_table->guts->mutex);
		return ls;
	}

	ls = lcache_alloc_add(inum);

	pthread_spin_unlock(&lease_table->guts->mutex);

	return ls;
}

// Allocate a new inode with the given type on device dev.
// A free inode has a type of zero.
mlfs_lease_t* lalloc(uint8_t type, uint32_t inode_nr)
{
	uint32_t inum;
	int ret;
	//struct buffer_head *bp;
	mlfs_lease_t *ls;


	ls = lcache_find(inode_nr);

	if (!ls)
		ls = lget(inode_nr);

#if 0

	ip->_dinode = (struct dinode *)ip;

	if (!(ip->flags & I_VALID)) {
		read_ondisk_inode(dev, inode_nr, &dip);

		if (dip.itype == 0) {
			memset(&dip, 0, sizeof(struct dinode));
			dip.itype = type;
			dip.dev = dev;
		}

		sync_inode_from_dinode(ip, &dip);
		ip->flags |= I_VALID;
	}

	ip->i_sb = sb;
	ip->i_generation = 0;
	ip->i_data_dirty = 0;
	ip->nlink = 1;
#endif

	return ls;
}


int read_ondisk_lease(uint32_t inum, mlfs_lease_t *ls)
{
	int ret;
	struct buffer_head *bh;
	addr_t lease_block;

	lease_block = get_lease_block(g_root_dev, inum);
	mlfs_debug("read lease block: %lu\n", lease_block);
	bh = bh_get_sync_IO(g_root_dev, lease_block, BH_NO_DATA_ALLOC);

	bh->b_size = sizeof(mlfs_lease_t);
	bh->b_data = (uint8_t *)ls;
	bh->b_offset = sizeof(mlfs_lease_t) * (inum % LPB);
	bh_submit_read_sync_IO(bh);
	mlfs_io_wait(g_root_dev, 1);

	return 0;
}

int write_ondisk_lease(mlfs_lease_t *ls)
{
	int ret;
	struct buffer_head *bh;
	addr_t lease_block;

	lease_block = get_lease_block(g_root_dev, ls->inum);
	mlfs_debug("write lease block: %lu\n", lease_block);
	bh = bh_get_sync_IO(g_root_dev, lease_block, BH_NO_DATA_ALLOC);

	bh->b_size = sizeof(mlfs_lease_t);
	bh->b_data = (uint8_t *)ls;
	bh->b_offset = sizeof(mlfs_lease_t) * (ls->inum % LPB);
	ret = mlfs_write(bh);
	mlfs_io_wait(g_root_dev, 0);

	return ret;
}

#ifdef LIBFS
int acquire_family_lease(uint32_t inum, int type, char *path)
{
	mlfs_printf("trying to acquire child/parent leases of type %d for inum %u\n",
			type, inum);
	// child should always be acquired first, otherwise we deadlock
	// due to ordering of namei
	acquire_lease(inum, type, path);
	acquire_parent_lease(inum, type, path);

	return 1;
}

int acquire_parent_lease(uint32_t inum, int type, char *path)
{
	mlfs_printf("trying to acquire parent lease of type %d for inum %u\n",
			type, inum);
	char name[DIRSIZ], parent_path[DIRSIZ];
	struct inode *pid;

	pid = nameiparent(path, name);
	get_parent_path(path, parent_path, name);

	acquire_lease(pid->inum, type, parent_path);

	return 1;
}

int report_lease_error(uint32_t inum)
{
	mlfs_lease_t* ls = lcache_find(inum);

	if (!ls)
		panic("failed to find lease\n");

	ls->errcode = 1;
	return 1;
}

// invoked by KernFS upcall
int revoke_lease(int sockfd, uint64_t seq_n, uint32_t inum)
{
#ifdef LEASE_DEBUG
	mlfs_printf("\e[31m [L] KernFS requesting lease revocation for inum %u \e[0m\n", inum);
#endif

	mlfs_lease_t* ls = lcache_find(inum);

	if (!ls) {
		ls = lcache_alloc_add(inum);
	}

	if (!ls)
		panic("failed to allocate lease\n");

	pthread_spin_lock(&ls->mutex);

	if (ls->hid != g_self_id)
		panic("kernfs trying to revoke an unowned lease\n");

	// busy waiting for all revokable marks
	// FIXME: Use CPU instruction PAUSE - Spin Loop Hint
	while (ls->holders > 0) {
		cpu_relax();
	}

	//rpc_lease_flush_response(sockfd, seq_n, inum, ls->lversion, ls->lblock);
	rpc_lease_change(ls->mid, g_self_id, ls->inum, LEASE_FREE, ls->lversion, ls->lblock, false);

	ls->state = LEASE_FREE;
	ls->hid = 0;

	pthread_spin_unlock(&ls->mutex);

#ifdef LEASE_DEBUG
	mlfs_printf("\e[1;95m[LIBFS ID=%d] revoke lease (inum %u)\e[0m\n", g_self_id, inum);
#endif
}

int mark_lease_revocable(uint32_t inum)
{
	uint64_t start_tsc;

	mlfs_lease_t* ls = lcache_find(inum);

	if (!ls) {
		panic("failed to allocate lease\n");
	}

	if (ls->holders == 0) {
		panic("trying to give up unowned lease.\n");
	}

	struct logheader_meta *loghdr_meta;
	loghdr_meta = get_loghdr_meta();

	if (ls->state == LEASE_WRITE) {
		ls->lversion = g_fs_log[0]->avail_version;
		ls->lblock = loghdr_meta->hdr_blkno;
	}

#ifdef LEASE_DEBUG
	mlfs_printf("\e[1;95m[LIBFS ID=%d] mark lease revocable (inum=%u, block=%lu, version=%hu)\e[0m\n",
		  g_self_id, inum, ls->lblock, ls->lversion);
#endif

	if (enable_perf_stats)
		start_tsc = asm_rdtscp();

	if (ls->holders == 1) {
		while (make_digest_request_async(100) == -EBUSY)
			cpu_relax();

		mlfs_info("%s", "[L] Giving up lease. asynchronous digest!\n");
	}

	int holders = ls->holders;

	// subtract ls->holders exactly one
	while (!__sync_bool_compare_and_swap(&ls->holders, holders, holders - 1))
		holders = ls->holders;

	if (enable_perf_stats)
		g_perf_stats.lease_revoke_wait_tsc += (asm_rdtscp() - start_tsc);

	return 0;
}

int acquire_lease(uint32_t inum, int type, char *path)
{
#ifdef LEASE_DEBUG
	mlfs_printf("\e[1;95m[LIBFS ID=%d] acquire lease (inum=%u, type=%d)\e[0m\n", g_self_id, inum, type);
#endif

	mlfs_lease_t* ls = lcache_find(inum);

	int i_dirty = 0;

	uint64_t start_tsc;

	if (!ls) {
		ls = lcache_alloc_add(inum);
	}

	if (!ls)
		panic("failed to allocate lease\n");

	if (enable_perf_stats)
		start_tsc = asm_rdtscp();

	pthread_spin_lock(&ls->mutex);

	// FIXME: Use inum instead of path for read RPCs
	// temporarily trimming paths to reduce overheads
	// char trimmed_path[MAX_REMOTE_PATH];
	// snprintf(trimmed_path, MAX_REMOTE_PATH, "%s", path);
retry:
	if ((type != LEASE_FREE && ls->state == LEASE_FREE) || ls->hid != g_self_id) {
		// case for LEASE_FREE -> LEASE_READ or LEASE_FREE -> LEASE_WRITE
		// if hid is not g_self_id, should acquire lease from kernfs
		rpc_lease_change(ls->mid, g_self_id, ls->inum, type, 0, 0, true);
#if 0
		// received invalid response
		if (ls->errcode) {
			ls->errcode = 0; // clear error code
			goto retry;
		}
#endif
		ls->holders++;
		ls->state = type;
		ls->hid = g_self_id;
#if 1
		i_dirty = 1;
#endif
#ifdef LEASE_DEBUG
		mlfs_printf("LIBFS ID = %d lease acquired path %s new state %d manager ID = %u\n",
				g_self_id, path, type, ls->mid);
#endif
	}
	else if (ls->state == LEASE_READ && type == LEASE_WRITE) {
		// case for LEASE_READ -> LEASE_WRITE
		rpc_lease_change(ls->mid, g_self_id, ls->inum, type, 0, 0, true);
#if 0
		// received invalid response
		if (ls->errcode) {
			ls->errcode = 0; // clear error code
			goto retry;
		}
#endif
		ls->holders++;
		ls->state = type;
#if 1
		i_dirty = 1;
#endif
#ifdef LEASE_DEBUG
		mlfs_printf("LIBFS ID = %d lease acquired path %s new state %d manager ID = %u\n",
				g_self_id, path, type, ls->mid);
#endif
	}
	else {
		ls->holders++;
#ifdef LEASE_DEBUG
		mlfs_printf("\e[1;95m[LIBFS ID=%d] lease already acquired (inum=%u, state=%d). No RPCs triggered.\e[0m\n", g_self_id, inum, ls->state);
#endif
	}

	mlfs_info("[DEBUG] after ls->inum %u ls->holders = %d\n", ls->inum, ls->holders);

	pthread_spin_unlock(&ls->mutex);

#if 1
	if (i_dirty) {
		// update size after acquiring inode
		// necessary to prevent issues with directory allocation
		struct inode *ip;
		ip = icache_find(inum);

		ilock(ip);
		// no need to update size for dirty inodes
		//if (ip && !(ip->flags & I_DIRTY)) {
		//	struct dinode dip;
		//	read_ondisk_inode(inum, &dip);

		//	ip->size = dip.size;
		//}
		sync_inode_ext_tree(ip);
		iunlock(ip);
	}
#endif

	if (enable_perf_stats)
		g_perf_stats.lease_lpc_wait_tsc += (asm_rdtscp() - start_tsc);

	return 1;
}

// necessary when dropping a lease
// FIXME: prevent unnecessary cache wipes by only calling this when a lease is in conflict
int purge_dir_caches(uint32_t inum)
{
	//purge node's de_cache entry from inode's parent
	//purge dcache entry of inode
	//purge dcache entries of all children files
	//purge decache entries of all files

	mlfs_info("purging cache for inum: %u\n", inum);


#if 0
	struct inode* inode = icache_find(g_root_dev, inum);

	if (!inode)
		return 0;

	mlfs_info("purging caches for inum: %u\n", inode->inum);

#if 0
	struct inode *id, *pid;
	uint8_t *dirent_array;
	struct mlfs_dirent de = {0};
	char name[DIRSIZ];
	offset_t off = 0;

	pid = nameiparent(path, name);

	if ((id = dir_lookup(pid, name, &off)) != NULL)  {
		get_dirent(pid, &de, off);

		mlfs_assert(strcmp(de.name, name) == 0);
		de_cache_del(pid, name);
	}

	dlookup_del(path);
#endif
	if(inode->itype == T_DIR)
		de_cache_del_all(inode);

	fcache_del_all(inode);
	icache_del(inode);
	free(inode);

	//TODO: should also delete path from lookup table
	// is there an easy way to do this using just the inum?
	//dlookup_del(inode->dev, path);
#endif
	return 1;
}
#endif


#ifdef LIBFS
void update_remote_ondisk_lease(uint8_t node_id, mlfs_lease_t *ls)
{
	mlfs_debug("writing remote ondisk lease inum %u to peer %u\n", ls->inum, node_id);
	addr_t offset = (get_lease_block(g_root_dev, ls->inum) << g_block_size_shift) + sizeof(mlfs_lease_t) * (ls->inum %LPB);
	addr_t src = ((uintptr_t)g_bdev[g_root_dev]->map_base_addr) + offset;
	addr_t dst = mr_remote_addr(g_peers[node_id]->sockfd[0], MR_NVM_SHARED) + offset;
	rdma_meta_t *meta = create_rdma_meta(src, dst, sizeof(mlfs_lease_t));

	IBV_WRAPPER_WRITE_ASYNC(g_peers[node_id]->sockfd[0], meta, MR_NVM_SHARED, MR_NVM_SHARED);
}

#if 0
// For lease acquisitions, lease must be locked beforehand.
int modify_lease_state(int req_id, int inum, int new_state, int version, addr_t logblock)
{
	uint64_t start_tsc;

	// get lease from cache
	mlfs_lease_t* ls = lget(inum);

	if (!ls)
		panic("failed to allocate lease\n");


	mlfs_info("modifying lease inum[%u] from %d to %d Req. ID = %d Holder ID = %d (num holders = %u)\n",
			inum, ls->state, new_state, req_id, ls->hid, ls->holders);

	if(new_state == LEASE_READ) {
		panic("read path not implemented!\n");
	}
	else if(new_state == LEASE_FREE) {
		ls->lversion = version;
		ls->lblock = logblock;

		if(ls->holders > 0)
			ls->holders--;
		else
			panic("lease surrender failed. Lease has no current owners!\n");
		//ls->holders--;
		return 1;
	}

	else {

		//pthread_mutex_lock(&ls->mutex);

		if(ls->holders == 0) {

			goto acquire_lease;
		}
		else {


			if (enable_perf_stats)
				start_tsc = asm_rdtscp();

			// wait until original lease owner relinquishes its lease
			while(ls->holders > 0)
				cpu_relax();

			if (enable_perf_stats)
				g_perf_stats.local_contention_tsc += (asm_rdtscp() - start_tsc);

			//FIXME: need to implement this call; we currently assume that the digest checkpoint was reached
			//wait_till_digest_checkpoint(g_sync_ctx[ls->hid]->peer, ls->lversion, ls->lblock);

acquire_lease:
			if (enable_perf_stats)
				start_tsc = asm_rdtscp();

			// wait until lease reaches its designated digest checkpoint
			// KernFS sets lease version and lblock to zero when this condition is met
			while(ls->lversion | ls->lblock)
				cpu_relax();

			if (enable_perf_stats)
				g_perf_stats.local_digestion_tsc += (asm_rdtscp() - start_tsc);

			ls->holders++;
			ls->state = new_state;
			ls->hid = req_id;
			//ls->holders--;

			goto PERSIST;
		}


	}

PERSIST:

	write_ondisk_lease(ls);

	for(int i=0; i<g_n_nodes;i++) {
		if(i == g_kernfs_id)
			continue;
		mlfs_printf("updating remote lease for inum %u\n", ls->inum);
		update_remote_ondisk_lease(i, ls);
	}

	//pthread_mutex_unlock(&ls->mutex);

	return 1;
}
#endif

void shutdown_lease_protocol()
{
	//mlfs_printf("Elapsed time for revocations: %lf\n", revoke_sec);
}

#else
void handle_lazy_revoke(void *arg) {
	mlfs_lease_t *ls = (mlfs_lease_t *)arg;

	for (int i = 0; i < LIBFS_NUM; i++) {
		if (ls->rid[i]) {
			rpc_lease_flush(ls->rid[i], ls->inum, false);
		}
	}
	uint8_t wid = ls->wid;

	pthread_spin_unlock(&ls->mutex_revoke);

	// wid still exists
	while (ls->holders > 1)
		cpu_relax();

	clear_peer_digesting(g_sync_ctx[wid]->peer);
	pthread_spin_unlock(&ls->mutex);
}

int modify_lease_state(int req_id, int inum, int new_state, int version, addr_t logblock)
{
	mlfs_lease_t* ls = lget(inum);

	if (!ls)
		panic("failed to allocate lease\n");

	if (enable_perf_stats) {
		g_perf_stats.lease_rpc_remote_nr++;
	}

#ifdef LEASE_DEBUG
	mlfs_printf("\e[1;95m[KERNFS] modify lease state (req_id=%d, inum=%d, state=%d->%d)\e[0m\n",
			req_id, inum, ls->state, new_state);
#endif

	if (new_state == LEASE_FREE) {
		// only triggered by revoke_lease or discard_lease
		// state transition and rid & wid deletion
		// will be done by original modify_lease_state function (currently busy waiting)

		pthread_spin_lock(&ls->mutex_revoke);

		if (ls->state == LEASE_FREE) {
			pthread_spin_unlock(&ls->mutex_revoke);
			return -1;
		}
		else if (ls->state == LEASE_READ) {
			int i;
			for (i = 0; i < LIBFS_NUM; i++) {
				if (ls->rid[i] == req_id) {
					ls->rid[i] = 0;
					break;
				}
			}
			if (i == LIBFS_NUM) {
				pthread_spin_unlock(&ls->mutex_revoke);
				return -1;
			}
		}
		else if (ls->state == LEASE_WRITE) {
			if (ls->wid != req_id) {
				pthread_spin_unlock(&ls->mutex_revoke);
				return -1;
			}
			ls->wid = 0;
			ls->lversion = version;
			ls->lblock = logblock;
		}

		ls->holders--;
		if (ls->holders == 0) {
			ls->state = LEASE_FREE;
		}
		pthread_spin_unlock(&ls->mutex_revoke);

		return 1;
	}
	else if (new_state == LEASE_READ) {
		pthread_spin_lock(&ls->mutex);

		pthread_spin_lock(&ls->mutex_revoke);
		if (ls->state == LEASE_FREE) {
			ls->holders++;
			ls->state = new_state;
			ls->rid[0] = req_id;

			clock_gettime(CLOCK_MONOTONIC, &ls->time);
		}
		else if (ls->state == LEASE_READ) {
			ls->holders++;
			for (int i = 0; i < LIBFS_NUM; i++) {
				if (ls->rid[i] == 0) {
					ls->rid[i] = req_id;
					break;
				}
			}
		}
		else {
			// ls->state == LEASE_WRITE
#ifdef LEASE_DEBUG
			mlfs_printf("%s", "\x1B[31m [L] Conflict detected: synchronous digest request and wait! \x1B[0m\n");
#endif

			// do spin lock for minimum guarantee time of libfs lease.
#if 1
			double nsec = 0.0;
			do {
				struct timespec now;
				clock_gettime(CLOCK_MONOTONIC, &now);
				nsec = (now.tv_sec * 1000000000.0 + (double)now.tv_nsec) - (ls->time.tv_sec * 1000000000.0 + (double)ls->time.tv_nsec);
			} while (nsec < LEASE_MIN_NSEC);
#endif
			uint8_t wid = ls->wid;
			pthread_spin_unlock(&ls->mutex_revoke);

			rpc_lease_flush(wid, inum, true);

			// wait until original lease owner relinquishes its lease
			// FIXME: Use CPU instruction PAUSE - Spin Loop Hint
			while (ls->holders > 0)
				cpu_relax();
			pthread_spin_lock(&ls->mutex_revoke);

			// if write holder still exists
			if (g_sync_ctx[wid] != NULL)
				wait_till_digest_checkpoint(g_sync_ctx[wid]->peer, ls->lversion, ls->lblock);

			ls->holders++;
			ls->state = new_state;
			ls->rid[0] = req_id;

			clock_gettime(CLOCK_MONOTONIC, &ls->time);
		}
		pthread_spin_unlock(&ls->mutex_revoke);
	}
	else {
		// new_state == LEASE_WRITE
#ifdef LEASE_DEBUG
		mlfs_printf("%s", "wow1\n");
#endif
		pthread_spin_lock(&ls->mutex);
#ifdef LEASE_DEBUG
		mlfs_printf("%s", "wow2\n");
#endif

		pthread_spin_lock(&ls->mutex_revoke);
#ifdef LEASE_DEBUG
		mlfs_printf("%s", "wow3\n");
#endif
		if (ls->state == LEASE_FREE) {
#ifdef LEASE_DEBUG
			mlfs_printf("%s", "wow (free)\n");
#endif
			ls->holders++;
			ls->state = new_state;
			ls->wid = req_id;

			clock_gettime(CLOCK_MONOTONIC, &ls->time);
		}
		else if (ls->state == LEASE_READ) {
#ifdef LEASE_DEBUG
			mlfs_printf("%s", "\x1B[31m [L] Conflict detected: synchronous digest request and wait! \x1B[0m\n");
#endif
			// do spin lock for minimum guarantee time of libfs lease.
#if 1
			double nsec = 0.0;
			do {
				struct timespec now;
				clock_gettime(CLOCK_MONOTONIC, &now);
				nsec = (now.tv_sec * 1000000000.0 + (double)now.tv_nsec) - (ls->time.tv_sec * 1000000000.0 + (double)ls->time.tv_nsec);
			} while (nsec < LEASE_MIN_NSEC);
#endif

			// Do lazy revokation for READ_LEASE
			// Give WRITE_LEASE first, and block digest

			for (int i = 0; i < LIBFS_NUM; i++) {
				if (ls->rid[i] == req_id) {
					ls->rid[i] = 0;
				}
			}
			ls->wid = req_id;
			ls->state = LEASE_WRITE;
			clock_gettime(CLOCK_MONOTONIC, &ls->time);

#if 0
			write_ondisk_lease(ls);

			for (int i = 0; i < g_n_nodes; i++) {
				if (i == g_self_id)
					continue;
				update_remote_ondisk_lease(i, ls);
			}
#endif

			set_peer_digesting(g_sync_ctx[req_id]->peer);

			thpool_add_work(thread_pool, handle_lazy_revoke, (void *)ls);
			return 1;
		}
		else {
			// ls->state == LEASE_WRITE
#ifdef LEASE_DEBUG
			mlfs_printf("%s", "\x1B[31m [L] Conflict detected: synchronous digest request and wait! \x1B[0m\n");
#endif
			// do spin lock for minimum guarantee time of libfs lease.
#if 1
			double nsec = 0.0;
			do {
				struct timespec now;
				clock_gettime(CLOCK_MONOTONIC, &now);
				nsec = (now.tv_sec * 1000000000.0 + (double)now.tv_nsec) - (ls->time.tv_sec * 1000000000.0 + (double)ls->time.tv_nsec);
			} while (nsec < LEASE_MIN_NSEC);
#endif

#ifdef LEASE_DEBUG
			mlfs_printf("%s", "\x1B[31m [L] Now triggering... \x1B[0m\n");
#endif
			int wid = ls->wid;
			pthread_spin_unlock(&ls->mutex_revoke);

			rpc_lease_flush(wid, inum, true);

			// wait until original lease owner relinquishes its lease
			// FIXME: Use CPU instruction PAUSE - Spin Loop Hint
			while (ls->holders > 0)
				cpu_relax();
			pthread_spin_lock(&ls->mutex_revoke);

			// if write holder still exists
			if (g_sync_ctx[wid] != NULL)
				wait_till_digest_checkpoint(g_sync_ctx[wid]->peer, ls->lversion, ls->lblock);

			ls->holders = 1;
			ls->state = new_state;
			ls->wid = req_id;

			clock_gettime(CLOCK_MONOTONIC, &ls->time);
		}
		pthread_spin_unlock(&ls->mutex_revoke);
	}

	write_ondisk_lease(ls);

	for (int i = 0; i < g_n_nodes; i++) {
		if (i == g_self_id)
			continue;
		update_remote_ondisk_lease(i, ls);
	}

#if 0
	if (do_migrate) {
		if (enable_perf_stats) {
			g_perf_stats.lease_migration_nr++;
		}

		for (int i = 0; i < g_n_nodes; i++) {
			if (i == g_self_id)
				continue;
			rpc_lease_migrate(i, ls->inum, ls->mid);
		}
	}
#endif

	pthread_spin_unlock(&ls->mutex);

	return 1;
}

int clear_lease_checkpoints(int req_id, int version, addr_t log_block)
{
	mlfs_lease_t *item, *tmp;

	mlfs_printf("clearing outdated digest checkpoints for leases belonging to peer ID = %d\n", req_id);

	pthread_spin_lock(&lease_table->guts->mutex);

	//FIXME: keep a list of leases held by req_id to make this go faster
	HASH_ITER(hash_handle, lease_table->guts->hash, item, tmp) {
		//HASH_DELETE(hash_handle, lease_hash, item);
		mlfs_debug("[DEBUG] hash item: lease inum %u state %u\n", item->inum, item->state);

		if (item->wid == req_id && ((version == item->lversion && log_block >= item->lblock)
				|| version > item->lversion)) {

				if (item->lversion || item->lblock) {
					item->lversion = 0;
					item->lblock = 0;
					mlfs_printf("lease digestion checkpoint cleared for inum %u\n", item->inum);
				}
		}

		//mlfs_free(item);
	}

	pthread_spin_unlock(&lease_table->guts->mutex);
}

int discard_leases(int req_id)
{
//#ifdef LAZY_SURRENDER
	mlfs_lease_t *item, *tmp;

	mlfs_printf("discarding all leases for peer ID = %d\n", req_id);

	//pthread_spin_lock(&lcache_spinlock);

	HASH_ITER(hash_handle, lease_table->guts->hash, item, tmp) {
		//HASH_DELETE(hash_handle, lease_hash, item);
		mlfs_debug("[DEBUG] hash item: lease inum %u state %u\n", item->inum, item->state);

		if (item->state == LEASE_WRITE) {
#ifdef LEASE_DEBUG
			mlfs_assert(item->holders == 1);
#endif
			if (item->wid == req_id) {
				modify_lease_state(req_id, item->inum, LEASE_FREE, 0, 0);
				mlfs_debug("write lease discarded for inum %u\n", item->inum);

#if 0
				if (g_sync_ctx[item->wid] != NULL)
					wait_till_digest_checkpoint(g_sync_ctx[item->wid]->peer, item->lversion, item->lblock);
#endif
			}

		}

		else if (item->state == LEASE_READ) {
#ifdef LEASE_DEBUG
			mlfs_assert(item->holders);
#endif
			for (int i = 0; i < LIBFS_NUM; i++) {
				if (item->rid[i] == req_id) {
					modify_lease_state(req_id, item->inum, LEASE_FREE, 0, 0);
					mlfs_debug("read lease discarded for inum %u\n", item->inum);
				}
			}
		}

		//mlfs_free(item);
	}
	//HASH_CLEAR(hash_handle, lease_hash);

	//pthread_spin_unlock(&lcache_spinlock);
//#endif
	return 0;
}

int update_lease_manager(uint32_t inum, uint32_t new_kernfs_id)
{
	mlfs_lease_t* ls = lcache_find(inum);

	if (ls) {
		mlfs_printf("migrate - inum %u update lease manager from %u to %u\n",
				inum, ls->mid, new_kernfs_id);
		ls->mid = new_kernfs_id;
	}

	return 0;
}

#if 0
// modify lease state (this call is potentially blocking)
// FIXME: use locks!
int modify_lease_state(int libfs_id, int inum, int req_type, int log_version, addr_t log_block)
{
	struct inode* ip = icache_find(g_root_dev, inum);
	if (!ip) {
		struct dinode dip;
		ip = icache_alloc_add(g_root_dev, inum);

		read_ondisk_inode(g_root_dev, inum, &dip);

		// FIXME: possible synchronization issue
		// Sometimes LibFS requests lease for a dirty (undigested) inode
		// As a temporary workaround, just return if this happens

		if(dip.itype == 0)
			return 1;
		//mlfs_assert(dip.itype != 0);

		sync_inode_from_dinode(ip, &dip);

		ip->i_sb = sb;

		if(dip.dev == 0)
			return 1;
		//mlfs_assert(dip.dev != 0);
	}

	mlfs_info("modifying lease inum[%u] from %d to %d Req. ID = %d Holder ID= %d (num holders = %u)\n",
			inum, ip->lstate, req_type, libfs_id, ip->lhid, ip->lholders);

	// Cases with no conflicts:

	// (a) LibFS is giving up its lease
	if(req_type == LEASE_FREE) {
		if(ip->lstate == LEASE_WRITE) {
			mlfs_assert(ip->lhid == libfs_id);
			mlfs_assert(ip->lholders == 1);

			ip->lstate = LEASE_DIGEST_TO_ACQUIRE;
			//ip->lstate = req_type;
		}
		else if(ip->lstate == LEASE_READ || ip->lstate == LEASE_READ_REVOCABLE) {
			// FIXME: temporarily commenting assertion
			//mlfs_assert(ip->lholders > 0);

			//ip->lholders = 1;
			;
		}
		else {
			return 1;
			//panic("undefined codepath");
		}

		//ip->lhid = g_self_id; //set id to self
		if(ip->lholders > 0)
			ip->lholders--;
		//bitmap_clear(ip->libfs_ls_bitmap, libfs_id, 1);
		goto PERSIST;
	}

#ifdef LEASE_MIGRATION
	// We are trying to acquire the lease; update ltime and check if we need to migrate

	// update lease acquisition time if it's being accessed from a different node
	if(local_kernfs_id(ip->lhid) != local_kernfs_id(libfs_id))
		mlfs_get_time(&ip->ltime);


	// consecutive lease requests from same LibFS & requester's local kernfs isn't manager
	else if(local_kernfs_id(libfs_id) != g_self_id) {
		mlfs_time_t now;
		mlfs_time_t elaps;
		mlfs_get_time(&now);
		timersub(&now, &ip->ltime, &elaps);
		double sec = (double)(elaps.tv_sec * 1000000.0 + (double)elaps.tv_usec) / 1000000.0;

		mlfs_debug("Lease elapsed time: %f\n", sec);

		if(sec >= 1) {
			// migrate lease management to local kernfs
			ip->lmid = local_kernfs_id(ip->lhid);

			mlfs_debug("Migrating lease to KernFS ID = %d\n", ip->lmid);

			// flush all caches for inode
			int pos = 0;
			while(!bitmap_empty(ip->libfs_ls_bitmap, MAX_LIBFS_PROCESSES)) {
				pos = find_next_bit(ip->libfs_ls_bitmap, MAX_LIBFS_PROCESSES, pos);
				if(pos != libfs_id) {
					rpc_lease_flush(pos, ip->inum, false);
				}
				bitmap_clear(ip->libfs_ls_bitmap, pos, 1);
			}
		}
	}

#endif

	// (b) Multiple concurrent LibFS readers
	if((ip->lstate == LEASE_READ || ip->lstate == LEASE_READ_REVOCABLE)
			&& (req_type == LEASE_READ || req_type == LEASE_READ_REVOCABLE)) {
		ip->lhid = libfs_id;
		bitmap_set(ip->libfs_ls_bitmap, libfs_id, 1);
		if(req_type == LEASE_READ) {
			ip->lholders++;
			goto PERSIST;
		}
		else
			return 1;
	}

	// if I already hold an existing lease, remove myself and decrement lholders
	// NOTE: this shouldn't really happen since we surrender leases after closing a file/dir.
	// Should we panic?
	if(ip->lstate == LEASE_WRITE && ip->lholders > 0 && ip->lhid == libfs_id) {
		//mlfs_assert(ip->lholders > 0);
		//bitmap_clear(ip->libfs_ls_bitmap, libfs_id, 1);
		ip->lholders--;
	}

	// (c) remaining cases can involve conflicts if there are other lease holders

	// TODO: Add timeout mechanism to prevent prepetual stalling
	// if a LibFS holds a lease for too long (maliciously or otherwise)
	while(1) {	
		if(!cmpxchg(&ip->lholders, 0, 1)) {
			if(ip->lstate == LEASE_WRITE || ip->lstate == LEASE_DIGEST_TO_ACQUIRE
					|| ip->lstate == LEASE_DIGEST_TO_READ) {
				// check if we need to digest anything
				if(ip->lhid != libfs_id) {
					mlfs_printf("DEBUG ip->lhid %d req_id %d\n", ip->lhid, libfs_id);
					if(!rpc_lease_flush(ip->lhid, ip->inum, true))
						wait_till_digest_checkpoint(g_sync_ctx[ip->lhid]->peer, ip->lversion, ip->lblock);

					bitmap_clear(ip->libfs_ls_bitmap, ip->lhid, 1);
				}
				else {
					// if a LibFS process is downgrading its lease
					// make sure to digest before another acquires it
					mlfs_assert(req_type == LEASE_READ || req_type == LEASE_READ_REVOCABLE
							|| req_type == LEASE_WRITE);

					if(req_type == LEASE_READ)
						req_type = LEASE_DIGEST_TO_READ;
					else if(req_type == LEASE_READ_REVOCABLE)
						req_type = LEASE_DIGEST_TO_ACQUIRE;
				}	
			}

			int pos = 0;
			while(!bitmap_empty(ip->libfs_ls_bitmap, MAX_LIBFS_PROCESSES)) {
				pos = find_next_bit(ip->libfs_ls_bitmap, MAX_LIBFS_PROCESSES, pos);
				if(pos != libfs_id) {
					//FIXME: temporarily disabling icache flushes to avoid issue with persist_log_inode()
					//rpc_lease_flush(pos, ip->inum, false);
				}
				bitmap_clear(ip->libfs_ls_bitmap, pos, 1);
			}

			ip->lhid = libfs_id;
			ip->lstate = req_type;
			ip->lversion = log_version;

			// revocable leases don't increase lholder count
			if(ip->lstate == LEASE_READ_REVOCABLE)
				ip->lholders--;

			bitmap_set(ip->libfs_ls_bitmap, libfs_id, 1);
			goto PERSIST;
		}

		while(ip->lholders > 0)
			cpu_relax();
	}
//#if 0
PERSIST:

	write_ondisk_inode(ip);
	
	for(int i=0; i<g_n_nodes;i++) {
		if(i == g_self_id)
			continue;
		update_remote_ondisk_inode(i, ip);
	}
//#endif
	return 1;

}

#endif

#endif

#if 0
int resolve_lease_conflict(int sockfd, char *path, int type, uint64_t seqn)
{
	struct inode *ip;

	ip = namei(path);

	mlfs_info("trying to resolve conflicts [requested: %d, current: %d] for %s\n",
			type, ip->ilease, path);

	if(!ip) {
		mlfs_printf("%s", "trying to acquire lease for non-existing inode\n");
		assert(0);
	}

	int old_ilease = ip->ilease;
	int new_ilease = lease_downgrade(type, ip->ilease);

	if(new_ilease) {
		//ensure that inode is not locked
		ilock(ip);
		iunlock(ip);

		//close fd (if already open)
		struct file *f;
		HASH_FIND_STR(g_fd_table.open_files_ht, path, f);
		if(f) {
			assert(f->ref > 0);
			mlfs_posix_close(f->fd);
		}

		if(new_ilease == LEASE_NONE) {
			purge_dir_caches(ip, path);
		}

		ip->ilease = new_ilease;
	}

	// only replicate data if we had a write lease that was revoked
	//FIXME: locking!
	int do_replicate = (old_ilease == LEASE_WRITE && new_ilease && atomic_load(&get_peer()->n_unsync_blk) > 0);
	mlfs_info("replicate:%s old_lease %d new_lease %d n_unsync_blk %lu\n",
			do_replicate?"yes":"no", old_ilease, new_ilease, atomic_load(&get_peer()->n_unsync_blk));
	rpc_lease_response(sockfd, seqn, do_replicate);
	if(do_replicate) {
		//triggering a local digest will automatically sync logs and digest remotely
		while(make_digest_request_async(100) != -EBUSY);
		//make_digest_request_async()
		//mlfs_do_rdigest();
	}

	mlfs_debug("lease downgraded path %s type %d\n", path, type);
}
#endif

#endif
