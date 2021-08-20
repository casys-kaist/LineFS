DPDK_VER := 16.11

.PHONY: download all clean
all:
	#make install -C dpdk-$(DPDK_VER) T=x86_64-native-linuxapp-gcc DESTDIR=dpdk-$(DPDK_VER)/ -j8
	sudo apt install -y libndctl-dev libdaxctl-dev
	make -C nvml -j $(nproc)
	sudo apt install -y build-essential cmake gcc libudev-dev libnl-3-dev libnl-route-3-dev ninja-build pkg-config valgrind
	(cd rdma-core && ./build.sh)
	(cd rdma && make)
	sudo apt install -y pkg-config libcapstone-dev
	(cd syscall_intercept; \
	 mkdir build; \
	 mkdir install; \
	 cd build; \
	 cmake -DCMAKE_INSTALL_PREFIX=$(shell pwd)/syscall_intercept/install -DCMAKE_BUILD_TYPE=Release ..; \
	 make; \
	 make install)
	if [ ! -d jemalloc-4.5.0 ]; then tar -xf jemalloc-4.5.0.tar.bz2; fi
	(cd jemalloc-4.5.0 && \
	 ./autogen.sh && \
	 ./configure && \
	 make && \
	 sudo make install)

snic-all:
	#make -C nvml -j $(nproc)
	(cd rdma-core && ./build.sh)
	(cd rdma && make)
	if [ ! -d jemalloc-4.5.0 ]; then tar -xf jemalloc-4.5.0.tar.bz2; fi
	(cd jemalloc-4.5.0 && \
	 ./autogen.sh && \
	 ./configure && \
	 make && \
	 sudo make install)

#shouldn't redownload DPDK, there is an issue right now with fPIC; it's manually
#added in its Makefile (mk/rte.lib.mk, line 53 has CFLAGS += -fPIC).
#DPDK has a config to make it shared (CONFIG_RTE_BUILD_SHARED_LIB=y)
#but it breaks SPDK compilation
redownload:
	########
	# 	NVML
	########
	@rm -rf nvml
	git clone https://github.com/pmem/nvml
	(cd nvml && git checkout tags/1.7)
	########
	#       syscall_intercept
	#
	@rm -rf syscall_intercept
	git clone https://github.com/pmem/syscall_intercept.git
	if [ ! -d "jemalloc-4.5.0" ]; then tar xf jemalloc-4.5.0.tar.bz2; fi

clean:
	@rm -rf dpdk-$(DPDK_VER)/x86_64-native-linuxapp-gcc
	make clean -C nvml
	@rm -rf rdma-core/build
