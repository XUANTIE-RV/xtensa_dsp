TESTS := test_dsp_ip

CFLAGS = -O0 -Wall -g -lm -lpthread
LDFLAGS += -L../../driver/xrp-user/ -ldsp
LDFLAGS += -Ithread-pthread 
SRCS += test_dsp_ip.c

INCLUDES +=   -I../../driver/xrp-user/include
# object files will be generated from .c sourcefiles
OBJS  = $(notdir $(SRCS:.c=.o))


all: $(TESTS)

prepare:
	mkdir -p output

$(OBJS):$(SRCS)
	$(CC) -c $(CFLAGS) $(INCLUDES) $(SRCS)

$(TESTS):prepare $(OBJS)
	$(CC)  -o $(TESTS) $(OBJS) $(CFLAGS) $(LDFLAGS)
	cp -r $(TESTS) ./output/

clean:
	rm -f $(TESTS)
	rm -f *.o
	rm -rf output

install:

.PHONY: clean all prepare common

