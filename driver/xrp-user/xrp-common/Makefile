##
 # Copyright (C) 2021 Alibaba Group Holding Limited
 #
 # This program is free software; you can redistribute it and/or modify
 # it under the terms of the GNU General Public License version 2 as
 # published by the Free Software Foundation.
##


TARGET = libxrp-common.a

CFLAGS = -O0 -Wall -g -lm
CFLAGS += -fPIC


xrp_SRCS += xrp_rb_file.c
xrp_SRCS += xrp_ns.c

INCLUDES = -I$(CURDIR)  -I../include  -I../../xrp-kernel
# object files will be generated from .c sourcefiles
xrp_OBJS   = $(notdir $(xrp_SRCS:.c=.o))

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

