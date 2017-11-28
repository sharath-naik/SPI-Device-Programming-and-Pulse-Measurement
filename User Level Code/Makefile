
IOT_HOME = /opt/iot-devkit/1.7.2/sysroots

PATH := $(PATH):$(IOT_HOME)/x86_64-pokysdk-linux/usr/bin/i586-poky-linux

CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SROOT=$(IOT_HOME)/i586-poky-linux/

APP1 = spi

all:
	$(CC) -o $(APP1) -pthread -Wall --sysroot=$(SROOT) main3.c Gpio_func.c

clean:
	rm -f *.o
	rm -f $(APP1) 
