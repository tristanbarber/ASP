obj-m := assignment5_drv.o

KERNEL_DIR = /usr/src/linux-headers-$(shell uname -r)

all:
	@$(MAKE) -C $(KERNEL_DIR) M=$(shell pwd) modules

