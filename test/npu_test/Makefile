TESTS := test_dsp_npu

CFLAGS = -O0 -Wall -g -lm -lpthread -lrt
LDFLAGS += -L../../driver/xrp-user/ -ldsp
LDFLAGS += -Ithread-pthread 
SRCS += io.c
SRCS += test_bench_utils.c
SRCS += dsp_src_test.c

INCLUDES +=   -I../../driver/xrp-user/include
# object files will be generated from .c sourcefiles
OBJS  = $(notdir $(SRCS:.c=.o))


all: $(TESTS) npu_sink_test

prepare:
	mkdir -p output

$(OBJS):$(SRCS)
	$(CC) -c $(CFLAGS) $(INCLUDES) $(SRCS)

$(TESTS):prepare $(OBJS)
	$(CC)  -o $(TESTS) $(OBJS) $(CFLAGS) $(LDFLAGS)
	cp -r $(TESTS) ./output/

npu_sink_test: npu_sink_test.c io.c test_bench_utils.c
	$(CC)  $(CFLAGS) -o  $@ $^ $(LDFLAGS)
	cp -r npu_sink_test ./output/
clean:
	rm -f $(TESTS)
	rm -f *.o
	rm -rf output

install:

.PHONY: clean all prepare common

