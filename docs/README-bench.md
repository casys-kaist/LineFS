# Readme for Running Benchmarks in LineFS Paper  <!-- omit in toc -->

- [1. Set TTY variables](#1-set-tty-variables)
- [2. Pinning CPU cores](#2-pinning-cpu-cores)
- [3. Microbenchmarks (throughput and latency)](#3-microbenchmarks-throughput-and-latency)
	- [3.1. Build microbenchmarks](#31-build-microbenchmarks)
	- [3.2. Run microbenchmarks](#32-run-microbenchmarks)
- [4. LevelDB](#4-leveldb)
	- [4.1. Required package](#41-required-package)
	- [4.2. Build LevelDB](#42-build-leveldb)
	- [4.3. Run LevelDB](#43-run-leveldb)
- [5. Filebench](#5-filebench)
	- [5.1. Rebuild LineFS without `BATCH_MEMCPY_LIST` flag](#51-rebuild-linefs-without-batch_memcpy_list-flag)
	- [5.2. Build filebench](#52-build-filebench)
	- [5.3. Disable ASLR](#53-disable-aslr)
	- [5.4. Run filebench](#54-run-filebench)
	- [5.5. Known issues](#55-known-issues)
- [6. Experiments to be added](#6-experiments-to-be-added)

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

## 3. Microbenchmarks (throughput and latency)

### 3.1. Build microbenchmarks

```shell
cd bench/micro
make -j`nproc`
```

### 3.2. Run microbenchmarks

To run throughput microbenchmark:

```shell
cd bench/micro
./run_all.sh
```

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

## 4. LevelDB

### 4.1. Required package

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

### 4.2. Build LevelDB

* gcc 7 was used. (gcc 4.8 does not enable snappy compression.)

```shell
cd bench/leveldb
make -j`nproc`
```

### 4.3. Run LevelDB

``` shell
cd bench/leveldb/mlfs
./run_all.sh
```

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

## 5. Filebench

### 5.1. Rebuild LineFS without `BATCH_MEMCPY_LIST` flag

Currently, filebench is not patched to utilize batching RPC requests to the kernel worker (§4, §5.2.4). Disable it by commenting out the lines that set `BATCH_MEMCPY_LIST` flag in `kernfs/Makefile` and `libfs/Makefile`.

### 5.2. Build filebench

>The distributed source code includes a modified `bench/filebench/Makefile.in` file for LibFS use. This file is automatically generated during the installation of filebench. (*Step 1* described in `bench/filebench/README`)
If you regenerate autotool scripts (*Step 1*) due to some reasons, e.g. an `aclocal` version mismatch after OS upgrade, `Makefile.in` will be overwritten. In this case, you have to manually apply the LineFS related modifications on the original `Makefile.in` to a new `Makefile.in` file. You can easily identify the lines by searching the keyword "mlfs" or "MLFS" in the original `Makefile.in` file.

The installation process is as below. Refer to `bench/filebench/README` file for details.

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

### 5.3. Disable ASLR

Disabling ASLR is required.
Related issue: [link](https://github.com/filebench/filebench/issues/112)

```shell
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
```

### 5.4. Run filebench

```shell
cd bench/filebench
./run_all.sh
```

You can select experiments to run by changing `bench/filebench/run_all.sh` script.

### 5.5. Known issues

- Filebench hangs at the end of the execution.
  - Printed message:

    ```shell
    ...
    [worker] shutting down mlfs
    ```

  - Workaround: Exit the program (`sudo pkill -9 filebench`). It doesn't affect the experiment results.

## 6. Experiments to be added

- Performance interference experiment
- Assise-opt
- Diverse copying method experiment
