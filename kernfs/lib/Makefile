
.PHONY: download all clean
all:
	#make install -C dpdk-$(DPDK_VER) T=x86_64-native-linuxapp-gcc DESTDIR=dpdk-$(DPDK_VER)/ -j8
	(cd spdk && \
	 cd scripts && \
	 sudo ./pkgdep.sh && \
	 cd .. && \
	 ./configure --without-isal && \
	 make -j $(nproc))

	 # ./configure && \

redownload:
	########
	# 	SPDK
	########
	@rm -rf spdk
	git clone https://github.com/spdk/spdk.git
	(cd spdk; git checkout v21.07; git submodule update --init)

clean:
	make clean -C spdk
