#include "config.h"
#include "filebench.h"
#include "flowop.h"
#include "threadflow.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <strings.h>
#include <stdint.h>
#include <inttypes.h>
#include "filebench.h"
#include "fsplug.h"

#include "uthash.h"
#include "utlist.h"

#define mb ((uint64_t)1024*1024)

/***************************************************************************
****************************************************************************
****************************************************************************
**********************  CHANGE DEVICE SIZES AND PATHS HERE   ***************
****************************************************************************
****************************************************************************
****************************************************************************/
static char* dev_paths[] = {
    "./pmem",
    "./ssd",
    "/backup/filebench",
};

static uint64_t dev_sizes[] = {
    32168 *mb, 120 * 1024*mb , 400 *1024*mb,
};

#define DEBUG 0

/***************************************************************************
****************************************************************************
****************************************************************************
********************  CHANGE DEVICE SIZES AND PATHS ABOVE   ****************
****************************************************************************
****************************************************************************
****************************************************************************/


#define dbg(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

/*
 * These routines implement local file access. They are placed into a
 * vector of functions that are called by all I/O operations in fileset.c
 * and flowop_library.c. This represents the default file system plug-in,
 * and may be replaced by vectors for other file system plug-ins.
 */

static int fb_umfs_freemem(fb_fdesc_t *fd, off64_t size);
static int fb_umfs_open(fb_fdesc_t *, char *, int, int);
static int fb_umfs_pread(fb_fdesc_t *, caddr_t, fbint_t, off64_t);
static int fb_umfs_read(fb_fdesc_t *, caddr_t, fbint_t);
static int fb_umfs_pwrite(fb_fdesc_t *, caddr_t, fbint_t, off64_t);
static int fb_umfs_write(fb_fdesc_t *, caddr_t, fbint_t);
static int fb_umfs_lseek(fb_fdesc_t *, off64_t, int);
static int fb_umfs_truncate(fb_fdesc_t *, off64_t);
static int fb_umfs_rename(const char *, const char *);
static int fb_umfs_close(fb_fdesc_t *);
static int fb_umfs_link(const char *, const char *);
static int fb_umfs_symlink(const char *, const char *);
static int fb_umfs_unlink(char *);
static ssize_t fb_umfs_readlink(const char *, char *, size_t);
static int fb_umfs_mkdir(char *, int);
static int fb_umfs_rmdir(char *);
static DIR *fb_umfs_opendir(char *);
static struct dirent *fb_umfs_readdir(DIR *);
static int fb_umfs_closedir(DIR *);
static int fb_umfs_fsync(fb_fdesc_t *);
static int fb_umfs_stat(char *, struct stat64 *);
static int fb_umfs_fstat(fb_fdesc_t *, struct stat64 *);
static int fb_umfs_access(const char *, int);
static void fb_umfs_recur_rm(char *);

static fsplug_func_t fb_umfs_funcs =
{
	"umfs",
	fb_umfs_freemem,		/* flush page cache */
	fb_umfs_open,		/* open */
	fb_umfs_pread,		/* pread */
	fb_umfs_read,		/* read */
	fb_umfs_pwrite,		/* pwrite */
	fb_umfs_write,		/* write */
	fb_umfs_lseek,		/* lseek */
	fb_umfs_truncate,	/* ftruncate */
	fb_umfs_rename,		/* rename */
	fb_umfs_close,		/* close */
	fb_umfs_link,		/* link */
	fb_umfs_symlink,		/* symlink */
	fb_umfs_unlink,		/* unlink */
	fb_umfs_readlink,	/* readlink */
	fb_umfs_mkdir,		/* mkdir */
	fb_umfs_rmdir,		/* rmdir */
	fb_umfs_opendir,		/* opendir */
	fb_umfs_readdir,		/* readdir */
	fb_umfs_closedir,	/* closedir */
	fb_umfs_fsync,		/* fsync */
	fb_umfs_stat,		/* stat */
	fb_umfs_fstat,		/* fstat */
	fb_umfs_access,		/* access */
	fb_umfs_recur_rm		/* recursive rm */
};


struct um_file {
    char filename[256];
    char internal_path[128];

    int device_id;
    char moved;
    uint64_t size;

    struct um_file* next;
    struct um_file* prev;

    UT_hash_handle hh;
};

struct fd_dev_mapping {
    int fd;
    struct um_file* file;
    UT_hash_handle hh;
};

struct umfs_t {
    uint64_t dev_space_left[3];
    int file_id_counter;
    struct um_file* filepath_hash;
    struct fd_dev_mapping* fd_hash;
    struct um_file* head;
};

//umfs contains the whole state of the fs
struct umfs_t umfs;
int umfs_initd = 0;

/*
 * Marshalls the state of the filesystem to a file, so the child
 * can read it
 */
void umfs_marshall(void)
{
    int pid = getpid();
    char mfile[64];
    sprintf(mfile, "%d.umfs", pid);

    int fd = open(mfile, O_CREAT | O_TRUNC | O_RDWR, 0666);

    //write main struct
    write(fd, &umfs, sizeof(struct umfs_t));

    //write # of files, then files
    struct um_file *it;
    int files;
    CDL_COUNT(umfs.head, it, files);
    dbg("marshalling %d files\n", files);
    write(fd, &files, sizeof(int));

    //write the files in LRU order
    CDL_FOREACH(umfs.head, it) {
        write(fd, it, sizeof(struct um_file));
        dbg("\t\twrote %s\n", it->filename);
    }
    
    fsync(fd);
    close(fd);
}

/*
 * Reads the fs state from a file
 */
void umfs_unmarshall(char* fname)
{
    int fd = open(fname, O_RDONLY);

    //read main data struct and adjust hash and list
    read(fd, &umfs, sizeof(struct umfs_t));
    umfs.head = NULL;
    umfs.filepath_hash =  NULL;
    umfs.fd_hash =  NULL;

    int nfiles;
    read(fd, &nfiles, sizeof(int));
    
    int i;
    for(i = 0 ; i < nfiles ; i++) {
        //allocate memory and read from file
        struct um_file *f = malloc(sizeof(struct um_file));
        read(fd, f, sizeof(struct um_file));
        f->next = f->prev = NULL;
        //printf("> %"PRId64"\n", f->size);
        //add to hash and LRU list
        HASH_ADD_STR(umfs.filepath_hash, filename, f);
        CDL_APPEND(umfs.head, f);
    }
    close(fd);
}

void umfs_init(void) {
    if (umfs_initd) return;

    int ppid = getppid();
    char mfile[64];
    sprintf(mfile, "%d.umfs", ppid);

    //parent/master, normal init
    if (access(mfile, F_OK) == -1) {
        dbg("INIT parent%s", "\n");
        //devices start empty
        umfs.dev_space_left[0] = dev_sizes[0];
        umfs.dev_space_left[1] = dev_sizes[1];
        umfs.dev_space_left[2] = dev_sizes[2];

        //start hashes and linked list
        umfs.head = NULL;
        umfs.filepath_hash =  NULL;
        umfs.fd_hash =  NULL;

    	umfs.file_id_counter = 1;
    }
    //child/worker
    //there exists a state file, need to read it 
    else {
        dbg("INIT child%s", "\n");
        umfs_unmarshall(mfile);
    } 
    
    //init'd now
    umfs_initd = 1;
}


/*
 * Initialize file system functions vector to point to the vector of local file
 * system functions. This function will be called for the master process and
 * every created worker process.
 */
void
fb_umfs_funcvecinit(void)
{
	fs_functions_vec = &fb_umfs_funcs;
}

/*
 * Initialize those flowops which implementation is file system specific. It is
 * called only once in the master process.
 */
void
fb_umfs_newflowops(void)
{
    fb_umfs_recur_rm("./tmp0/*");
    fb_umfs_recur_rm("./tmp1/*");
    fb_umfs_recur_rm("./tmp2/*");
    fb_umfs_recur_rm("./*.umfs");
}

/*
 * Do a posix close of a file. Return what close() returns.
 */
static int
fb_umfs_close(fb_fdesc_t *fd)
{
    //dbg(">> close%s", "\n");
    struct fd_dev_mapping* map;
    HASH_FIND_INT(umfs.fd_hash, &(fd->fd_num), map);
    
    //fd is not mapped
    if(map == NULL) {
        printf("PANIC on close!\n");
        exit(1);
    }

    HASH_DEL(umfs.fd_hash, map);
    free(map);
    //dbg("<< close%s", "\n");
	return (close(fd->fd_num));
}

/*
 * Does an open64 of a file. Inserts the file descriptor number returned
 * by open() into the supplied filebench fd. Returns FILEBENCH_OK on
 * successs, and FILEBENCH_ERROR on failure.
 */

static int
fb_umfs_open_internal(int *fd, char *path, int flags, int perms)
{
    //dbg(">> open%s", "\n");
    umfs_init();
    struct um_file *file;
    HASH_FIND_STR(umfs.filepath_hash, path, file);

    //file is not mapped, need to create
    if(file == NULL) {
        file = malloc(sizeof(struct um_file));
        strncpy(file->filename, path, 255);
        sprintf(file->internal_path, "%s/%d", dev_paths[0], umfs.file_id_counter++);
        file->device_id = 0;
        file->size = 0;
        file->moved = 0;
        //dbg("Mapping new file %s\tto\to%s\n", file->filename, file->internal_path);

        HASH_ADD_STR(umfs.filepath_hash, filename, file);
    }
    //its mapped, just remove from lru list
    else {
        //remove from LRU, we re-add later
        CDL_DELETE(umfs.head, file);
    }

    //put file into LRU list head
    CDL_PREPEND(umfs.head, file);

    if ((*fd = open64(file->internal_path, flags , perms)) < 0) 
		return (FILEBENCH_ERROR);
	else {
        //add this fd/device mapping to hash
        struct fd_dev_mapping* map = malloc(sizeof(struct fd_dev_mapping));
        map->fd = *fd;
        map->file = file;
        //add to fd/device hash
        HASH_ADD_INT(umfs.fd_hash, fd, map);
		return (FILEBENCH_OK);
    }
}

static int
fb_umfs_open(fb_fdesc_t *fd, char *path, int flags, int perms) {
    return fb_umfs_open_internal(&(fd->fd_num), path, flags, perms);
}

/*
 * Do a write to a file.
 */
static int
fb_umfs_write_internal(int *fd, caddr_t iobuf, fbint_t iosize)
{
	int ret;
    //get the file of this fd
    struct fd_dev_mapping* map;
    HASH_FIND_INT(umfs.fd_hash, fd, map);
    //trying to write to an non-existing fd
    if(!map) {
        printf("PANIC on write!\n");
        exit(1);
    }

    struct um_file* file = map->file;
    //if this var is set, it was moved to a different, device
    //need to reopen
    if(file->moved) {
        dbg("File moved, need to reopen%d\n",0);
        close(*fd);
        HASH_DEL(umfs.fd_hash, map);
        free(map);
        
        fb_umfs_open_internal(fd, file->filename, O_RDWR, 0644);
        file->moved = 0;
    }
    //there is not enough space, need to move files down
    while(umfs.dev_space_left[file->device_id] < iosize) {
        dbg("\tWant to write %d, but device %d is full,%" PRId64 " bytes left\n", 
            (unsigned)iosize, file->device_id, umfs.dev_space_left[file->device_id]);

        struct um_file* old;
        //find the first file thats is in our device
        for(old = umfs.head->prev ; 
                old->device_id != file->device_id ; 
                old = old->prev )
            ;

        dbg("\tMoving down %s (%d KB) from device %d (%"PRId64" KB left) to device %d (%"PRId64" KB left)\n", 
            old->internal_path,
            (int) old->size/1024,
            old->device_id, 
            umfs.dev_space_left[old->device_id]/1024,
            old->device_id+1, 
            umfs.dev_space_left[old->device_id+1]/1024);

        //move file down:        
        //get fd to file to read/delete
        int old_fd = open64(old->internal_path, O_RDWR, 0);
        if(old_fd == -1) {
            printf("OPEN64 ERROR\n");
            exit(1);
        }

        //create buffer, read contents, unlink, update device size
        char* buffer = malloc(old->size);
        read(old_fd, buffer, old->size);
        close(old_fd);
        unlink(old->internal_path);
        
        umfs.dev_space_left[old->device_id] += old->size;

        //set device to next, update internal path
        old->device_id += 1;
        if(old->device_id > 2) {
            printf("Moving to device that doesn't exist, not enough space!");
            exit(1);
        }
        sprintf(old->internal_path, "%s/%d", 
            dev_paths[old->device_id], umfs.file_id_counter++);
        //printf("\tNew path will be %s\n", old->internal_path);
        
        //create file in new device
        int new_fd = open64(old->internal_path, O_RDWR | O_CREAT, 0644);
        
        //adding to map since we use this write recursively
        struct fd_dev_mapping* new_map = malloc(sizeof(struct fd_dev_mapping));
        new_map->fd = new_fd;
        new_map->file = old;
        HASH_ADD_INT(umfs.fd_hash, fd, new_map);

        //touch in LRU so we dont move the same file twice
        CDL_DELETE(umfs.head, old);
        CDL_PREPEND(umfs.head, old);

        //recursive call to write
        dbg("\tRecursive write of size %"PRId64"\n", old->size);
        //write will add the io_size to file size, so we save to decrement later
        int old_size = old->size;
        fb_umfs_write_internal(&new_fd, buffer, old->size);
		fsync(new_fd);

        //write incremented size, we put it back to correct
        old->size -= old_size;
        //finished moving, set moved flag
        old->moved = 1;
        
        //close and remove from open fds
        close(new_fd);
        HASH_DEL(umfs.fd_hash, new_map);
        free(buffer);
        free(new_map);

        dbg("\tMoving down is done%d\n",0);
    }

    file->size += iosize;
    umfs.dev_space_left[file->device_id] -= iosize;

    //dbg("Writing %d KB to device %d, it has now %"PRId64" KB left\n", 
    //    (int)iosize/1024, file->device_id, umfs.dev_space_left[file->device_id]/1024);

    //touch file in LRU
    CDL_DELETE(umfs.head, file);
    CDL_PREPEND(umfs.head, file);

    //dbg("<< write%s", "\n");
	//return (write(*fd, iobuf, iosize));
	ret = write(*fd, iobuf, iosize);

	fsync(*fd);

	return ret;
}


static int
fb_umfs_write(fb_fdesc_t *fd, caddr_t iobuf, fbint_t iosize) {
    return fb_umfs_write_internal(&(fd->fd_num), iobuf, iosize);
}

/*
 * Does an unlink (delete) of a file.
 */
static int
fb_umfs_unlink(char *path)
{
    //printf(">> unlink %s\n", path);
    struct um_file *file;
    HASH_FIND_STR(umfs.filepath_hash, path, file);

    if(file == NULL) {
        printf("Unlink non-existent file: %s\n", path);
        return 0;
        exit(1);
    }

    int ret = unlink(file->internal_path);

    HASH_DEL(umfs.filepath_hash, file);
    CDL_DELETE(umfs.head, file);
    umfs.dev_space_left[file->device_id] += file->size;
    free(file);

    //dbg("<< unlink%s", "\n");
    return ret;
}

/*
 * Do a posix lseek of a file. Return what lseek() returns.
 */
static int
fb_umfs_lseek(fb_fdesc_t *fd, off64_t offset, int whence)
{
    //NOP for now since we only do whole read and append
    //return offset;
	return (lseek64(fd->fd_num, offset, whence));
}

/*
 * Does an fstat of a file.
 */
static int
fb_umfs_fstat(fb_fdesc_t *fd, struct stat64 *statbufp)
{
	return (fstat64(fd->fd_num, statbufp));
}

/*
 * Does a stat of a file.
 */
static int
fb_umfs_stat(char *path, struct stat64 *statbufp)
{   
    struct um_file *file;
    HASH_FIND_STR(umfs.filepath_hash, path, file);

    if(file == NULL) {
        return -1;
    }

	return (stat64(file->internal_path, statbufp));
}



/*
 *
 *      Everything below here will not implemented
 *
 * 
 */



/*
 * Frees up memory mapped file region of supplied size. The
 * file descriptor "fd" indicates which memory mapped file.
 * If successful, returns 0. Otherwise returns -1 if "size"
 * is zero, or -1 times the number of times msync() failed.
 */
static int
fb_umfs_freemem(fb_fdesc_t *fd, off64_t size)
{
	printf("PANIC fb_umfs_freemem\n");
    exit(1);
}

/*
 * Does a posix pread. Returns what the pread() returns.
 */
static int
fb_umfs_pread(fb_fdesc_t *fd, caddr_t iobuf, fbint_t iosize, off64_t fileoffset)
{
	printf("PANIC fb_umfs_pread\n");
    exit(1);
	//return (pread64(fd->fd_num, iobuf, iosize, fileoffset));
}

/*
 * Does a posix read. Returns what the read() returns.
 */
static int
fb_umfs_read(fb_fdesc_t *fd, caddr_t iobuf, fbint_t iosize)
{
	return (read(fd->fd_num, iobuf, iosize));
}

/*
 * Does a readlink of a symbolic link.
 */
static ssize_t
fb_umfs_readlink(const char *path, char *buf, size_t buf_size)
{
    printf("PANIC fb_umfs_readlink\n");
    exit(1);
	//return (readlink(path, buf, buf_size));
}

/*
 * Does fsync of a file. Returns with fsync return info.
 */
static int
fb_umfs_fsync(fb_fdesc_t *fd)
{
    printf("PANIC fb_umfs_fsync\n");
    exit(1);
	//return (fsync(fd->fd_num));
}

/*
 * Do a posix rename of a file. Return what rename() returns.
 */
static int
fb_umfs_rename(const char *old, const char *new)
{
    printf("PANIC fb_umfs_rename\n");
    exit(1);
	//return (rename(old, new));
}

/*
 * Use mkdir to create a directory.
 */
static int
fb_umfs_mkdir(char *path, int perm)
{
    //printf("PANIC fb_umfs_mkdir\n");
    //exit(1);
	return (mkdir(path, perm));
}

/*
 * Use rmdir to delete a directory. Returns what rmdir() returns.
 */
static int
fb_umfs_rmdir(char *path)
{
    printf("PANIC fb_umfs_rmdir\n");
    exit(1);
	//return (rmdir(path));
}

/*
 * does a recursive rm to remove an entire directory tree (i.e. a fileset).
 * Supplied with the path to the root of the tree.
 */
static void
fb_umfs_recur_rm(char *path)
{
    //printf("Calling recursive rm on %s\n", path);
    //printf("PANIC fb_umfs_recur_rm\n");
    //exit(1);
    
	char cmd[MAXPATHLEN];
	(void) snprintf(cmd, sizeof (cmd), "rm -rf %s", path);
	/* We ignore system()'s return value */
	if (system(cmd));
	return;
}

/*
 * Does a posix opendir(), Returns a directory handle on success,
 * NULL on failure.
 */
static DIR *
fb_umfs_opendir(char *path)
{
    printf("PANIC fb_umfs_opendir\n");
    exit(1);
	//return (opendir(path));
}

/*
 * Does a readdir() call. Returns a pointer to a table of directory
 * information on success, NULL on failure.
 */
static struct dirent *
fb_umfs_readdir(DIR *dirp)
{
    printf("PANIC fb_umfs_readdir\n");
    exit(1);
	//return (readdir(dirp));
}

/*
 * Does a closedir() call.
 */
static int
fb_umfs_closedir(DIR *dirp)
{
    printf("PANIC fb_umfs_closedir\n");
    exit(1);
	//return (closedir(dirp));
}

/*
 * Do a pwrite64 to a file.
 */
static int
fb_umfs_pwrite(fb_fdesc_t *fd, caddr_t iobuf, fbint_t iosize, off64_t offset)
{
    int i = 0;
    for(; i < 2000 ; i++)
        printf("PANIC fb_umfs_pwrite\n");
    exit(1);
	//return (pwrite64(fd->fd_num, iobuf, iosize, offset));
}

/*
 * Does a truncate operation and returns the result
 */
static int
fb_umfs_truncate(fb_fdesc_t *fd, off64_t fse_size)
{
    printf("PANIC fb_umfs_truncate\n");
    exit(1);
/*
#ifdef HAVE_FTRUNCATE64
	return (ftruncate64(fd->fd_num, fse_size));
#else
	filebench_log(LOG_ERROR, "Converting off64_t to off_t in ftruncate,"
			" might be a possible problem");
	return (ftruncate(fd->fd_num, (off_t)fse_size));
#endif
*/
}

/*
 * Does a link operation and returns the result
 */
static int
fb_umfs_link(const char *existing, const char *new)
{
    printf("PANIC fb_umfs_link\n");
    exit(1);
	//return (link(existing, new));
}

/*
 * Does a symlink operation and returns the result
 */
static int
fb_umfs_symlink(const char *existing, const char *new)
{
    printf("PANIC fb_umfs_symlink\n");
    exit(1);
	//return (symlink(existing, new));
}

/*
 * Does an access() check on a file.
 */
static int
fb_umfs_access(const char *path, int amode)
{
    printf("SOMEONE CALLED access, THIS WILL TRIGGER THE MARSHALLING OF \
            THE FILES' STATE%s", "\n");
	//return (access(path, amode));
	umfs_marshall();
    return 0;
}
