TESTS := dsp_demo
TESTS_UT :=test_dsp_drv_ut
TESTS_M_THREAD :=test_dsp_thread
TESTS_MAX_PWR :=test_dsp_max_power
TESTS_X_TEST :=test_dsp_x_test

CFLAGS += -O0 -Wall -g -lm -lpthread
# LDFLAGS += -L../driver/xrp-user/xrp-host -lxrp_linux
# LDFLAGS += -L../driver/xrp-user/xrp-common -lxrp-common
LDFLAGS += -Ithread-pthread 
LDFLAGS += -L../../driver/xrp-user/ -ldsp
LDFLAGS += -L../test_utility/lib/ -lCppUTest -lCppUTestExt 
SRCS += dsp_demo.c

SRCS_UT += test_post_process.cpp
SRCS_UT += test_vi_enhance_process.cpp
SRCS_UT += test_post_process_lib_loader.cpp
SRCS_UT += test_post_process_dma_buf.cpp
# SRCS +=../driver/xrp-user/dsp-ps/dsp_ps_core.c
SRCS_THREAD +=test_dsp_thread.c

SRCS_MAX_PWR +=dsp_max_power.c
SRCS_X_TEST +=dsp_x_test.c

INCLUDES +=   -I../../driver/xrp-user/include
INCLUDES += -I../test_utility/include/
# object files will be generated from .c sourcefiles
OBJS  = $(notdir $(SRCS:.c=.o))
OBJS_UT =  $(notdir $(SRCS_UT:.cpp=.o))
OBJS_THREAD = $(notdir $(SRCS_THREAD:.c=.o))
OBJS_MAX_PWR= $(notdir $(SRCS_MAX_PWR:.c=.o))
OBJS_X_TEST= $(notdir $(SRCS_X_TEST:.c=.o))

all: $(TESTS) $(TESTS_UT)  $(TESTS_MAX_PWR) $(TESTS_M_THREAD) $(TESTS_X_TEST)

prepare:
	mkdir -p output

$(OBJS):$(SRCS)
	$(CC) -c $(CFLAGS) $(INCLUDES) $(SRCS)

$(OBJS_UT):$(SRCS_UT)
	$(CXX) -c $(CFLAGS) $(INCLUDES) $(SRCS_UT)

$(OBJS_THREAD):$(SRCS_THREAD)
	$(CC) -c $(CFLAGS) $(INCLUDES) $(SRCS_THREAD)

$(OBJS_MAX_PWR):$(SRCS_MAX_PWR)
	$(CC) -c $(CFLAGS) $(INCLUDES) $(SRCS_MAX_PWR)

$(OBJS_X_TEST):$(SRCS_X_TEST)
	$(CC) -c $(CFLAGS) $(INCLUDES) $(SRCS_X_TEST)


$(TESTS_UT):prepare $(OBJS_UT)
	$(CXX)  -o $(TESTS_UT) $(OBJS_UT) $(CFLAGS) $(LDFLAGS)
	cp -r $(TESTS_UT) ./output/

$(TESTS):prepare $(OBJS)
	$(CC)  -o $(TESTS) $(OBJS) $(CFLAGS) $(LDFLAGS)
	cp -r $(TESTS) ./output/

$(TESTS_M_THREAD):prepare $(OBJS_THREAD)
	$(CC)  -o $(TESTS_M_THREAD) $(OBJS_THREAD) $(CFLAGS) $(LDFLAGS)
	cp -r $(TESTS_M_THREAD) ./output/

$(TESTS_MAX_PWR):prepare $(OBJS_MAX_PWR)
	$(CC)  -o $(TESTS_MAX_PWR) $(OBJS_MAX_PWR) $(CFLAGS) $(LDFLAGS)
	cp -r $(TESTS_MAX_PWR) ./output/

$(TESTS_X_TEST):prepare $(OBJS_X_TEST)
	$(CC)  -o $(TESTS_X_TEST) $(OBJS_X_TEST) $(CFLAGS) $(LDFLAGS)
	cp -r $(TESTS_X_TEST) ./output/

clean:
	rm -f $(TESTS)
	rm -f *.o
	rm -rf output

install:

.PHONY: clean all prepare common

