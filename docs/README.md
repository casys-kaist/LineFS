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
- [4. Download source code](#4-download-source-code)
- [5. NFS mount for source code management and deployment](#5-nfs-mount-for-source-code-management-and-deployment)
  - [5.1. NFS server setup](#51-nfs-server-setup)
  - [5.2. NFS client setup](#52-nfs-client-setup)
- [6. Configuring LineFS](#6-configuring-linefs)
  - [6.1. Set project root paths, hostnames, and network interfaces](#61-set-project-root-paths-hostnames-and-network-interfaces)
  - [6.2. Make root and a user can ssh to the x86 hosts and NICs without entering a password](#62-make-root-and-a-user-can-ssh-to-the-x86-hosts-and-nics-without-entering-a-password)
  - [6.3. Compile-time configurations](#63-compile-time-configurations)
  - [6.4. Run-time configurations](#64-run-time-configurations)
- [7. Compiling LineFS](#7-compiling-linefs)
  - [7.1. Build on the host machine](#71-build-on-the-host-machine)
  - [7.2. Build on SmartNIC](#72-build-on-smartnic)
- [8. Formatting devices](#8-formatting-devices)
- [9. Deploying LineFS](#9-deploying-linefs)
  - [9.1. Deployment scenario](#91-deployment-scenario)
  - [9.2. Setup I/OAT device](#92-setup-ioat-device)
  - [9.3. Run kernel workers on host machines](#93-run-kernel-workers-on-host-machines)
  - [9.4. Run NICFS on SmartNIC](#94-run-nicfs-on-smartnic)
    - [9.4.1. Running all Kernel workers and NICFSes at once](#941-running-all-kernel-workers-and-nicfses-at-once)
  - [9.5. Run applications](#95-run-applications)
- [10. Run Assise](#10-run-assise)
- [11. Running benchmarks](#11-running-benchmarks)

> If you are using our testbed for SOSP 2021 Artifact Evaluation, please read [README for Artifact Evaluation Reviewers]() first. It is provided on the HotCRP submission page.  
***After reading it, you can directly go on to [Configuring LineFS](#5-configuring-linefs).***

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

- Ubuntu 18.04
- Linux kernel version: 5.3.11
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

LineFS uses persistent memory as storage and it needs to be configured as Device-DAX mode. Make sure that the created namespace has enough size. It must be larger than the size reserved by LineFS (`dev_size` in `libfs/src/storage/storage.h`). A command for creating a new namespace is as below.

```shell
sudo ndctl create-namespace -m dax --region=region0 --size=132G
```

Now, you can find out DAX devices under `/dev` directory as below.

```shell
$ ls /dev/dax*
/dev/dax0.0  /dev/dax0.1
```

## 4. Download source code

```shell
git clone git@github.com:casys-kaist/LineFS.git
cd LineFS
```

## 5. NFS mount for source code management and deployment

With NFS, you can easily share the same source code among three machines and three SmartNICs.
It is recommended to maintain two different directories for the same source code, one is for x86 hosts and the other is for ARM SoCs in SmartNIC.

### 5.1. NFS server setup

Locate source codes in one of the x86 hosts. Let assume source codes for x86 are stored at ${HOME}/LineFS_x86 and for ARM are at ${HOME}/LineFS_ARM.

```shell
cd ~
git clone https://github.com/casys-kaist/LineFS.git LineFS_x86
cp -r LineFS_x86 LineFS_ARM
```

***You can skip the following process if the NFS server is already set up.***

Install NFS server on libra06 machine.

```shell
sudo apt update
sudo apt install nfs-kernel-server
```

Add two source code directory paths to `/etc/exports` file. For example, if the paths are `/home/guest/LineFS_x86` and `/home/guest/LineFS_ARM`, add following lines to the file.

```
/home/guest/LineFS_x86 *(rw,no_root_squash,sync,insecure,no_subtree_check,no_wdelay)
/home/guest/LineFS_ARM *(rw,no_root_squash,sync,insecure,no_subtree_check,no_wdelay)
```

To apply the modification, run the following command.
```shell
sudo exportfs -rv
```

### 5.2. NFS client setup

***You can skip this process if the NFS clients are set up correctly.***

In the x86 hosts and NICs, install NFS client packages.

```shell
sudo apt update
sudo apt install nfs-common
```

On the x86 hosts, mount the x86 source code directory.

```shell
cd ~
mkdir LineFS_x86 	# If there is no directory, create one.
sudo mount -t nfs <nfs_server_address>:/home/guest/LineFS_x86 /home/guest/LineFS_x86
```

Now, you should see the source code at `${HOME}/LineFS_x86` directory.

Similarly, mount LineFS_ARM source codes to SmartNICs.
Access to SmartNIC from host machines. In the NIC, mount the ARM source code directory. You need to set up all three NICs.
```shell
mkdir LineFS_ARM     # If there is no directory, create one.
sudo mount -t nfs <nfs_server_address>:/home/guest/LineFS_ARM /home/guest/LineFS_ARM
```

## 6. Configuring LineFS

### 6.1. Set project root paths, hostnames, and network interfaces

Set project root paths of the host machine and the SmartNIC at `scripts/global.sh`. For example, if your source codes are located in `/home/guest/LineFS_x86` on host and `/home/geust/LineFS_ARM` on SmartNIC, set as below.

```shell
PROJ_DIR="/home/guest/LineFS_x86"
NIC_PROJ_DIR="/home/guest/LineFS_ARM"
```

Set host's path of directory that contains source codes for ARM SoC.

```shell
NIC_SRC_DIR="/home/guest/LineFS_ARM"
```

Set hostnames for three x86 hosts and three SmartNICs. For example, If you are using three host machines, `host01`, `host02`, and `host03`, and three SmartNICs, `host01-nic`, `host02-nic`, and `host03-nic`, change `scripts/global.sh` as below.

```shell
# Hostnames of X86 hosts.
# You can get this values by running `hostname` command on each X86 host.
HOST_1="host01"
HOST_2="host02"
HOST_3="host03"

# Hostnames of NICs
# You can get this values by running `hostname` command on each NIC.
NIC_1="host01-nic"
NIC_2="host02-nic"
NIC_3="host03-nic"
```

Set the names of network interfaces of x86 hosts and SmartNICs. You can use IP addresses instead of the names. The name of SmartNICs should be the name of RDMA interfaces. If hosts' IP addresses are mapped to `host01`, `host02`, and `host03`, and NICs' are mapped to `host01-nic-rdma`, `host02-nic-rdma`, and `host03-nic-rdma` in `/etc/hosts`, set `scripts/global.sh` as below.

```shell
# Hostname (or IP address) of host machines. You should be able to ssh to each machine with these names.
HOST_1_INF="host01"
HOST_2_INF="host02"
HOST_3_INF="host03"

# Name (or IP address) of RDMA interface of NICs. You should be able to ssh to each NIC with these names.
NIC_1_INF="host01-nic-rdma"
NIC_2_INF="host02-nic-rdma"
NIC_3_INF="host03-nic-rdma"
```

### 6.2. Make root and a user can ssh to the x86 hosts and NICs without entering a password

To use scripts provided in this source code, you must be able to access all the machines and NICs via ssh without entering a password as a `root`.
In other words, you need to copy the `root` account's public key to all the machines and NICs. It is optional but recommended to copy the public key of your account too if you don't want to execute all the scripts as a `root`. `ssh-copy-id` is a useful command to do it. Note that, if you don't have a key, generate one with `ssh-keygen`.

### 6.3. Compile-time configurations

Locations of compile-time configurations are as below.

- `kernfs/Makefile` and `libfs/Makefile` include compile-time configurations. You have to re-compile LineFS to apply changes in the configurations. You can leave it as a default.
- Some constants like the private log size, the number of max LibFS processes are defined in `libfs/src/global/global.h`. You can leave it as a default.
- IP addresses of machines and SmartNICs and the order of replication chain are defined as a variable `hot_replicas` in `libfs/src/distributed/rpc_interface.h`. You have to set correct values for this configuration.
- A device size to be used by LineFS is defined as a variable `dev_size` in `libfs/src/storage/storage.h`. You can leave it as a default.
- Paths of pmem devices should be defined in `libfs/src/storage/storage.c` as below. `g_dev_path[1]` is the path of the device for the public area and `g_dev_path[4]` is for the log area. You have to set correct values for this configuration. Here is an example.

  ```c
  char *g_dev_path[] = {
  (char *)"unused",
  (char *)"/dev/dax0.0", # Public Area
  (char *)"/backup/mlfs_ssd",
  (char *)"/backup/mlfs_hdd",
  (char *)"/dev/dax0.1", # Log Area
  };
  ```

### 6.4. Run-time configurations

`mlfs_config.sh` includes run-time configurations. To apply changes in the configurations you need to restart LineFS.

## 7. Compiling LineFS

### 7.1. Build on the host machine

The following command will do all the compilations required on the host machine. It includes downloading and compiling libraries, compiling *LibFS* library, a *kernel worker*, an RDMA module and benchmarks, setting SPDK up, and formatting file system.

```shell
make host-init
```

You can build the components one by one with the following commands. Refer to `Makefile` in the project root directory for detail.

```shell
make host-lib   # Build host libraries.
make rdma       # Build rdma module.
make kernfs-linefs     # Build kernel worker.
make libfs-linefs      # Build LibFS.
```

### 7.2. Build on SmartNIC

The following command will do all the compilations required on SmartNIC. It includes downloading and compiling libraries and compiling an RDMA module and `NICFS`.

```shell
make snic-init
```

You can build the components one by one with the following commands. Refer to `Makefile` in the project root directory for detail.

```shell
make snic-lib           # Build libraries.
make rdma               # Build rdma module.
make kernfs-linefs      # Build `NICFS`
```

## 8. Formatting devices

Run the following command at the project root directory. Run it only on the host machines. (There is no device on the SmartNIC.)

```shell
make mkfs
```

## 9. Deploying LineFS

### 9.1. Deployment scenario

Let's think of the following deployment scenario.
There are three host machines and each host machine is equipped with a SmartNIC.

|                              |   Hostname   | RDMA IP address |
| :--------------------------: | :----------: | :-------------: |
|       *Host machine 1*       |   `host01`   | 192.168.13.111  |
|       *Host machine 2*       |   `host02`   | 192.168.13.113  |
|       *Host machine 3*       |   `host03`   | 192.168.13.115  |
| SmartNIC of *Host machine 1* | `host01-nic` | 192.168.13.112  |
| SmartNIC of *Host machine 2* | `host02-nic` | 192.168.13.114  |
| SmartNIC of *Host machine 3* | `host03-nic` | 192.168.13.116  |

We want to make LineFS have a replication chain as below.

- *Host machine 1* --> *Host machine 2* --> *Host machine 3*

### 9.2. Setup I/OAT device

The command should be executed on each host machine.

```shell
make spdk-init
```

You only need to run it once after reboot.

### 9.3. Run kernel workers on host machines

The following script runs *kernel worker*.

```shell
scripts/run_kernfs.sh
```

You have to execute this script on all three host machines. After running the script, *kernel workers* wait for SmartNICs to connect.

### 9.4. Run NICFS on SmartNIC

Execute `scripts/run_kernfs.sh` on all three SmartNICs. Run them in the reverse order of the replication chain. The replication chain is defined as `hot_replicas` at `libfs/src/distributed/rpc_interface.h`. For example, if they are defined as below,

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

run `scripts/run_kernfs.sh` in `host03-nic` --> `host02-nic` --> `host01-nic` order. You have to wait that the previous SmartNIC finishes establishing its connections.

#### 9.4.1. Running all Kernel workers and NICFSes at once

If you could correctly run the Kernel workers and NICFSes manually, you can use a script to run all them with one command.
To make the script work, set signal paths in `mlfs_conf.sh` as below. This script works only if you are sharing the source codes using NFS (Refer to [NFS mount for source coe management and deployment](#5-nfs-mount-for-source-code-management-and-deployment)).

```shell
export X86_SIGNAL_PATH='/home/host01/LineFS_x86/scripts/signals' # Signal path in X86 host. It should be the same as PROJ_DIR in global.sh
export ARM_SIGNAL_PATH='/home/host01/LineFS_ARM/scripts/signals' # Signal path in ARM SoC. It should be the same as NIC_PROJ_DIR in global.sh
```

Run the script. It will format devices of x86 hosts and run kernel workers and NICFSes. Make sure that the source code is built as LineFS (`make kernfs-linefs`).

```shell
# At the project root directory of host01 machine,
scripts/run_all_kernfs.sh
```

### 9.5. Run applications

We are going to run a simple test application, `iotest`. Note that, all the LineFS applications run on the Primary host CPU (`host01`).

```shell
cd libfs/tests
sudo ./run.sh ./iotest sw 1G 4K 1   # sequential write, 1GB file, 4KB i/o size, 1 thread
```

## 10. Run Assise

To run Assise (DFS without NIC-offloading) rather than LineFS, you have to rebuild LibFS and SharedFS (a.k.a. KernFS).

```shell
# At the project root directory,
make kernfs-assise
make libfs-assise
```

You can use the same script, `scripts/run_kernfs.sh`, however, a SharedFS (KernFS) needs to wait for the next SharedFS in the replication chain to be ready.
For example, run Replica 2's SharedFS -> wait for a while -> run Replica 1's SharedFS -> wait for a while --> run Primary's SharedFS.

You can use the same script, `scripts/run_all_kernfs.sh` to format devices and run all the SharedFSes with one command as described in [Running all Kernel workers and NICFSes at once](#941-running-all-kernel-workers-and-nicfses-at-once).

## 11. Running benchmarks

Refer to [README-bench](README-bench.md).
