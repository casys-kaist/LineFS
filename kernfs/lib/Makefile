
.PHONY: download all clean
all:
	#make install -C dpdk-$(DPDK_VER) T=x86_64-native-linuxapp-gcc DESTDIR=dpdk-$(DPDK_VER)/ -j8
	(cd spdk && \
	 cd scripts && \
	 sudo ./pkgdep.sh && \
	 cd .. && \
	 ./configure && \
	 make -j $(nproc))

	 # ./configure --without-isal && \ 	# ISAL might be a problem in Ubuntu 20.04

redownload:
	########
	# 	SPDK
	########
	@rm -rf spdk
	git clone https://github.com/spdk/spdk.git
	(cd spdk; git checkout v21.07; git submodule update --init)
	#(cd spdk; git checkout 88e3ffd7b; git submodule update --init)

clean:
	make clean -C spdk
