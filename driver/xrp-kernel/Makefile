#
# Copyright (c) 2017 Cadence Design Systems Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Alternatively you can use and distribute this file under the terms of
# the GNU General Public License version 2 or later.
#
ifdef VISYS_SYM_PATH
KBUILD_EXTRA_SYMBOLS += $(VISYS_SYM_PATH)/bm_visys/Module.symvers
else
KBUILD_EXTRA_SYMBOLS += $(shell pwd)/../../../baremetal-drivers/driver/visys/Module.symvers
endif
# export KBUILD_EXTRA_SYMBOLS
EXTRA_CFLAGS += -DWITH_VISYS_KO


xrp-y += xvp_main.o xrp_address_map.o xrp_alloc.o xrp_debug.o
xrp-$(CONFIG_OF) += xrp_firmware.o
xrp-$(CONFIG_CMA) += xrp_cma_alloc.o

obj-$(CONFIG_XRP) += xrp.o
obj-$(CONFIG_XRP_HW_SIMPLE) += xrp_hw_simple.o
obj-m += xrp_hw_comm.o
obj-$(CONFIG_XRP_HW_HIKEY960) += xrp_hw_hikey960.o

ifeq ($(BUILD_TYPE),DEBUG)
	EXTRA_CFLAGS += -DDEBUG
endif
KBUILD_CFLAGS += -O2
ccflags-$(CONFIG_XRP_DEBUG) += -DDEBUG
ccflags-$(CONFIG_XRP_HW_HIKEY960) += -I$(srctree)/drivers/hisi/hifi_mailbox

# Remove this comment and all lines below it when integrating this Makefile
# into the linux kernel make system.
#
# sed -i % '/Remove this comment and all lines below it/,$d'
# make -C $(KERNEL) M=$(PWD) modules

# KSRC ?= /lib/modules/$(shell uname -r)/build

modules:
	$(MAKE) -C $(KERNEL) M=`pwd` CONFIG_XRP=m CONFIG_XRP_HW_SIMPLE=m CONFIG_XRP_DEBUG=y modules

%:
	$(MAKE) -C $(KERNEL) M=`pwd` CONFIG_XRP=m $@
