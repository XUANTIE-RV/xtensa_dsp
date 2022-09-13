/*
 * XRP: Linux device driver for Xtensa Remote Processing
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
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

#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/acpi.h>
#include <linux/completion.h>
#include <linux/delay.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
#include <linux/dma-mapping.h>
#else
#include <linux/dma-direct.h>
#endif
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/highmem.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <asm/mman.h>
#include <linux/mman.h>
#include <asm/uaccess.h>
#include "xrp_cma_alloc.h"
#include "xrp_firmware.h"
#include "xrp_hw.h"
#include "xrp_internal.h"
#include "xrp_kernel_defs.h"
#include "xrp_kernel_dsp_interface.h"
#include "xrp_private_alloc.h"

#define DRIVER_NAME "xrp_hw_com"

struct xrp_hw_common_drvdata {

	phys_addr_t sys_reg_phys;
	void __iomem *sys_regs;
	size_t  sys_reg_size;
	struct mutex lock;

};
static struct xrp_hw_common_drvdata  *hw_drvdata = NULL;
#define XRP_REG_RESET		(0x28)
#define DSP0_RESET_BIT_MASK      (0x1<<8)

#define DSP1_RESET_BIT_MASK      (0x1<<12)
int xrp_set_reset_reg(int dsp_id)
{
	uint32_t bit_mask = dsp_id==0?DSP0_RESET_BIT_MASK
								:DSP1_RESET_BIT_MASK;

	pr_debug("%s,reset\n", __func__);
	if(!hw_drvdata)
	{
		pr_debug("%s hw_drvdata is NULL \n",__func__);
		return -1;
	}
	mutex_lock(&hw_drvdata->lock);

	uint32_t old_value = __raw_readl(hw_drvdata->sys_regs+XRP_REG_RESET);
	pr_debug("%s,reset reg:%x\n",__func__,old_value);
	__raw_writel(old_value^bit_mask,hw_drvdata->sys_regs+XRP_REG_RESET);
	udelay(10000);
	__raw_writel(old_value,hw_drvdata->sys_regs+XRP_REG_RESET);
	mutex_unlock(&hw_drvdata->lock);

	return 0;
}
EXPORT_SYMBOL(xrp_set_reset_reg);
static const struct of_device_id xrp_hw_common_of_match[] = {
	{
		.compatible = "thead,dsp-hw-common",
	}, {},
};
// MODULE_DEVICE_TABLE(of, xrp_hw_common_of_match);
static int xrp_hw_common_probe(struct platform_device *pdev)
{
	long ret = -EINVAL;

	struct resource *mem;

	hw_drvdata =
		devm_kzalloc(&pdev->dev, sizeof(*hw_drvdata), GFP_KERNEL);
	if (!hw_drvdata)
		return -ENOMEM;	

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		return -ENODEV;
	}
	hw_drvdata->sys_reg_phys = mem->start;
	hw_drvdata->sys_reg_size =mem->end-mem->start;
	hw_drvdata->sys_regs = devm_ioremap_resource(&pdev->dev, mem);
	pr_debug("%s,sys_reg_phys:%lx,sys_regs:%lx,size:%x\n", __func__,
				hw_drvdata->sys_reg_phys,hw_drvdata->sys_regs,hw_drvdata->sys_reg_size);
	mutex_init(&hw_drvdata->lock);

	return 0;
}

static int xrp_hw_common_remove(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver xrp_hw_common_driver = {
	.probe   = xrp_hw_common_probe,
	.remove  = xrp_hw_common_remove,
	.driver  = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr( xrp_hw_common_of_match),
	},
};

module_platform_driver(xrp_hw_common_driver);

MODULE_AUTHOR("T-HEAD");
MODULE_DESCRIPTION("XRP: Linux device driver for Xtensa Remote Processing");
MODULE_LICENSE("Dual MIT/GPL");
