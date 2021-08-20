.PHONY: all host-init snic-init host-lib snic-lib redownload kernfs libfs bench-micro mkfs spdk-init

all:
	$(error Argument required)

host-init: redownload host-lib spdk-init kernfs libfs bench-micro mkfs

snic-init: redownload snic-lib kernfs

redownload:
	cd libfs/lib && make redownload
	cd kernfs/lib && make redownload

host-lib:
	cd libfs/lib && make all
	cd kernfs/lib && make all

snic-lib:
	cd libfs/lib && make snic-all

kernfs:
	cd kernfs && make clean && make -j`nproc` && cd tests && make clean && make -j`nproc`

libfs:
	cd libfs && make clean && make -j`nproc` && cd tests && make clean && make -j`nproc`

bench-micro:
	cd bench/micro && make

parsec:
	cd bench/parsec && if [ -d parsec-3.0 ]; then make; else make redownload && make; fi
mkfs:
	cd kernfs/tests && sudo ./mkfs.sh

spdk-init:
	sudo kernfs/lib/spdk/scripts/setup.sh

rdma:
	cd libfs/lib/rdma && make clean && make
