# NOVA: NOn-Volatile memory Accelerated log-structured file system

## Introduction
NOVA is a log-structured file system designed for byte-addressable non-volatile memories, developed by
the [Non-Volatile Systems Laboratory][NVSL], University of California, San Diego.

NOVA extends ideas of LFS to leverage NVMM, yielding a simpler, high-performance file system that supports fast and efficient garbage collection and quick recovery from system failures.
NOVA has passed the [Linux POSIX test suite][POSIXtest], and existing applications need not be modified to run on NOVA. NOVA bypasses the block layer and OS page cache, writes to NVM directly and reduces the software overhead.

NOVA provides strong data consistency guanrantees:

* Atomic metadata update: each directory operation is atomic.
* Atomic data update; for each `write` operation, the file data and the inode are updated in a transactional way.
* DAX-mmap: NOVA supports DAX-mmap which maps NVMM pages directly to the user space.

With atomicity guarantees, NOVA is able to recover from system failures and restore to a consistent state.

For more details about the design and implementation of NOVA, please see this paper:

**NOVA: A Log-structured File system for Hybrid Volatile/Non-volatile Main Memories**<br>
[PDF](http://cseweb.ucsd.edu/~swanson/papers/FAST2016NOVA.pdf)<br>
*Jian Xu and Steven Swanson, University of California, San Diego*<br>
Published in FAST 2016

## Building NOVA
NOVA works on the 4.3 version of x86-64 Linux kernel.

To build NOVA, simply run a

~~~
#make
~~~

command.

## Running NOVA
NOVA runs on a physically contiguous memory region that is not used by the Linux kernel, and relies on the kernel NVDIMM support.

To run NOVA, first build up your kernel with NVDIMM support enabled (`CONFIG_BLK_DEV_PMEM`), and then you can
reserve the memory space by booting the kernel with `memmap` command line option.

For instance, adding `memmap=16G!8G` to the kernel boot parameters will reserve 16GB memory starting from 8GB address, and the kernel will create a `pmem0` block device under the `/dev` directory.

After the OS has booted, you can initialize a NOVA instance with the following commands:


~~~
#insmod nova.ko
#mount -t NOVA -o init /dev/pmem0 /mnt/ramdisk 
~~~

The above commands create a NOVA instance on pmem0 device, and mount on `/mnt/ramdisk`.

To recover an existing NOVA instance, mount NOVA without the init option, for example:

~~~
#mount -t NOVA /dev/pmem0 /mnt/ramdisk 
~~~

There are two scripts provided in the source code, `setup-nova.sh` and `remount-nova.sh` to help setup NOVA.

## Current limitations

* NOVA only works on x86-64 kernels.
* NOVA does not currently support extended attributes or ACL.
* NOVA requires the underlying block device to support DAX (Direct Access) feature.
* Applications can write to a file, or mmap a file and load/store the file directly, but not at the same time, i.e. writing to a mmaped file is disallowed, because write is copy-on-write (out-of-place) while mmap is DAX (in-place).

[NVSL]: http://nvsl.ucsd.edu/ "http://nvsl.ucsd.edu"
[POSIXtest]: http://www.tuxera.com/community/posix-test-suite/ 
