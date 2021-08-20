# Availability experiment of LineFS (§5.5) <!-- omit in toc -->

- [1. Enable RDMA memcpy on the host failure](#1-enable-rdma-memcpy-on-the-host-failure)
- [2. Enable printing per second real-time throughput of Filebench](#2-enable-printing-per-second-real-time-throughput-of-filebench)
- [3. Make Replica 1's host OS stop for a while](#3-make-replica-1s-host-os-stop-for-a-while)
	- [3.1. Build stop-machine module](#31-build-stop-machine-module)
	- [3.2. Make host OS freeze](#32-make-host-os-freeze)
- [4. Run `Varmail` workload and freeze the host](#4-run-varmail-workload-and-freeze-the-host)
- [5. Check real-time memcopy bandwidth](#5-check-real-time-memcopy-bandwidth)

With this experiment, we show that LineFS is available under the failure of Replica 1's host OS.
The purpose of the experiment is to see that the real-time throughput of Filebench with Varmail workload does not change even if Replica 1's host OS stops for a while.

## 1. Enable RDMA memcpy on the host failure

Set `BACKUP_RDMA_MEMCPY` flag to enable RDMA memcopy when a host does not response. Also, set `PROFILE_REALTIME_MEMCPY_BW` flag to monitor the real-time bandwidth usage by memcopy.


## 2. Enable printing per second real-time throughput of Filebench

Uncomment the following lines (line number: 2576-2577) at `parser_pause()` in `bench/filebench/parser_gram.y`.
```c
2576	stats_snap();
2577	printf("\n\n");
```

Rebuild Filebench.

```shell
make clean; make
```

It will print out IO summary including throughput (ops/s) every second as below.

```shell
...
22.444: Per-Operation Breakdown
closefile4           14024ops     1168ops/s   0.0mb/s    0.185 us/op [0.000 us - 12.000 us]
readfile4            14024ops     1168ops/s   9.3mb/s    5.442 us/op [1.000 us - 28.000 us]
openfile4            14024ops     1168ops/s   0.0mb/s    1.324 us/op [1.000 us - 19.000 us]
closefile3           14024ops     1168ops/s   0.0mb/s    0.300 us/op [0.000 us - 13.000 us]
fsyncfile3           14024ops     1168ops/s   0.0mb/s  212.100 us/op [186.000 us - 4164.000 us]
appendfilerand3      14024ops     1168ops/s   9.2mb/s   10.215 us/op [2.000 us - 27.000 us]
readfile3            14024ops     1168ops/s   9.4mb/s    6.374 us/op [1.000 us - 23.000 us]
openfile3            14024ops     1168ops/s   0.0mb/s    1.488 us/op [1.000 us - 14.000 us]
closefile2           14024ops     1168ops/s   0.0mb/s    0.297 us/op [0.000 us - 12.000 us]
fsyncfile2           14024ops     1168ops/s   0.0mb/s  231.636 us/op [211.000 us - 4030.000 us]
appendfilerand2      14025ops     1169ops/s   9.1mb/s    8.471 us/op [2.000 us - 24.000 us]
createfile2          14025ops     1169ops/s   0.0mb/s    7.669 us/op [5.000 us - 487.000 us]
deletefile1          14025ops     1169ops/s   0.0mb/s    9.724 us/op [5.000 us - 400.000 us]
22.444: IO Summary: 182315 ops 15189.864 ops/s 2337/2337 rd/wr  37.1mb/s   0.1ms/op


23.444: Per-Operation Breakdown
closefile4           16036ops     1233ops/s   0.0mb/s    0.183 us/op [0.000 us - 12.000 us]
readfile4            16036ops     1233ops/s   9.8mb/s    5.429 us/op [1.000 us - 28.000 us]
openfile4            16036ops     1233ops/s   0.0mb/s    1.320 us/op [1.000 us - 19.000 us]
closefile3           16036ops     1233ops/s   0.0mb/s    0.300 us/op [0.000 us - 13.000 us]
fsyncfile3           16036ops     1233ops/s   0.0mb/s  211.906 us/op [186.000 us - 4164.000 us]
appendfilerand3      16036ops     1233ops/s   9.7mb/s   10.203 us/op [2.000 us - 27.000 us]
readfile3            16036ops     1233ops/s   9.9mb/s    6.361 us/op [1.000 us - 23.000 us]
openfile3            16036ops     1233ops/s   0.0mb/s    1.485 us/op [1.000 us - 14.000 us]
closefile2           16036ops     1233ops/s   0.0mb/s    0.294 us/op [1.000 us - 12.000 us]
fsyncfile2           16036ops     1233ops/s   0.0mb/s  231.647 us/op [211.000 us - 4030.000 us]
appendfilerand2      16037ops     1233ops/s   9.6mb/s    8.465 us/op [2.000 us - 24.000 us]
createfile2          16037ops     1233ops/s   0.0mb/s    7.657 us/op [5.000 us - 487.000 us]
deletefile1          16037ops     1233ops/s   0.0mb/s    9.746 us/op [5.000 us - 400.000 us]
23.445: IO Summary: 208471 ops 16033.014 ops/s 2467/2467 rd/wr  39.1mb/s   0.1ms/op


24.445: Per-Operation Breakdown
closefile4           16036ops     1145ops/s   0.0mb/s    0.183 us/op [0.000 us - 12.000 us]
readfile4            16036ops     1145ops/s   9.1mb/s    5.429 us/op [1.000 us - 28.000 us]
openfile4            16036ops     1145ops/s   0.0mb/s    1.320 us/op [1.000 us - 19.000 us]
closefile3           16036ops     1145ops/s   0.0mb/s    0.300 us/op [0.000 us - 13.000 us]
fsyncfile3           16036ops     1145ops/s   0.0mb/s  211.906 us/op [186.000 us - 4164.000 us]
appendfilerand3      16036ops     1145ops/s   9.0mb/s   10.203 us/op [2.000 us - 27.000 us]
readfile3            16037ops     1145ops/s   9.2mb/s    6.362 us/op [1.000 us - 23.000 us]
openfile3            16037ops     1145ops/s   0.0mb/s    1.486 us/op [1.000 us - 14.000 us]
closefile2           16037ops     1145ops/s   0.0mb/s    0.295 us/op [1.000 us - 12.000 us]
fsyncfile2           16037ops     1145ops/s   0.0mb/s  231.647 us/op [211.000 us - 4030.000 us]
appendfilerand2      16037ops     1145ops/s   8.9mb/s    8.465 us/op [2.000 us - 24.000 us]
createfile2          16037ops     1145ops/s   0.0mb/s    7.657 us/op [5.000 us - 487.000 us]
deletefile1          16037ops     1145ops/s   0.0mb/s    9.746 us/op [5.000 us - 400.000 us]
24.445: IO Summary: 208475 ops 14888.075 ops/s 2290/2290 rd/wr  36.3mb/s   0.1ms/op
...
```

Note that, due to printing IO summary every second, it shows lower throughput.

## 3. Make Replica 1's host OS stop for a while

We use `stop-machine` kernel module ([article](https://dev.to/satorutakeuchi/a-linux-kernel-module-for-stopping-the-system-for-a-while-4ja5), [github](https://github.com/satoru-takeuchi/stop-machine)) to make the host OS freeze.


### 3.1. Build stop-machine module

Change `SRCDIR` in `utils/stop-machine/Makefile` to have your kernel source path. For example, if your kernel version is `5.4.128`, set `SRCDIR` as below. You can confirm your kernel version with `uname -r`.

```Makefile
SRCDIR := /lib/modules/5.4.128/build
```

You can set the duration to stop by changing the value of `STOP_MSECS` in `stop-machine.c`. The default value is 5 seconds.

Build the kernel module.
```shell
make
```

### 3.2. Make host OS freeze

In Replica 1's host machine, installing the module will freeze the host for a given period.

```shell
sudo insmod stop-machine.ko
```

After the host is restored, uninstall the module for the next use.

```shell
sudo rmmod stop-machine
```

## 4. Run `Varmail` workload and freeze the host

Run `Varmail` workload and make Replica 1's host freeze during the execution. `Varmail` runs for 30 seconds.

```shell
cd bench/filebench
./run_all.sh -t nic -v
```

## 5. Check real-time memcopy bandwidth

Before the host freezes, you can see the bandwidth usage by the host kernel worker. After the host stops, memcopy is performed by SmartNIC.
To see the bandwidth usage of memcopy by SmartNIC you have to log in to SmartNIC directly from the primary machine or Replica 2.

```shell
# In the Primary or Replica 2 machine,
ssh 192.168.13.118  # IP address of RDMA interface of Replica 1's SmartNIC.
```

As the host is restored from a failure, memcopy is performed by the host again.
