Assise - private
==================================

Assise has been tested on both Fedora 27 & Ubuntu 16.04 with Linux kernel 4.8.12+ and gcc
version 5.4.0.

This repository contains the source code for Assise in addition to various benchmarks/applications
that are configured to use our filesystem.

### Building Assise ###
Assume current directory is a project root directory.

##### 0. Install dependencies

```
sudo apt install -y \
libnuma-dev \
uuid-dev \
libaio1 \
libaio-dev \
libndctl-dev \
libdaxctl-dev \
cmake \
gcc \
libudev-dev \
libnl-3-dev \
libnl-route-3-dev \
ninja-build \
pkg-config \
valgrind \
pkg-config \
libcapstone-dev \
numactl \
libpmem-dev \
build-essential
```

If you happened to manually build `ndctl`, then you would need to install the followings:
```
$ sudo apt install -y \
asciidoctor \
libkmod-dev \
libjson-c-dev \
libkeyutils-dev \
uuid-dev \
libudev-dev
```

After building `ndctl`, add the path.
```
export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/lib/pkgconfig
```

##### 1. Change storage configuration
~~~
./utils/change_dev_size.py [dax0.0] [SSD] [HDD] [dax0.1]
~~~
This script does the following:

1. Opens `libfs/src/storage/storage.h`
2. Modifies`dev_size` array values with each storage size in bytes.
    - `dev_size[0]`: could be always 0 (not used)
    - `dev_size[1]`: NVM shared area size (dax0.0)
    - `dev_size[2]`: SSD shared area size : just put 0 for now
    - `dev_size[3]`: HDD shared area size : put 0 for now
    - `dev_size[4]`: NVM log size (dax0.1) : all write logs are assigned from this device
    
##### 2. Configure KernFS cluster

To appropriately configure the cluster, consult the following files:

* (a) libfs/src/global/global.h
* (b) libfs/src/distributed/rpc_interface.h

For (a) set ‘g_n_hot_rep’ to the number of nodes you’re planning to use. By default, this value is 2.
If set to 1, Assise simply runs as a local filesystem.

For (b), configure the IP address of all the ‘g_n_hot_rep’ nodes in the hot_replicas array. Example for two-node cluster:

```c
static struct peer_id hot_replicas[g_n_hot_rep] = {                                                         
 { .ip = "172.17.15.2", .role = HOT_REPLICA, .type = KERNFS_PEER},                                   
 { .ip = "172.17.15.4", .role = HOT_REPLICA, .type = KERNFS_PEER},
};
```
~~~
TODO: Define the various roles of replicas.
~~~

##### 3. Build glibc

Building glibc might not be an easy task in some machines. We provide pre-built libc binaries in "shim/glibc-build".
If you keep failing to build glibc, I recommand to use the pre-built glibc for your testing.

~~~
cd shim
make
~~~
##### 4. Build dependent libraries (RDMA-CORE, SPDK, NVML, JEMALLOC)
~~~
cd libfs/lib
git clone https://github.com/pmem/nvml
make

tar xvjf jemalloc-4.5.0.tar.bz2
cd jemalloc-4.5.0
./autogen
./configure
make
~~~

For SPDK build errors, please consult the SPDK webpage (http://www.spdk.io/doc/getting_started.html)

For NVML build errors, please consult the NVML repository (https://github.com/pmem/nvml/)
##### 5. Build Libfs
~~~
cd libfs
make
~~~
##### 6. Build KernelFS
~~~
cd kernfs
make
cd tests
make
~~~
##### 7. Build libshim
~~~
cd shim/libshim
make
~~~

### Running Assise ###

##### 1. Create DEV-DAX devices\*

~~~
cd utils
sudo ./use_dax.sh bind
~~~
This instruction will create nvm namespaces running in use dev-dax mode.

To destroy the created namespaces, run:
~~~
sudo ./use_dax.sh unbind
~~~

\* This step assumes that your machines is equipped with NVDIMMs. For NVM emulation, refer to this [article](https://pmem.io/2016/02/22/pm-emulation.html).

##### 2. Setup UIO for SPDK
~~~
cd utils
sudo ./uio_setup.sh linux config
~~~
To rollback to previous setting,
~~~
sudo ./uio_setup.sh linux reset
~~~

##### 3. Formatting storages
~~~
cd libfs
sudo ./bin/mkfs.mlfs <dev_id>
~~~
<dev_id> is a hardcoded device identifier used by Assise. The ids are defined as follows:

* dev_id 1 = NVM shared area
* dev_id 2 = SSD shared area
* dev_id 3 = HDD shared area
* dev_id 4 = Write log(s)

Before formatting your devices, make sure their storage sizes are appropriately configured (refer to [Building Assise](#markdown-header-building-assise)).

##### 4. Run KernFS

For each node in the cluster, do:
~~~
cd kernfs/tests
make
sudo ./run.sh kernfs
~~~

Make sure to run KernFS on the nodes in the reverse order of 'libfs/src/distributed/rpc_interface.h' to avoid connection
establishment issues.

##### 5. Run testing problem
~~~
cd libfs/tests
make
sudo ./run.sh iotest sw 2G 4K 1 #sequential write, 2GB file with 4K IO and 1 thread
~~~

### Assise flags ###
~~~
TODO: define the configuration flags in the LibFS/KernFS makefiles.
~~~