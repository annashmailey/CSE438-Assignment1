KDIR:= /opt/iot-devkit/1.7.3/sysroots/i586-poky-linux/usr/src/kernel
PWD:= $(shell pwd)

CC = /opt/iot-devkit/1.7.3/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux/i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
#SROOT = /opt/iot-devkit/1.7.3/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux
#/opt/iot-devkit/1.7.3/sysroots/i586-poky-linux/usr/src/kernel


obj-m:= sharedQueue.o
sharedQueue-objs := sharedQueue.o Assignment1_part2.o


all:
	make -C $(KDIR) M=$(PWD) modules

	#/opt/iot-devkit/1.7.3/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux-gcc Assignment1_part2.c -pthread --sysroot=$(SROOT) 

clean: 
	rm -f *.o
	rm -f *.ko
	rm -f *.mod.c
	rm -fr *.order
	rm -fr *.symvers
	rm -rf .tmp_versions
	rm -f *.mod.o
	rm -f *.cmd
