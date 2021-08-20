CONFIG_MODULE_SIG=n

TARGET = kern_float
obj-m += $(TARGET).o
$(TARGET)-objs := main.o

PWD := $(shell pwd)
KERNDIR := /lib/modules/$(shell uname -r)/build
KERNSRC := /lib/modules/$(shell uname -r)/source

#EXTRA_CFLAGS := -I$(KERNSRC)/fs/proc/ -mhard-float -Og

all:
	make -C $(KERNDIR) M=$(PWD) modules

clean:
	make -C $(KERNDIR) M=$(PWD) clean
