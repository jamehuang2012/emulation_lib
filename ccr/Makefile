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



CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSSCOMPILE)ld
LDFLAGS := -shared -fpic -lrt -lcrypto -L../logger -llogger -L../pm -lpm -L/usr/scu/libs

TOP = ..
OBJS  = FileProc.o
OBJS += CollectLogEx.o
OBJS += Command.o
OBJS += config.o
OBJS += Connect.o
OBJS += CRC_Proc.o
OBJS += crypto.o
OBJS += cmgrapi.o
OBJS += Disconnect.o
OBJS += DLE_EOT.o
OBJS += ProperDataDef.o
OBJS += Prtcl_RS232C_8.o
OBJS += Prtcl_RS232C_8_FuncDef.o
OBJS += RS232C_Interface.o
OBJS += RS232C_UserInterface_Proc.o
OBJS += Transmit.o

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

.PHONY:intall
install:
	cp $(TARGET) /usr/scu/libs
