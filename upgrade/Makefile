#
# Copyright (C) 2009-2012 MapleLeaf Software, Inc
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

TOP = ../..

SOURCES  = upgrademanager.c
SOURCES += netlink.c
SOURCES += swupdate.c
SOURCES += socketlist.c
SOURCES += xmlConfig.c
SOURCES += xmlparser.c

CFLAGS  = -Wall
CFLAGS += -I.
CFLAGS += -I$(TOP)/simulib/include

CFLAGS += -I$(TOP)/simulib/src
CFLAGS += -I$(TOP)/simulib/include
CFLAGS += -I$(TOP)/simulib/include

LDFLAGS =

# Default architecture is x86
ARCH ?= x86

CROSS_COMPILE ?=
ifeq ($(CROSS_COMPILE),arm-none-linux-gnueabi-)
ARCH = arm
INSTALL_DIR = $(TOP)/rootfs/usr/scu/bin
CFLAGS += --sysroot=$(TOP)/rootfs
CFLAGS += -I$(TOP)/rootfs/usr/include/libxml2
LDFLAGS += --sysroot=$(TOP)/rootfs
else
INSTALL_DIR = /usr/scu/bin
CFLAGS += -I/usr/include/libxml2
endif

ifeq ($(ARCH),arm)
CFLAGS += -mtune=cortex-a8
CFLAGS += -mfpu=neon
CFLAGS += -ftree-vectorize
CFLAGS += -mfloat-abi=softfp
CFLAGS += -mcpu=arm9
CFLAGS += -DSCU_BUILD
endif

LIBS = -lcrypto -lxml2  -lz

CC		= $(CROSS_COMPILE)gcc
LD		= $(CROSS_COMPILE)gcc
EXECUTABLE	= upgrademanager
TARGETDIR       = $(ARCH)

ifeq ($(findstring debug,$(MAKECMDGOALS)),debug)
OBJDIR = $(TARGETDIR)/debug
CFLAGS += -g -DDEBUG
LDFLAGS += -g
else
OBJDIR = $(TARGETDIR)/release
CFLAGS += -O2
endif

CFLAGS += $(EXTRA_CFLAGS)

PROGRAMDIR      = $(OBJDIR)
PROGRAM		= $(PROGRAMDIR)/$(EXECUTABLE)
MAP		= $(PROGRAMDIR)/$(EXECUTABLE).map

RM := rm -rf

OBJECTS = $(addprefix $(OBJDIR)/,$(SOURCES:.c=.o))

all: $(PROGRAM)

debug: $(PROGRAM)

$(OBJDIR):
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)

# Tool invocations
$(PROGRAM): $(OBJECTS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	@$(RM) $(ARCH)
	@echo "Clean complete"

install: $(PROGRAM)
	@echo "Installing $(PROGRAM)..."
	@mkdir -p $(INSTALL_DIR)
	@cp $(PROGRAM) $(INSTALL_DIR)
	@echo "Installation complete"

$(OBJDIR)/%.o:  %.c
	@rm -f $@
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -MMD -MP -o $@ $<


ifneq ($(MAKECMDGOALS),clean)
-include $(OBJECTS:.o=.d)
endif
