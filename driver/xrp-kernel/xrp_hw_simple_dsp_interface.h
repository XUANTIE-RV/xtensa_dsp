/*
 * @Author: your name
 * @Date: 2022-03-25 12:36:07
 * @LastEditTime: 2022-03-25 12:36:08
 * @LastEditors: your name
 * @Description: 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 * @FilePath: /workspace/thead-build/light-fm/tmp-glibc/work/riscv64-oe-linux/xtensa-dsp/1.0-r0/git/driver/xrp-kernel/xrp_hw_simple_dsp_interface.h
 */
/*
 * XRP interface between hardware-specific linux and DSP parts of example
 * hardware
 *
 * Copyright (c) 2017 Cadence Design Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Alternatively you can use and distribute this file under the terms of
 * the GNU General Public License version 2 or later.
 */

#ifndef _XRP_KERNEL_SIMPLE_HW_DSP_INTERFACE
#define _XRP_KERNEL_SIMPLE_HW_DSP_INTERFACE

#include "xrp_ring_buffer.h"
enum {
	XRP_DSP_SYNC_IRQ_MODE_NONE = 0x0,
	XRP_DSP_SYNC_IRQ_MODE_LEVEL = 0x1,
	XRP_DSP_SYNC_IRQ_MODE_EDGE = 0x2,
};

struct xrp_hw_simple_sync_data {
	__u32 device_mmio_base;
	__u32 host_irq_mode;
	__u32 host_irq_offset;
	__u32 host_irq_bit;
	__u32 device_irq_mode;
	__u32 device_irq_offset;
	__u32 device_irq_bit;
	__u32 device_irq;
};


struct xrp_hw_panic {
	__u32 panic;
	__u32 ccount;
	__u32 reserved[2];
	struct xrp_ring_buffer rb;
};
#endif
