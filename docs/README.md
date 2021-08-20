# LineFS repository for the Artifact Evaluation of SOSP 2021 <!-- omit in toc -->

- [1. System requirements (Tested environment)](#1-system-requirements-tested-environment)
  - [1.1. Hardware requirements](#11-hardware-requirements)
    - [1.1.1. Host machine](#111-host-machine)
    - [1.1.2. SmartNIC](#112-smartnic)
  - [1.2. Software requirements](#12-software-requirements)
    - [1.2.1. Host machine](#121-host-machine)
    - [1.2.2. SmartNIC](#122-smartnic)
- [2. Dependent package installation](#2-dependent-package-installation)
- [3. Hardware setup](#3-hardware-setup)
  - [3.1. RoCE configuration](#31-roce-configuration)
  - [3.2. SmartNIC setup](#32-smartnic-setup)
  - [3.3. Persistent memory configuration](#33-persistent-memory-configuration)
- [4. Configuring LineFS](#4-configuring-linefs)
  - [4.1. Compile-time configurations](#41-compile-time-configurations)
  - [4.2. Run-time configurations](#42-run-time-configurations)
- [5. Compiling LineFS](#5-compiling-linefs)
  - [5.1. Build on the host machine](#51-build-on-the-host-machine)
  - [5.2. Build on SmartNIC](#52-build-on-smartnic)
- [6. Formatting devices](#6-formatting-devices)
- [7. Deploying LineFS](#7-deploying-linefs)
  - [7.1. Deployment scenario](#71-deployment-scenario)
  - [7.2. Run kernel workers on host machines](#72-run-kernel-workers-on-host-machines)
  - [7.3. Run NICFS on SmartNIC](#73-run-nicfs-on-smartnic)
  - [7.4. Run applications](#74-run-applications)
- [8. Run Assise](#8-run-assise)
- [9. TODO](#9-todo)

If you are using our testbed for SOSP 2021 Artifact Evaluation, please read [README-AE.md](README-AE.md) first. After reading `README-AE.md`, you can directly go on to [Configuring LineFS](#configuring-linefs).

## 1. System requirements (Tested environment)

### 1.1. Hardware requirements

#### 1.1.1. Host machine

- 16 cores per NUMA node
- 96 GB DRAM
- 6 NVDIMM persistent memory per NUMA node

#### 1.1.2. SmartNIC

- NVIDIA BlueField DPU 25G (Model number: MBF1M332A-ASCAT)
  - 16 ARM cores
  - 16 GB DRAM

### 1.2. Software requirements

#### 1.2.1. Host machine

- Ubuntu 20.04
- Linux kernel version: 5.4.128
- Mellanox OFED driver version: 4.7-3.2.9
- Bluefield Software version: 2.5.1

#### 1.2.2. SmartNIC

- Ubuntu 18.04
- Linux kernel version: 4.18.0
- Mellanox OFED driver: 4.7-3.2.9

## 2. Dependent package installation

```shell
sudo apt install build-essential make pkg-config autoconf libnuma-dev libaio1 libaio-dev uuid-dev librdmacm-dev ndctl numactl libncurses-dev libssl-dev libelf-dev rsync
```


## 3. Hardware setup

### 3.1. RoCE configuration

You need to configure RoCE to enable RDMA on Ethernet. This document does not describe how to deploy RoCE because configuration processes differ according to a switch and adapters in the system. Please refer to [Recommended Network Configuration Examples for RoCE Deployment] written by NVIDIA for RoCE setup.

### 3.2. SmartNIC setup

We assume that Ubuntu and MLNX_OFED driver are installed on SmartNIC. SmartNIC should be accessible via ssh.
To set up SmartNIC, refer to [BlueField DPU Software documentation](https://docs.mellanox.com/category/bluefieldsw).

### 3.3. Persistent memory configuration

> If your system does not have persistent memory, you need to emulate it using DRAM. Refer to [How to Emulate Persistent Memory Using Dynamic Random-access Memory (DRAM)](https://software.intel.com/content/www/us/en/develop/articles/how-to-emulate-persistent-memory-on-an-intel-architecture-server.html) for persistent memory emulation.

LineFS uses persistent memory as storage and it needs to be configured as Device-DAX mode. Make sure that the created namespace has enough size. It must be larger than the size reserved by LineFS (TODO: Add link). A command for creating a new namespace is as below.

```shell
sudo ndctl create-namespace -m dax --region=region0 --size=132G
```

Now, you can find out DAX devices under `/dev` directory as below.

```shell
$ ls /dev/dax*
/dev/dax0.0  /dev/dax0.1
```


## 4. Configuring LineFS

### 4.1. Compile-time configurations

`kernfs/Makefile` and `libfs/Makefile` includes compile-time configurations. You need to re-compile LineFS by running as below.

```shell
# At the project root directory.

# If you modified 'kernfs/Makefile', run:
make kernfs

# If you modified 'libfs/Makefile', run:
make libfs
```

Some constants like the private log size, the number of max LibFS processes are defined in `libfs/src/global/global.h`.

IP addresses of machines and SmartNICs and the order of replication chain are defined as a variable `hot_replicas` in `libfs/src/distributed/rpc_interface.h`.

A device size to be used by LineFS is defined as variable `dev_size` in `libfs/src/storage/storage.h`.

### 4.2. Run-time configurations

`mlfs_config.sh` includes run-time configurations. To apply a change in configurations you need to restart LineFS.

## 5. Compiling LineFS

### 5.1. Build on the host machine

The following command will do all the compilations required on the host machine. It includes downloading and compiling libraries, compiling *LibFS*, *kernel worker*, an RDMA module and benchmarks, setting SPDK up, and formatting file system.

```shell
make host-init
```

You can build the components one by one with the following commands. Refer to `Makefile` in the project root directory for detail.

```shell
make host-lib   # Build host libraries.
make rdma       # Build rdma module.
make kernfs     # Build kernel worker. TODO: change to another name.
make libfs      # Build LibFS.
```

### 5.2. Build on SmartNIC

The following command will do all the compilations required on SmartNIC. It includes downloading and compiling libraries and compiling an RDMA module and `NICFS`.

```shell
make snic-init
```

You can build the components one by one with the following commands. Refer to `Makefile` in the project root directory for detail.

```shell
make snic-lib   # Build libraries.
make rdma       # Build rdma module.
make kernfs     # Build `NICFS` TODO: change to another name.
```

## 6. Formatting devices

Run the following command at the project root directory.

```shell
make mkfs
```

## 7. Deploying LineFS

### 7.1. Deployment scenario

Let's think of the following deployment scenario.
There are three host machines and each host machine is equipped with a SmartNIC.

|| Hostname |  IP address
|:---:|:---:|:---: 
|*Host machine 1* | `host01` | 192.168.13.111
|*Host machine 2* | `host02` | 192.168.13.113
|*Host machine 3* | `host03` | 192.168.13.115
|SmartNIC of *Host machine 1* | `host01-nic` | 192.168.13.112
|SmartNIC of *Host machine 2* | `host02-nic` | 192.168.13.114
|SmartNIC of *Host machine 3* | `host03-nic` | 192.168.13.116

We want to make LineFS have a replication chain as below.

- *Host machine 1* --> *Host machine 2* --> *Host machine 3*

### 7.2. Run kernel workers on host machines

The following script runs *kernel worker*.

```shell
scripts/run_kernfs.sh
```

We need to execute this script on all three host machines. After running the script, *kernel workers* wait for SmartNICs to connect.

TODO: Add screenshot. "Waiting for client accesses."

### 7.3. Run NICFS on SmartNIC

We need to execute this script on all three SmartNICs. Run them in the reverse order in which they are defined as `hot_replicas` at `libfs/src/distributed/rpc_interface.h`. For example, if they are defined as below,
```c
static struct peer_id hot_replicas[g_n_hot_rep] = {
    { .ip = "192.168.13.114", .role = HOT_REPLICA, .type = KERNFS_NIC_PEER},  // SmartNIC on host machine 1
    { .ip = "192.168.13.113", .role = HOT_REPLICA, .type = KERNFS_PEER},      // Host machine 1
    { .ip = "192.168.13.118", .role = HOT_REPLICA, .type = KERNFS_NIC_PEER},  // SmartNIC on host machine 2
    { .ip = "192.168.13.117", .role = HOT_REPLICA, .type = KERNFS_PEER},      // Host machine 2
    { .ip = "192.168.13.116", .role = HOT_REPLICA, .type = KERNFS_NIC_PEER},  // SmartNIC on host machine 2
    { .ip = "192.168.13.115", .role = HOT_REPLICA, .type = KERNFS_PEER}       // Host machine 3
}
```
then run `mkfs_run_kernfs.sh` in `host03-nic` --> `host02-nic` --> `host01-nic` order. You have to wait that the previous SmartNIC finishes establishing its connections.

TODO: Add screenshot. "Ready for receiving LibFS accesses."

### 7.4. Run applications

We are going to run a simple test application, `iotest`.
```
cd libfs/tests
sudo ./run.sh iotest sw 1G 4K 1   # sequential write, 1GB file, 4KB i/o size, 1 thread
```

## 8. Run Assise

## 9. TODO

- Compare with Assise
- Compare with Hyperloop
- Add what's missing in this artifact compared with paper.