lease - ironore15
==================================
Initially implemented by Waleed Reda.

Improved by ironore15.

### Makefile Options ###

##### 1. Kernfs Makefile FLAGS
To use lease functionalities, `USE_LEASE` option is necessary.
~~~
MLFS_FLAGS += -DUSE_LEASE
~~~
`LEASE_OPT` option should be disabled. It remains because of older version of lease implementation.
~~~
#MLFS_FLAGS += -DLEASE_OPT
~~~
`LEASE_MIGRATION` option should be disabled. It is not stable yet.
~~~
#MLFS_FLAGS += -DLEASE_MIGRATION
~~~
If you want more debugging messages, add `LEASE_DEBUG` option.
~~~
MLFS_FLAGS += -DLEASE_DEBUG
~~~

##### 2. Libfs Makefile FLAGS
To use lease functionalities, `USE_LEASE` and `LAZY_SURRENDER` options are necessary.
~~~
MLFS_FLAGS += -DUSE_LEASE
MLFS_FLAGS += -DLAZY_SURRENDER
~~~
`LEASE_OPT` option should be disabled. It remains because of older version of lease implementation.
~~~
#MLFS_FLAGS += -DLEASE_OPT
~~~
If you want more debugging messages, add `LEASE_DEBUG` option.
~~~
MLFS_FLAGS += -DLEASE_DEBUG
~~~

### Recommended testcases ###

##### 1. lease_test
Below three commands should be executed at each terminal in order.
~~~
./lease_test w 100 1 /
./lease_test w 100 1 /
./lease_test s
~~~

##### 2. iobench
Below three commands should be executed at each terminal in order.
~~~
./iobench sw 100M 1M 1
./iobench sw 100M 1M 1
./iobench -p
~~~
