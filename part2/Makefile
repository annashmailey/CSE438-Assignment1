KDIR:= /opt/iot-devkit/1.7.3/sysroots/i586-poky-linux/usr/src/kernel
PWD:= $(shell pwd)

#SROOT:= /opt/iot-devkit/1.7.3/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux

obj-m:= sharedQueue.o

all:
	make -C /lib/modules/$(shell uname -r)/build $(KDIR) M=$(PWD) modules

	#gcc -o assignment1 assignment1_part2.c --sysroot=$(SROOT) -pthread
	#make -C $(KDIR) M=$(PWD) modules

clean: 
	rm -f *.o
	rm -f *.ko
	rm -f *.mod.c
	rm -f modules.order
	rm -f Module.symvers
	rm -rf .tmp_versions
	rm -f *.mod.o
	rm -f \.*.cmd
	rm -f Module.markers
