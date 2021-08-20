SYS ?= $(shell gcc -dumpmachine)
export SYS

export DEBUG

#DEBUG=1

GLIBC_SRC = glibc-2.28
SHIM_SRC = src
BUILD_DIR = glibc-build-all
GLIBC_TARGET = $(addprefix $(BUILD_DIR)/,libc.so.6 ld-linux-x86-64.so.2 libpthread.so.0 libm.so.6 libdl.so.2 libutil.so.1 crt1.o crti.o crtn.o liblibos.so.1)

all: $(GLIBC_TARGET)

shim: $(SHIM_SRC)
	$(MAKE) -C $(SHIM_SRC) all

install:
	[ -d glibc-build ] || mkdir -p glibc-build
	./buildglibc.py install --quiet

ifneq (,$(filter $(SYS), x86_64-linux-gnu aarch64-redhat-linux))
$(GLIBC_TARGET): $(BUILD_DIR)/Makefile
	cd $(BUILD_DIR) && $(MAKE)

$(BUILD_DIR)/Makefile: $(GLIBC_SRC)/configure
ifeq ($(DEBUG),1)
	./buildglibc.py --quiet --debug
else
	./buildglibc.py --quiet
endif

$(GLIBC_SRC)/configure:
	[ -f $(GLIBC_SRC).tar.gz ] || \
	wget http://ftp.gnu.org/gnu/glibc/$(GLIBC_SRC).tar.gz
	tar -xzf $(GLIBC_SRC).tar.gz
	cp -r glibc-2.19-shim/* $(GLIBC_SRC)
	cd $(GLIBC_SRC) && patch -p1 < ../glibc-2.19-shim.patch

.PHONY: pack
pack: $(GLIBC_TARGET)
	@../Scripts/pack_binaries.sh glibc $^
	$(MAKE) -C $(SHIM_DIR) pack

clean:
	@#$(MAKE) -C $(SHIM_DIR) clean
	rm -rf $(BUILD_DIR)
else
$(GLIBC_TARGET): .packed/glibc.tar.gz
	tar -xmozf $< $@

clean:
	rm -rf $(BUILD_DIR)
endif
