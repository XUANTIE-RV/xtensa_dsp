##
 # Copyright (C) 2021 Alibaba Group Holding Limited
 #
 # This program is free software; you can redistribute it and/or modify
 # it under the terms of the GNU General Public License version 2 as
 # published by the Free Software Foundation.
##


# INCLUDES = -I../../driver/isp -I../../common

TARGET = libxrp_linux.a

CFLAGS = -O0 -Wall -g -lm -lpthread
CFLAGS += -fPIC

# USE_THREADS = 

# ifdef USE_THREADS
queue_sources = xrp_threaded_queue.c
INCLUDE +=-Ithread-pthread 
# else
# queue_sources = xrp_sync_queue.c
# endif

xrp_SRCS += $(queue_sources)
xrp_SRCS += xrp_host_common.c
xrp_SRCS += hosted/xrp_linux.c
xrp_SRCS += xrp_report.c

INCLUDES = -I$(CURDIR) -Ihosted -I../include -Ithread-pthread -I../xrp-common -I../../xrp-kernel
# object files will be generated from .c sourcefiles
xrp_OBJS = $(notdir $(xrp_SRCS:.c=.o))

all: $(TARGET)


$(xrp_OBJS): $(xrp_SRCS)
	$(CC) -c $(CFLAGS) $(INCLUDES)  $(xrp_SRCS)


$(TARGET): $(xrp_OBJS)
	$(AR)  -rc $(TARGET) $(xrp_OBJS)

clean:
	rm -f *.o *.a
	rm -rf output

install:

.PHONY: clean all prepare common

