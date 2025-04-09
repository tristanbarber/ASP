obj-m := assignment6.o

KERNEL_DIR = /usr/src/linux-headers-$(shell uname -r)

all:
	@$(MAKE) -C $(KERNEL_DIR) M=$(shell pwd) modules 
	@gcc -o test test.c


clean:
	@rm -rf *.o *.ko *.mod.* *.symvers *.order *.cmd *.tmp_versions test assignment6.mod .*.cmd .*.o.cmd .*.ko.cmd .*.mod.cmd .*.mod.o.cmd .*.mod.o .*.mod.o.d .*.o.d .*.tmp_versions .*.symvers
