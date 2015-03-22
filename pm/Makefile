ARCH ?= x86


ifeq ($(ARCH),arm)
ARCH 		?=arm
CROSS_COMPILE   ?= 
CFLAGS += -mtune=cortex-a8
CFLAGS += -mfpu=neon
CFLAGS += -ftree-vectorize
CFLAGS += -mfloat-abi=softfp
CFLAGS += -mcpu=arm9
CFLAGS += -DSCU_BUILD
CFLAGS += -g -DDEBUG
endif


CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
#LDFLAGS := -shared -fpic -lrt -lcrypto -L/usr/scu/libs
LDFLAGS := -shared -fpic -lrt -lcrypto -L../logger -llogger -L/usr/scu/libs -ludev

TOP =..
OBJS += pdm.o 
OBJS += pm.o 
OBJS += scm.o 
OBJS += pdmqueue.o 
OBJS += usbdevice.o 

TARGET:= libpm.so


INCLUDE_PATH = -I./lua -I../logger

CFLAGS += -I$(TOP)/include
CFLAGS += -I$(TOP)/src
CFLAGS += -pthread -D_GNU_SOURCE
CFLAGS += $(INCLUDE_PATH)
# Default architecture is x86


all:$(OBJS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS) 

%.o:%.c
	$(CC) -c -o $@ $< $(CFLAGS)  

.PHONY:clean

clean:  
	@rm *.so *.o -rf

.PHONY:intall
install:
	@cp $(TARGET) /usr/scu/libs
                                      
