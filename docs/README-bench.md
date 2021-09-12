# Readme for Running Benchmarks in LineFS Paper  <!-- omit in toc -->

- [1. Set TTY variables](#1-set-tty-variables)
- [2. Pinning CPU cores](#2-pinning-cpu-cores)
- [3. Setup `streamcluster` benchmark](#3-setup-streamcluster-benchmark)
- [4. Microbenchmarks (throughput and latency)](#4-microbenchmarks-throughput-and-latency)
	- [4.1. Build microbenchmarks](#41-build-microbenchmarks)
	- [4.2. Run microbenchmarks](#42-run-microbenchmarks)
	- [4.3. Check results](#43-check-results)
- [5. Streamcluster performance interference experiment](#5-streamcluster-performance-interference-experiment)
	- [5.1. Build throughput microbenchmark for interference experiment](#51-build-throughput-microbenchmark-for-interference-experiment)
	- [5.2. Run interference experiment](#52-run-interference-experiment)
	- [5.3. Check interference experiment result](#53-check-interference-experiment-result)
- [6. LevelDB](#6-leveldb)
	- [6.1. Required package](#61-required-package)
	- [6.2. Build LevelDB](#62-build-leveldb)
	- [6.3. Run LevelDB](#63-run-leveldb)
- [7. Filebench](#7-filebench)
	- [7.1. Rebuild LineFS without `BATCH_MEMCPY_LIST` flag](#71-rebuild-linefs-without-batch_memcpy_list-flag)
	- [7.2. Build filebench](#72-build-filebench)
	- [7.3. Disable ASLR](#73-disable-aslr)
	- [7.4. Run filebench](#74-run-filebench)
		- [7.4.1. Known issues](#741-known-issues)
- [8. Experiments to be added](#8-experiments-to-be-added)

All the benchmarks run on Primary host machine unless otherwise specified.

## 1. Set TTY variables

Set TTY session ID in `bench/micro/scripts/consts.sh` file. Scripts execute commands remotely over ssh and redirect stdout to the terminals based on the IDs. You can find the ID of an open session with the command `tty`.

```shell
$ tty
/dev/pts/13 # ID of this terminal session is 13.
```

For example, if my file names connected to opened terminal are:

- Primary host: /dev/pts/4
- Replica 1 host: /dev/pts/1
- Replica 2 host: /dev/pts/1
- Primary nic: /dev/pts/0
- Replica1 nic: /dev/pts/1
- Replica1 nic: /dev/pts/0

set the values of TTY variables as below.

```shell
# Set tty for output. You can find the number X (/dev/pts/X) with the command: `tty`
HOST_1_TTY=4
HOST_2_TTY=1
HOST_3_TTY=1

NIC_1_TTY=0
NIC_2_TTY=1
NIC_3_TTY=0
```

## 2. Pinning CPU cores

Pinning CPU cores to a single NUMA node improves the performance of LineFS. You need to configure it at the following files:

- `mlfs_config.sh`: Set `NUMA_NODE`. For example, set its value to 1 to use NUMA node 1.
- `scripts/global.sh`: Set `PINNING`. For example, set `PINNING="numactl -N0 -m0"` to use NUMA node 0 and set `PINNING="numactl -N1 -m1"` to use NUMA node 1.
- `scripts/run_stress_ng.sh`: Set `taskset` argument. For example, if you want to run stress-ng on NUMA node 1 that includes 16 cores, from core 16 to core 31, set `--taskset 16-31`.

## 3. Setup `streamcluster` benchmark

We use `streamcluster` of Parsec 3.0 as a CPU-intensive job that contending with benchmarks. Please refer to [README-parsec](README-parsec.md) to set up `streamcluster`.
After confirming that `streamcluster` runs correctly, you don't need to run it manually. Instead, benchmark scripts provided by this repository will run `streamcluster` automatically.

## 4. Microbenchmarks (throughput and latency)

> This benchmarks are related to Figure 4 and Table 2 of LineFS paper.

### 4.1. Build microbenchmarks

```shell
cd bench/micro
make -j`nproc`
```

### 4.2. Run microbenchmarks

To run microbenchmarks:

```shell
cd bench/micro
./run_all.sh
```

`bench/micro/run_all.sh` automatically compiles and deploys DFS, runs the microbenchmarks, and prints the results.

You can select which benchmark to run in the `bench/micro/run_all.sh` script. For example, a line `runLatencyMicrobench linefs streamcluster` executes the latency microbenchmark with streamcluster as a CPU-intensive job on LineFS.

```shell
...
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then # script is executed directly.
	# Kill all iobench processes.
	sudo pkill -9 iobench

	# Build linefs.
	buildLineFS

	# Run LineFS.
	runLatencyMicrobench linefs
	runLatencyMicrobench linefs streamcluster
	runThroughputMicrobench linefs
	runThroughputMicrobench linefs streamcluster

	# Build assise.
	buildAssise

	# Run Assise.
	runLatencyMicrobench assise
	runLatencyMicrobench assise streamcluster
	runThroughputMicrobench assise
	runThroughputMicrobench assise streamcluster

	# Print results.
	printLatencyMicrobenchResults linefs
	printLatencyMicrobenchResults assise
	printThroughputMicrobenchResults linefs
	printThroughputMicrobenchResults assise
fi
...
```

To change benchmark options like I/O size and the file size, modify `bench/micro/scripts/run_iobench_lat.sh` for the latency benchmark and `bench/micro/scripts/run_iobench.sh` for the throughput benchmark.

### 4.3. Check results

The results are printed to the terminal. You can print the results again with `bench/micro/run_all.sh` script. Enable only functions printing results and run it.

``` shell
...
 # Print results.
 printLatencyMicrobenchResults linefs
 printLatencyMicrobenchResults assise
 printThroughputMicrobenchResults linefs
 printThroughputMicrobenchResults assise
...
```

Here is an example of the result of a one-time execution running on our testbed.
You can find that latencies of cpu-idle and cpu-busy are 147.99 us and 147.34 us respectively. The throughput values are in MB/s. The numbers can be changed as we optimize LineFS.

```shell
##########################################################
#   Latency microbench results of linefs in microseconds
##########################################################
linefs solo:
sw
io_size,avg,p99,p99.9,fsync-avg
16K,147.99,185.67,199.10,138.06
linefs cpu-busy:
sw
io_size,avg,p99,p99.9,fsync-avg
16K,147.34,183.52,199.84,137.41

#########################################################
#   Throughput microbench results of linefs in MB/s
#########################################################
linefs solo:
sw_16K_1procs_1round 1460.680
sw_16K_2procs_1round 2061.809
sw_16K_4procs_1round 2113.059
sw_16K_8procs_1round 2012.788

linefs cpu-busy:
sw_16K_1procs_1round 1484.208
sw_16K_2procs_1round 1655.513
sw_16K_4procs_1round 1746.651
sw_16K_8procs_1round 1785.106
```

> You will get better LineFS latency than around 210 us that described in the paper as we have optimized the latency.

## 5. Streamcluster performance interference experiment

> This benchmarks are related to Figure 7 of LineFS paper.

### 5.1. Build throughput microbenchmark for interference experiment

You have to build microbenchmarks. Refer to [Build microbenchmarks](#31-build-microbenchmarks).

### 5.2. Run interference experiment

```shell
cd bench/micro
./run_interference_exp.sh
```

`run_interference_exp.sh` automatically configures, compiles, and deploys DFS, runs benchmarks, and prints the results.

### 5.3. Check interference experiment result

Results are prompted on the terminal. Here is an example of the result.

```shell
###################################################################
#   Interference Experiment Result
#     - Streamcluster execution time in seconds
#     - Throughput in MB/s
###################################################################
solo
Primary  24.238
Replica  23.782
linefs
Primary  29.769
Replica  27.389
Throughput: 1434.824 MB
assise
Primary  36.264
Replica  29.739
Throughput: 631.580 MB
```

## 6. LevelDB

> This benchmarks are related to Figure 8 (a) of LineFS paper.

### 6.1. Required package

Install [snappy](https://github.com/google/snappy) package.

```shell
sudo apt install libsnappy-dev
```

Or, install it from source:
```shell
git clone https://github.com/google/snappy.git
cd snappy
git submodule update --init     # if you download source code from github.
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON ../
make && sudo make install
```

### 6.2. Build LevelDB

* gcc 7 was used. (gcc 4.8 does not enable snappy compression.)

```shell
cd bench/leveldb
make -j`nproc`
```

### 6.3. Run LevelDB

``` shell
cd bench/leveldb/mlfs
./run_all.sh
```

`bench/leveldb/run_all.sh` automatically compiles and deploys DFS, runs LevelDB, and prints the results.

You can select experiments to run in `bench/leveldb/mlfs/run_all.sh`.

> Sometimes, LevelDB run with `fillrandom` or `fillsync` workload hangs while printing a result histogram. You can run the workload again by changing `bench/leveldb/mlfs/run_bench_histo.sh`. For example,
>
>from
>
>```shell
> ...
> for WL in fillseq,readseq fillseq,readrandom fillseq,readhot fillseq fillrandom fillsync; do
> ...
>```
>
>to
>
>```shell
> ...
> for WL in fillrandom fillsync; do
> ...
>```
>

## 7. Filebench

> This benchmarks are related to Figure 8 (b) of LineFS paper.

### 7.1. Rebuild LineFS without `BATCH_MEMCPY_LIST` flag

Currently, filebench is not patched to utilize batching RPC requests to the kernel worker (§4, §5.2.4). Disable it by commenting out the lines that set `BATCH_MEMCPY_LIST` flag in `kernfs/Makefile` and `libfs/Makefile`.

### 7.2. Build filebench

>The distributed source code includes a modified `bench/filebench/Makefile.in` file for LibFS use. This file is automatically generated during the installation of filebench. (*Step 1* described in `bench/filebench/README`)
If you regenerate autotool scripts (*Step 1*) due to some reasons, e.g. an `aclocal` version mismatch after OS upgrade, `Makefile.in` will be overwritten. In this case, you have to manually apply the LineFS related modifications on the original `Makefile.in` to a new `Makefile.in` file. You can easily identify the lines by searching the keyword "mlfs" or "MLFS" in the original `Makefile.in` file.

The installation process is as below. Refer to `bench/filebench/README` file for details. ***Build using `gcc-4.8` on Ubuntu 18.04.***

```shell
cd bench/filebench
cp Makefile.in Makefile.in.old  # Backup original file.
libtoolize
aclocal
autoheader
automake --add-missing
autoconf

# Restore lines containing keywords 'mlfs' and 'MLFS' in Makefile.in

./configure
make  # Without -j option.
```

### 7.3. Disable ASLR

Disabling ASLR is required.
Related issue: [link](https://github.com/filebench/filebench/issues/112)

```shell
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
```

### 7.4. Run filebench

```shell
cd bench/filebench
./run_all.sh
```

`bench/filebench/run_all.sh` automatically configures, compiles, and deploys DFS, runs Filebench, and prints the results.

You can select experiments to run by changing `bench/filebench/run_all.sh` script.

#### 7.4.1. Known issues

- Filebench hangs at the end of the execution.
  - Printed message:

    ```shell
    ...
    [worker] shutting down mlfs
    ```

  - Workaround: Exit the program (`sudo pkill -9 filebench`). It doesn't affect the experiment results.

## 8. Experiments to be added

- Performance interference experiment
- Assise-opt
- Diverse copying method experiment
