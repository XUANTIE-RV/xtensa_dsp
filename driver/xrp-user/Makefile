##
 # Copyright (C) 2021 Alibaba Group Holding Limited
 #
 # This program is free software; you can redistribute it and/or modify
 # it under the terms of the GNU General Public License version 2 as
 # published by the Free Software Foundation.
##


# INCLUDES = -I../../driver/isp -I../../common

TARGET = libdsp.so

CFLAGS = -O2 -Wall -g -lm -lpthread
CFLAGS:=-fPIC
LDFLAGS:=-shared -fpic
LDFLAGS+=--whole-archive 
LDFLAGS += -Lxrp-host -lxrp_linux
LDFLAGS += -Lxrp-common -lxrp-common
LDFLAGS+=--no-whole-archive
# LDFLAGS += -Lxrp-host -lxrp_linux
# LDFLAGS += -Lxrp-common -lxrp-common
# LIB_PATH += -Lxrp-host
# LIB_PATH += -Lxrp-common 
# LIBS += -lpthread
# LIBS += -lxrp_linux
# LIBS += -lxrp-common
# USE_THREADS = 


xrp_SRCS += dsp-ps/csi_dsp_core.c
# xrp_SRCS += dsp-ps/dsp_ps_core.c
xrp_SRCS += dsp-ps/csi_dsp_helper.c
xrp_SRCS += dsp-ps/dsp_common.c

INCLUDES = -I$(CURDIR) -Ihosted -Iinclude -Ithread-pthread -I../xrp-common -I../../xrp-kernel
# object files will be generated from .c sourcefiles
xrp_OBJS = $(notdir $(xrp_SRCS:.c=.o))

all: $(TARGET)

XRP_COMMON := xrp-common
$(XRP_COMMON): DUMMY
	make -C $(XRP_COMMON)

XRP_LINUX:=xrp-host
$(XRP_LINUX): DUMMY
	make -C $(XRP_LINUX)

DUMMY:


$(xrp_OBJS): $(xrp_SRCS)
	$(CC) -c $(CFLAGS) $(INCLUDES)  $(xrp_SRCS)


$(TARGET): $(xrp_OBJS) $(XRP_COMMON) $(XRP_LINUX)
	$(LD)  $(LDFLAGS) -o $(TARGET) $(xrp_OBJS) $(LIB_PATH) $(LIBS)

clean:
	rm -f *.o
	rm -f *.so
	rm -rf output

install:

.PHONY: clean all prepare common

