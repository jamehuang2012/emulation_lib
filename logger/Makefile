CC      := gcc
LD      := ld
LDFLAGS := -shared -fpic -lrt 

TOP =..
OBJS =  logger.o queue.o 
TARGET:= liblogger.so
ARCH ?= x86


CFLAGS += -I$(TOP)/include
CFLAGS += -I$(TOP)/src
CFLAGS += -pthread -D_GNU_SOURCE
CFLAGS += $(INCLUDE_PATH)
#CFLAGS += -fPIC
# Default architecture is x86

ifeq ($(ARCH),arm)
CFLAGS += -mtune=cortex-a8
CFLAGS += -mfpu=neon
CFLAGS += -ftree-vectorize
CFLAGS += -mfloat-abi=softfp
CFLAGS += -mcpu=arm9
CFLAGS += -DSCU_BUILD
else

#CFLAGS += -fPIC
endif



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
                                      
