#include "filesystem/fs.h"
#include "io/block_io.h"
#include "log/log.h"
#include "global/global.h"
#include "concurrency/thread.h"

#if MLFS_LEASE
#include "experimental/leases.h"
#endif

#if MLFS_NAMESPACES
#include "distributed/rpc_interface.h"
#endif

int namecmp(const char *s, const char *t)
{
	return strncmp(s, t, DIRSIZ);
}

int get_dirent(struct inode *dir_inode, struct mlfs_dirent *buf, offset_t offset)
{
	struct mlfs_reply reply;

	if (offset >= dir_inode->size)
		return 0;

	reply.remote = 0;
	reply.dst = (uint8_t *) buf;
	return readi(dir_inode, &reply, offset, sizeof(struct mlfs_dirent), NULL);
}

struct inode *dir_lookup(struct inode *dir_inode, char *name, offset_t *poff)
{
	struct mlfs_dirent de;
	struct inode *ip = NULL;
	int n_de_cache = 0;
	offset_t off;

	ip = de_cache_find(dir_inode, name, poff);

	if (ip)
		return ip;

	mlfs_debug("dir_lookup: de_cache miss for dir %u, name %s\n", dir_inode->inum, name);

	n_de_cache = dir_inode->n_de_cache_entry + 2;
	if (n_de_cache * sizeof(struct mlfs_dirent) == dir_inode->size) {
		mlfs_debug("%s\n", "not found w/ full de cache - skipping iteration");
		return NULL;
	}

	mlfs_debug("dir_lookup: starting search for name %s (dirs: cached %d total %ld)\n",
		name, n_de_cache, dir_inode->size / sizeof(struct mlfs_dirent));

	// iterate through file's dirent until finding name match
	for (off = n_de_cache * sizeof(struct mlfs_dirent); off < dir_inode->size; off += sizeof(struct mlfs_dirent)) {
		if (get_dirent(dir_inode, &de, off) != sizeof(struct mlfs_dirent))
			break;

		ip = icache_find(de.inum);

		if(!ip) {
#if MLFS_NAMESPACES
			ip = iget(de.inum | (dir_inode->inum & g_namespace_mask));
#else
			ip = iget(de.inum);
#endif
		}

		// add entry to cache
		de_cache_add(dir_inode, de.name, ip, off);

		if (!namecmp(name, de.name)) {
			//pthread_rwlock_unlock(g_debug_rwlock);
			*poff = off;
			return ip;
		}
	}

	mlfs_debug("dir_lookup: did not find %s in dir %u\n", name, dir_inode->inum);
	return NULL;
}

/* linux_dirent must be identical to gblic kernel_dirent
 * defined in sysdeps/unix/sysv/linux/getdents.c */
int dir_get_entry(struct inode *dir_inode, struct linux_dirent *buf, offset_t off)
{
	struct mlfs_dirent de;
	int ret;

	ret = get_dirent(dir_inode, &de, off);

#if MLFS_NAMESPACES
	buf->d_ino = de.inum | (dir_inode->inum & g_namespace_mask);
#else
	buf->d_ino = de.inum;
#endif

	buf->d_off = (off/sizeof(struct mlfs_dirent)) * sizeof(struct linux_dirent);
	buf->d_reclen = sizeof(struct linux_dirent);
	strncpy(buf->d_name, de.name, DIRSIZ);

	return sizeof(struct mlfs_dirent);
}

/* linux_dirent must be identical to gblic kernel_dirent
 * defined in sysdeps/unix/sysv/linux/getdents.c */
int dir_get_entry64(struct inode *dir_inode, struct linux_dirent64 *buf, offset_t off)
{
	struct mlfs_dirent de;
	int ret;

	ret = get_dirent(dir_inode, &de, off);

#if MLFS_NAMESPACES
	buf->d_ino = de.inum | (dir_inode->inum & g_namespace_mask);
#else
	buf->d_ino = de.inum;
#endif

	buf->d_off = (off/sizeof(struct mlfs_dirent)) * sizeof(struct linux_dirent64);
	buf->d_reclen = sizeof(struct linux_dirent64);
	strncpy(buf->d_name, de.name, DIRSIZ);

	return sizeof(struct mlfs_dirent);
}


struct mlfs_dirent *dir_change_entry(struct inode *dir_inode, char *oldname, char *newname)
{
	struct inode *ip;
	struct mlfs_dirent *new_de;
	offset_t de_off;

	ip = dir_lookup(dir_inode, oldname, &de_off);
	if (!ip) 
		return NULL;

	de_cache_del(dir_inode, oldname);

	// find and update dirent, writing back to log
	new_de = mlfs_zalloc(sizeof(struct mlfs_dirent));
#if MLFS_NAMESPACES
	new_de->inum = ip->inum & ~g_namespace_mask;
#else
	new_de->inum = ip->inum;
#endif
	strncpy(new_de->name, newname, DIRSIZ);
	add_to_log(dir_inode, (uint8_t *) new_de, de_off, sizeof(struct mlfs_dirent), L_TYPE_DIR_RENAME);

	de_cache_add(dir_inode, newname, ip, de_off);

	iput(ip);
	return new_de;
}

struct mlfs_dirent *dir_remove_entry(struct inode *dir_inode, char *name, struct inode **found)
{
	struct inode *ip = NULL;
	struct mlfs_dirent *last = NULL;
	offset_t last_off;
	offset_t de_off;

	ip = dir_lookup(dir_inode, name, &de_off);
	if (!ip) {
		*found = NULL;
		return NULL;
	}

	// set pointer to inode for caller to use
	*found = ip;
	de_cache_del(dir_inode, name);

	// if dirent is not last in file, swap in last entry to fill empty slot
	last_off = dir_inode->size - sizeof(struct mlfs_dirent);
	if (de_off < last_off) {
		last = mlfs_zalloc(sizeof(struct mlfs_dirent));
		get_dirent(dir_inode, last, last_off);
		de_cache_del(dir_inode, last->name);

		add_to_log(dir_inode, (uint8_t *) last, de_off, sizeof(struct mlfs_dirent), L_TYPE_DIR_ADD);
#if MLFS_NAMESPACES
		ip = iget(last->inum | (dir_inode->inum & g_namespace_mask));
#else
		ip = iget(last->inum);
#endif
		de_cache_add(dir_inode, last->name, ip, de_off);
		iput(ip);
	}

	// truncate space at end of directory file
	itrunc(dir_inode, last_off);

#if MLFS_LEASE
	//struct logheader_meta *loghdr_meta;

	//loghdr_meta = get_loghdr_meta();
	//mark_lease_revocable(dir_inode->inum, loghdr_meta->hdr_blkno);
#endif

	return last;
}

struct mlfs_dirent *dir_add_links(struct inode *dir_inode, uint32_t inum, uint32_t parent_inum)
{
	struct mlfs_dirent *link_de;

	link_de = mlfs_zalloc(sizeof(struct mlfs_dirent) * 2);
#if MLFS_NAMESPACES
	link_de[0].inum = inum & ~g_namespace_mask;
	link_de[1].inum = parent_inum & ~g_namespace_mask;
#else
	link_de[0].inum = inum;
	link_de[1].inum = parent_inum;
#endif
	strcpy(link_de[0].name, ".");
	strcpy(link_de[1].name, "..");

	// add "." and ".." links in a single log entry
	add_to_log(dir_inode, (uint8_t *) link_de, 0, sizeof(struct mlfs_dirent) * 2, L_TYPE_DIR_ADD);

	return link_de;
}

struct mlfs_dirent *dir_add_entry(struct inode *dir_inode, char *name, struct inode *ip)
{
	struct mlfs_dirent *new_de;
	offset_t off;

	new_de = mlfs_zalloc(sizeof(struct mlfs_dirent));
#if MLFS_NAMESPACES
	new_de->inum = ip->inum & ~g_namespace_mask;
#else
	new_de->inum = ip->inum;
#endif
	strncpy(new_de->name, name, DIRSIZ);
	off = dir_inode->size;

	// append to directory file
	mlfs_debug("adding new dirent to dir inode %u: %s ~ %u at offset %lu\n", dir_inode->inum, name, ip->inum, dir_inode->size);
	add_to_log(dir_inode, (uint8_t *) new_de, off, sizeof(struct mlfs_dirent), L_TYPE_DIR_ADD);

	de_cache_add(dir_inode, name, ip, off);

	return new_de;
}

// Paths
// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode* namex(char *path, int parent, char *name)
{
	mlfs_info("namex: path %s, parent %d, name %s\n", path, parent, name);

	struct inode *ip, *next;
	offset_t off;
	char namespace_id[DIRSIZ] = {'\0'};
	uint32_t namespace_inum;

	if (*path == '/') {
#if MLFS_NAMESPACES
		if (sscanf(path, "/mlfs:%[^/]", namespace_id) == 1) {
			mlfs_printf("parsed namespace id: '%s'\n", namespace_id);
			namespace_inum = translate_namespace_inum(ROOTINO, namespace_id);
			ip = iget(namespace_inum);
		} else {
			mlfs_printf("no namespace id in path '%s'\n", path);
			ip = iget(ROOTINO);
		}

#else
		ip = iget(ROOTINO);
#endif
	}
	else
		//ip = idup(proc->cwd);
		panic("relative path is not yet implemented\n");

	// directory walking of a given path
	while ((path = get_next_name(path, name)) != 0) {

		ilock(ip);
		if (ip->itype != T_DIR){
			iunlockput(ip);
			return NULL;
		}
		if (parent && *path == '\0') {
			// Stop one level early.
			iunlock(ip);
			return ip;
		}
#if MLFS_LEASE
		// acquire_lease(ip->inum, LEASE_READ, NULL);
#endif
		if ((next = dir_lookup(ip, name, &off)) == NULL) {
			iunlockput(ip);
			return NULL;
		}
#if MLFS_LEASE
		// mark_lease_revocable(ip->inum);
#endif

		iunlockput(ip);
		ip = next;
	}

	if (parent) {
		iput(ip);
		return NULL;
	}

	mlfs_debug("inum %u - refcount %d\n", ip->inum, ip->i_ref);
	return ip;
}

struct inode* namei(char *path)
{
#if 0 // This is for debugging.
	struct inode *inode, *_inode;
	char name[DIRSIZ];

	_inode = dlookup_find(g_root_dev, path);

	if (!_inode) {
		inode = namex(path, 0, name);
		if (inode)
			dlookup_alloc_add(g_root_dev, inode, path);
	} else {
		inode = namex(path, 0, name);
		mlfs_assert(inode == _inode);
	}

	return inode;
#else
	struct inode *inode;
	char name[DIRSIZ];

	inode = dlookup_find(path);

	if (inode && (inode->flags & I_DELETING))
		return NULL;

	if (!inode) {
		inode = namex(path, 0, name);
		if (inode) {
			dlookup_alloc_add(inode, path);
		}
	}

	return inode;
#endif
}

struct inode* nameiparent(char *path, char *name)
{
#if 0 // This is for debugging.
	struct inode *inode, *_inode;
	char parent_path[MAX_PATH];

	get_parent_path(path, parent_path);

	_inode = dlookup_find(g_root_dev, parent_path);

	if (!_inode) {
		inode = namex(path, 1, name);
		if (inode)
			dlookup_alloc_add(g_root_dev, inode, parent_path);
		_inode = inode;
	} else {
		inode = namex(path, 1, name);
		mlfs_assert(inode == _inode);
	}

	return inode;
#else
	struct inode *inode;
	char parent_path[MAX_PATH];

	get_parent_path(path, parent_path, name);

	inode = dlookup_find(parent_path);

	if (inode && (inode->flags & I_DELETING))
		return NULL;

	if (!inode) {
		inode = namex(path, 1, name);
		if (inode)
			dlookup_alloc_add(inode, parent_path);
	} else {
		ilock(inode);
		inode->i_ref++;
		iunlock(inode);
	}

	return inode;
#endif
}
