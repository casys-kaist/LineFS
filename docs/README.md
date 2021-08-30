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
- [5. Configuring LineFS](#5-configuring-linefs)
  - [5.1. Set project root paths and hostnames](#51-set-project-root-paths-and-hostnames)
  - [5.2. Compile-time configurations](#52-compile-time-configurations)
  - [5.3. Run-time configurations](#53-run-time-configurations)
- [6. Compiling LineFS](#6-compiling-linefs)
  - [6.1. Build on the host machine](#61-build-on-the-host-machine)
  - [6.2. Build on SmartNIC](#62-build-on-smartnic)
- [7. Formatting devices](#7-formatting-devices)
- [8. Deploying LineFS](#8-deploying-linefs)
  - [8.1. Deployment scenario](#81-deployment-scenario)
  - [8.2. Setup I/OAT device](#82-setup-ioat-device)
  - [8.3. Run kernel workers on host machines](#83-run-kernel-workers-on-host-machines)
  - [8.4. Run NICFS on SmartNIC](#84-run-nicfs-on-smartnic)
    - [8.4.1. Make NICFSes sleep for different durations](#841-make-nicfses-sleep-for-different-durations)
  - [8.5. Run applications](#85-run-applications)
- [9. Run Assise](#9-run-assise)
- [10. Running benchmarks](#10-running-benchmarks)

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

## 5. Configuring LineFS

### 5.1. Set project root paths and hostnames

Set project root paths of the host machine and the SmartNIC. For example, if your source codes are located in `/home/guest/LineFS_x86` on host and `/home/geust/LineFS_ARM` on SmartNIC, set as below.

```shell
PROJ_DIR="/home/guest/LineFS_x86"
NIC_PROJ_DIR="/home/guest/LineFS_ARM"
```

Set hostnames for three host machines and three SmartNICs. The name of SmartNICs should be the name of RDMA interfaces. For example, If you are using three host machines, `host01`, `host02`, and `host03`, and three SmartNICs, `host01-nic`, `host02-nic`, and `host03-nic`, change `scripts/global.sh` as below.

```shell
HOST_1="host01"
HOST_2="host02"
HOST_3="host03"

NIC_1="host01-nic-rdma"
NIC_2="host02-nic-rdma"
NIC_3="host03-nic-rdma"
```

Or you can use IP addresses instead of hostnames.

### 5.2. Compile-time configurations

Locations of compile-time configurations are as below.

- `kernfs/Makefile` and `libfs/Makefile` includes compile-time configurations. You have to re-compile LineFS to apply changes in the configurations. You can leave it as a default.
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

### 5.3. Run-time configurations

`mlfs_config.sh` includes run-time configurations. To apply changes in the configurations you need to restart LineFS.

## 6. Compiling LineFS

### 6.1. Build on the host machine

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

### 6.2. Build on SmartNIC

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

## 7. Formatting devices

Run the following command at the project root directory. Run it only on the host machines. (There is no device on the SmartNIC.)

```shell
make mkfs
```

## 8. Deploying LineFS

### 8.1. Deployment scenario

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

### 8.2. Setup I/OAT device

The command should be executed on each host machine.

```shell
make spdk-init
```

You only need to run it once after reboot.

### 8.3. Run kernel workers on host machines

The following script runs *kernel worker*.

```shell
scripts/run_kernfs.sh
```

You have to execute this script on all three host machines. After running the script, *kernel workers* wait for SmartNICs to connect.

### 8.4. Run NICFS on SmartNIC

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

#### 8.4.1. Make NICFSes sleep for different durations

It is convenient to make the script wait for a while before running NICFS. Two files in the `../local_scripts` directory (outside of the project directory) make scripts sleep for a while. Set proper sleep time into those files and make sure that a soft link to the `../local_scripts` directory exists at the project root directory.

1. `../local_scripts/sleep.sh`  
  `scripts/run_kernfs.sh` and `scripts/mkfs_run_kernfs.sh` run this script before running NICFS.

2. `../local_scripts/sleep_mkfs.sh`  
   `scripts/mkfs_run_kernfs.sh` runs this script before running NICFS.

Intended usage:

- Execute the same scripts on all the host machines and NICs at the same time.
- Use `run_kernfs.sh` if you dont' format host devices. Use `mkfs_run_kernfs.sh` if you format host device.

Instead of `run_kernfs.sh`, you can run NICFS with `mkfs_run_kernfs.sh`. Although it doesn't format device - there is no device on SmartNIC -, it additionally sleeps for a duration designated by `../local_scripts/sleep_mkfs.sh`.

> ***It is required to set `../local_scripts/sleep_mkfs.sh` up correctly because it is used by the other benchmark scripts.***

Here is what you need to do.  
Create `sleep.sh` and `sleep_mkfs.sh` files:

```shell
# At project root directory.

mkdir ../local_scripts
cat << EOT >> ../local_scripts/sleep.sh
#!/bin/bash
sleep 10
EOT
chmod +x ../local_scripts/sleep.sh

cat << EOT >> ../local_scripts/sleep_mkfs.sh
#!/bin/bash
sleep 32
EOT
chmod +x ../local_scripts/sleep_mkfs.sh
```

Adjust sleep time properly. For example, if both log area and public area are 19 GB then `../local_scripts/sleep.sh` files in each machine would be:

```shell
# Primary NIC
#!/bin/bash
sleep 20
```

```shell
# Replica 1 NIC
#!/bin/bash
sleep 10
```

```shell
# Replica 2 NIC
#!/bin/bash
# No sleep.
```

and `../local_scripts/sleep_mkfs.sh`  files would be:

```shell
# Primary NIC
#!/bin/bash
sleep 32 # 19 GB
```

```shell
# Replica 1 NIC
#!/bin/bash
sleep 32 # 19 GB
```

```shell
# Replica 2 NIC
#!/bin/bash
sleep 32 # 19 GB
```

If you increased a device size (`dev_size`) as mentioned at [Compile-time configurations](#52-compile-time-configurations), you have to increase sleep time of `../local_scripts/sleep_mkfs.sh` also.

### 8.5. Run applications

We are going to run a simple test application, `iotest`. Note that, all the LineFS applications run on the Primary host CPU (`host01`).

```shell
cd libfs/tests
sudo ./run.sh ./iotest sw 1G 4K 1   # sequential write, 1GB file, 4KB i/o size, 1 thread
```

## 9. Run Assise

To run Assise (DFS without NIC-offloading) rather than LineFS, you have to rebuild LibFS and SharedFS (KernFS).

```shell
# At the project root directory,
make kernfs-assise
make libfs-assise
```

You can use the same script, `scripts/run_kernfs.sh`, however, a SharedFS (KernFS) needs to wait for the next SharedFS in the replication chain to be ready.
For example, run Replica 2's SharedFS -> wait for a while -> run Replica 1's SharedFS -> wait for a while --> run Primary's SharedFS.

Note that, you have to set proper sleep time for each host as described at [Make NICFSes sleep for different durations](#841-make-nicfses-sleep-for-different-durations)

An example of `../local_scripts/sleep.sh` files in each host machine:

```shell
# Primary.
#!/bin/bash
sleep 8
```

```shell
# Replica 1
#!/bin/bash
sleep 4
```

```shell
# Replica 2
#!/bin/bash
# No sleep.
```

## 10. Running benchmarks

Refer to [README-bench](README-bench.md).
