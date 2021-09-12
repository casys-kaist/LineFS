# Readme for running CPU intensive job <!-- omit in toc -->

- [1. streamcluster of PARSEC 3.0](#1-streamcluster-of-parsec-30)
	- [1.1. Get source code](#11-get-source-code)
	- [1.2. Dependency](#12-dependency)
	- [1.3. Build stremacluster](#13-build-stremacluster)
	- [1.4. Run streamcluster individually in long mode](#14-run-streamcluster-individually-in-long-mode)
	- [1.5. Run streamcluster individually in short mode](#15-run-streamcluster-individually-in-short-mode)

## 1. streamcluster of PARSEC 3.0

### 1.1. Get source code

Download source code from the [PARSEC 3.0 official site](https://parsec.cs.princeton.edu).

### 1.2. Dependency

Some packages are required to build and run PARSEC 3.0. Refer to the 'Requirement' section on the [Download page](https://parsec.cs.princeton.edu/download.htm).

### 1.3. Build stremacluster

At the project root directory, run:

```shell
make parsec
```

### 1.4. Run streamcluster individually in long mode

> In `long mode`, `streamcluster` runs for quite a long time. Refer to `bench/parsec/scripts/run.sh` for long mode.

At the project root directory, run:

```shell
scripts/run_parsec.sh -r # Run streamcluster on Replica 1 and Replica 2.
scripts/run_parsec.sh -r -l # Run streamcluster on Primary, Replica 1 and Replica 2.
scripts/run_parsec.sh -k # Kill running streamcluster processes.
```

### 1.5. Run streamcluster individually in short mode

`short` mode is for measuring a degree of contention between  `streamcluster` and DFS.

```shell
scripts/run_parsec.sh -s # Run streamcluster on all three machines.
```
