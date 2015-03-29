ARCH ?= x86

ifeq ($(ARCH),arm)
CROSS_COMPILE   ?= 

CFLAGS += -mtune=cortex-a8
CFLAGS += -mfpu=neon
CFLAGS += -ftree-vectorize
CFLAGS += -mfloat-abi=softfp
CFLAGS += -mcpu=arm9
CFLAGS += -DSCU_BUILD
CFLAGS += -g -DDEBUG

endif

CFLAGS += -g -DDEBUG



CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSSCOMPILE)ld
LDFLAGS := -shared -fpic -lrt -lcrypto -L../logger -llogger -L../pm -lpm -L/usr/scu/libs

TOP = ..
OBJS  = ccr_api.o

TARGET  := libccr.so
INCLUDE_PATH = -I./lua -I../logger -I../include

CFLAGS += -I$(TOP)/include
CFLAGS += -I$(TOP)/src
CFLAGS += -pthread -D_GNU_SOURCE
CFLAGS += $(INCLUDE_PATH)
#CFLAGS +=  -fPIC
# Default architecture is x86

all:$(OBJS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o:%.c
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY:clean

clean:
	@rm *.so *.o -rf

.PHONY:exe
exe:
	$(CC) -o testccr testccr.c $(CFLAGS) -lrt
.PHONY:intall
install:
	cp $(TARGET) /usr/scu/libs
