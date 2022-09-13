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
#include <linux/timer.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
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
#include "xrp_debug.h"
#define DRIVER_NAME "xrp"
#define XRP_DEFAULT_TIMEOUT 60

#ifndef __io_virt
#define __io_virt(a) ((void __force *)(a))
#endif

struct xrp_alien_mapping {
	unsigned long vaddr;
	unsigned long size;
	phys_addr_t paddr;
	void *allocation;
	enum {
		ALIEN_GUP,
		ALIEN_PFN_MAP,
		ALIEN_COPY,
	} type;
};

struct xrp_mapping {
	enum {
		XRP_MAPPING_NONE,
		XRP_MAPPING_NATIVE,
		XRP_MAPPING_ALIEN,
		XRP_MAPPING_KERNEL = 0x4,
	} type;
	union {
		struct {
			struct xrp_allocation *xrp_allocation;
			unsigned long vaddr;
		} native;
		struct xrp_alien_mapping alien_mapping;
	};
};

struct xvp_file {
	struct xvp *xvp;
	spinlock_t busy_list_lock;
	struct xrp_allocation *busy_list;
};

struct xrp_known_file {
	void *filp;
	struct hlist_node node;
};

struct xrp_dma_buf_item{

	struct list_head  link;
    struct dma_buf *dmabuf;
    struct sg_table *sgt;
    struct dma_buf_attachment * attachment;
    int ref;
};
static int firmware_command_timeout = XRP_DEFAULT_TIMEOUT;
module_param(firmware_command_timeout, int, 0644);
MODULE_PARM_DESC(firmware_command_timeout, "Firmware command timeout in seconds.");

static int firmware_reboot = 1;
module_param(firmware_reboot, int, 0644);
MODULE_PARM_DESC(firmware_reboot, "Reboot firmware on command timeout.");

enum {
	LOOPBACK_NORMAL,	/* normal work mode */
	LOOPBACK_NOIO,		/* don't communicate with FW, but still load it and control DSP */
	LOOPBACK_NOMMIO,	/* don't comminicate with FW or use DSP MMIO, but still load the FW */
	LOOPBACK_NOFIRMWARE,	/*  communicate with FW or use DSP MMIO, don't load the FW */

	LOOPBACK_NOFIRMWARE_NOMMIO, /* don't communicate with FW or use DSP MMIO, don't load the FW */
};
static int loopback = 0;
module_param(loopback, int, 0644);
MODULE_PARM_DESC(loopback, "Don't use actual DSP, perform everything locally.");

static int load_mode = 0;
module_param(load_mode, int, 0644);
MODULE_PARM_DESC(load_mode, "firmware load mode. 0: load by driver. 1:load by xplorer to debug.");

enum {
    LOAD_MODE_AUTO,   /* load firmware auto by drvier */
    LOAD_MODE_MANUAL,   /* load firmware manually for debug*/
};

static int heartbeat_period = 0;
module_param(heartbeat_period, int, 0644);
MODULE_PARM_DESC(heartbeat_period, "Firmware command timeout in seconds.");

static int dsp_fw_log_mode = 1;
module_param(dsp_fw_log_mode, int, 0644);
MODULE_PARM_DESC(dsp_fw_log_mode, "Firmware LOG MODE.0:disable,1:ERROR(DEFAULT),2:WRNING,3:INFO,4:DEUBG,5:TRACE");
static DEFINE_HASHTABLE(xrp_known_files, 10);
static DEFINE_SPINLOCK(xrp_known_files_lock);

static DEFINE_SPINLOCK(xrp_dma_buf_lock);
static DEFINE_IDA(xvp_nodeid);

static int xrp_boot_firmware(struct xvp *xvp);

static long xrp_copy_user_from_phys(struct xvp *xvp,
				    unsigned long vaddr, unsigned long size,
				    phys_addr_t paddr, unsigned long flags);
static bool xrp_cacheable(struct xvp *xvp, unsigned long pfn,
			  unsigned long n_pages)
{
	if (xvp->hw_ops->cacheable) {
		return xvp->hw_ops->cacheable(xvp->hw_arg, pfn, n_pages);
	} else {
		unsigned long i;

		for (i = 0; i < n_pages; ++i)
			if (!pfn_valid(pfn + i))
				return false;
		return true;
	}
}

static int xrp_dma_direction(unsigned flags)
{
	static const enum dma_data_direction xrp_dma_direction[] = {
		[0] = DMA_NONE,
		[XRP_FLAG_READ] = DMA_TO_DEVICE,
		[XRP_FLAG_WRITE] = DMA_FROM_DEVICE,
		[XRP_FLAG_READ_WRITE] = DMA_BIDIRECTIONAL,
	};
	return xrp_dma_direction[flags & XRP_FLAG_READ_WRITE];
}

static void xrp_default_dma_sync_for_device(struct xvp *xvp,
					    phys_addr_t phys,
					    unsigned long size,
					    unsigned long flags)
{
	dma_sync_single_for_device(xvp->dev, phys_to_dma(xvp->dev, phys), size,
				   xrp_dma_direction(flags));
}

static void xrp_dma_sync_for_device(struct xvp *xvp,
				    unsigned long virt,
				    phys_addr_t phys,
				    unsigned long size,
				    unsigned long flags)
{
	if (xvp->hw_ops->dma_sync_for_device)
		xvp->hw_ops->dma_sync_for_device(xvp->hw_arg,
						 (void *)virt, phys, size,
						 flags);
	else
		xrp_default_dma_sync_for_device(xvp, phys, size, flags);
}

static void xrp_default_dma_sync_for_cpu(struct xvp *xvp,
					 phys_addr_t phys,
					 unsigned long size,
					 unsigned long flags)
{
	dma_sync_single_for_cpu(xvp->dev, phys_to_dma(xvp->dev, phys), size,
				xrp_dma_direction(flags));
}

static void xrp_dma_sync_for_cpu(struct xvp *xvp,
				 unsigned long virt,
				 phys_addr_t phys,
				 unsigned long size,
				 unsigned long flags)
{
	if (xvp->hw_ops->dma_sync_for_cpu)
		xvp->hw_ops->dma_sync_for_cpu(xvp->hw_arg,
					      (void *)virt, phys, size,
					      flags);
	else
		xrp_default_dma_sync_for_cpu(xvp, phys, size, flags);
}
static inline void xrp_comm_write32(volatile void __iomem *addr, u32 v)
{
	//__raw_writel(v, addr);
	writel(v, addr);
}

static inline u32 xrp_comm_read32(volatile void __iomem *addr)
{
	//return __raw_readl(addr);
	return readl(addr);
}

static inline void __iomem *xrp_comm_put_tlv(void __iomem **addr,
					     uint32_t type,
					     uint32_t length)
{
	struct xrp_dsp_tlv __iomem *tlv = *addr;

	xrp_comm_write32(&tlv->type, type);
	xrp_comm_write32(&tlv->length, length);
	*addr = tlv->value + ((length + 3) / 4);
	return tlv->value;
}

static inline void __iomem *xrp_comm_get_tlv(void __iomem **addr,
					     uint32_t *type,
					     uint32_t *length)
{
	struct xrp_dsp_tlv __iomem *tlv = *addr;

	*type = xrp_comm_read32(&tlv->type);
	*length = xrp_comm_read32(&tlv->length);
	*addr = tlv->value + ((*length + 3) / 4);
	return tlv->value;
}

static inline void xrp_comm_write(volatile void __iomem *addr, const void *p,
				  size_t sz)
{
	size_t sz32 = sz & ~3;
	u32 v;

	while (sz32) {
		memcpy(&v, p, sizeof(v));
		__raw_writel(v, addr);
		p += 4;
		addr += 4;
		sz32 -= 4;
	}
	sz &= 3;
	if (sz) {
		v = 0;
		memcpy(&v, p, sz);
		__raw_writel(v, addr);
	}
}

static inline void xrp_comm_read(volatile void __iomem *addr, void *p,
				  size_t sz)
{
	size_t sz32 = sz & ~3;
	u32 v;

	while (sz32) {
		v = __raw_readl(addr);
		memcpy(p, &v, sizeof(v));
		p += 4;
		addr += 4;
		sz32 -= 4;
	}
	sz &= 3;
	if (sz) {
		v = __raw_readl(addr);
		memcpy(p, &v, sz);
	}
}


static inline void xrp_send_device_irq(struct xvp *xvp)
{
	if (xvp->hw_ops->send_irq)
		xvp->hw_ops->send_irq(xvp->hw_arg);
}

static inline bool xrp_panic_check(struct xvp *xvp)
{
	if (xvp->hw_ops->panic_check)
		return xvp->hw_ops->panic_check(xvp->hw_arg);
	else
		return panic_check(xvp->panic_log);
}

static void xrp_add_known_file(struct file *filp)
{
	struct xrp_known_file *p = kmalloc(sizeof(*p), GFP_KERNEL);

	if (!p)
		return;

	p->filp = filp;
	spin_lock(&xrp_known_files_lock);
	hash_add(xrp_known_files, &p->node, (unsigned long)filp);
	spin_unlock(&xrp_known_files_lock);
}

static void xrp_remove_known_file(struct file *filp)
{
	struct xrp_known_file *p;
	struct xrp_known_file *pf = NULL;

	spin_lock(&xrp_known_files_lock);
	hash_for_each_possible(xrp_known_files, p, node, (unsigned long)filp) {
		if (p->filp == filp) {
			hash_del(&p->node);
			pf = p;
			break;
		}
	}
	spin_unlock(&xrp_known_files_lock);
	if (pf)
		kfree(pf);
}

static bool xrp_is_known_file(struct file *filp)
{
	bool ret = false;
	struct xrp_known_file *p;

	spin_lock(&xrp_known_files_lock);
	hash_for_each_possible(xrp_known_files, p, node, (unsigned long)filp) {
		if (p->filp == filp) {
			ret = true;
			break;
		}
	}
	spin_unlock(&xrp_known_files_lock);
	return ret;
}

static void xrp_sync_v2(struct xvp *xvp,
			void *hw_sync_data, size_t sz)
{
	struct xrp_dsp_sync_v2 __iomem *shared_sync = xvp->comm;
	void __iomem *addr = shared_sync->hw_sync_data;
    
	xrp_comm_write(xrp_comm_put_tlv(&addr,
					XRP_DSP_SYNC_TYPE_HW_SPEC_DATA, sz),
		       hw_sync_data, sz);
	if (xvp->n_queues > 1) {
		struct xrp_dsp_sync_v2 __iomem *queue_sync;
		unsigned i;

		xrp_comm_write(xrp_comm_put_tlv(&addr,
						XRP_DSP_SYNC_TYPE_HW_QUEUES,
						xvp->n_queues * sizeof(u32)),
			       xvp->queue_priority,
			       xvp->n_queues * sizeof(u32));
		for (i = 1; i < xvp->n_queues; ++i) {
			queue_sync = xvp->queue[i].comm;
			xrp_comm_write32(&queue_sync->sync,
					 XRP_DSP_SYNC_IDLE);
		}
	}
    struct xrp_dsp_debug_info debug_info ={
        .panic_addr = xvp->panic_phy,
        .log_level = dsp_fw_log_mode,
    };

    xrp_comm_write(xrp_comm_put_tlv(&addr,
					XRP_DSP_SYNC_TYPE_HW_DEBUG_INFO, sizeof(struct xrp_dsp_debug_info)),
		       &debug_info, sizeof(struct xrp_dsp_debug_info));
	xrp_comm_put_tlv(&addr, XRP_DSP_SYNC_TYPE_LAST, 0);
}

static int xrp_sync_complete_v2(struct xvp *xvp, size_t sz)
{
	struct xrp_dsp_sync_v2 __iomem *shared_sync = xvp->comm;
	void __iomem *addr = shared_sync->hw_sync_data;
	u32 type, len;

	xrp_comm_get_tlv(&addr, &type, &len);
	if (len != sz) {
		dev_err(xvp->dev,
			"HW spec data size modified by the DSP\n");
		return -EINVAL;
	}
	if (!(type & XRP_DSP_SYNC_TYPE_ACCEPT))
		dev_info(xvp->dev,
			 "HW spec data not recognized by the DSP\n");

	if (xvp->n_queues > 1) {
		void __iomem *p = xrp_comm_get_tlv(&addr, &type, &len);

		if (len != xvp->n_queues * sizeof(u32)) {
			dev_err(xvp->dev,
				"Queue priority size modified by the DSP\n");
			return -EINVAL;
		}
		if (type & XRP_DSP_SYNC_TYPE_ACCEPT) {
			xrp_comm_read(p, xvp->queue_priority,
				      xvp->n_queues * sizeof(u32));
		} else {
			dev_info(xvp->dev,
				 "Queue priority data not recognized by the DSP\n");
			xvp->n_queues = 1;
		}
	}
	return 0;
}

static int xrp_synchronize(struct xvp *xvp)
{
	size_t sz;
	void *hw_sync_data;
	unsigned long deadline = jiffies + firmware_command_timeout * HZ;
	struct xrp_dsp_sync_v1 __iomem *shared_sync = xvp->comm;
	int ret;
	u32 v, v1;

	hw_sync_data = xvp->hw_ops->get_hw_sync_data(xvp->hw_arg, &sz);
	if (!hw_sync_data) {
		ret = -ENOMEM;
		goto err;
	}
	ret = -ENODEV;
	dev_dbg(xvp->dev,"%s:comm sync:%p\n",__func__,&shared_sync->sync);
	xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_START);
	mb();
	do {
		v = xrp_comm_read32(&shared_sync->sync);
		if (v != XRP_DSP_SYNC_START)
			break;
		if (xrp_panic_check(xvp))
			goto err;
		schedule();
	} while (time_before(jiffies, deadline));
    dev_dbg(xvp->dev,"%s:comm sync data :%x\n",__func__,v);
	switch (v) {
	case XRP_DSP_SYNC_DSP_READY_V1:
		if (xvp->n_queues > 1) {
			dev_info(xvp->dev,
				 "Queue priority data not recognized by the DSP\n");
			xvp->n_queues = 1;
		}
		xrp_comm_write(&shared_sync->hw_sync_data, hw_sync_data, sz);
		break;
	case XRP_DSP_SYNC_DSP_READY_V2:
		xrp_sync_v2(xvp, hw_sync_data, sz);
		break;
	case XRP_DSP_SYNC_START:
		dev_err(xvp->dev, "DSP is not ready for synchronization\n");
		goto err;
	default:
		dev_err(xvp->dev,
			"DSP response to XRP_DSP_SYNC_START is not recognized\n");
		goto err;
	}

	mb();
	xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_HOST_TO_DSP);

	do {
		mb();
		v1 = xrp_comm_read32(&shared_sync->sync);
		if (v1 == XRP_DSP_SYNC_DSP_TO_HOST)
			break;
		if (xrp_panic_check(xvp))
			goto err;
		schedule();
	} while (time_before(jiffies, deadline));

	if (v1 != XRP_DSP_SYNC_DSP_TO_HOST) {
		dev_err(xvp->dev,
			"DSP haven't confirmed initialization data reception\n");
		goto err;
	}

	if (v == XRP_DSP_SYNC_DSP_READY_V2) {
		ret = xrp_sync_complete_v2(xvp, sz);
		if (ret < 0)
			goto err;
	}

	xrp_send_device_irq(xvp);

	// if (xvp->host_irq_mode) {
	// 	int res = wait_for_completion_timeout(&xvp->queue[0].completion,
	// 					      firmware_command_timeout * HZ);

	// 	ret = -ENODEV;
	// 	if (xrp_panic_check(xvp))
	// 		goto err;
	// 	if (res == 0) {
	// 		dev_err(xvp->dev,
	// 			"host IRQ mode is requested, but DSP couldn't deliver IRQ during synchronization\n");
	// 		goto err;
	// 	}
	// }
	ret = 0;
err:
	kfree(hw_sync_data);
	xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_IDLE);
	return ret;
}

static bool xrp_cmd_complete(struct xrp_comm *xvp)
{
	struct xrp_dsp_cmd __iomem *cmd = xvp->comm;
	u32 flags = xrp_comm_read32(&cmd->flags);
	pr_debug(" xrp_cmd_complete %x\n", flags);
	rmb();
	return (flags & (XRP_DSP_CMD_FLAG_REQUEST_VALID |
			 XRP_DSP_CMD_FLAG_RESPONSE_VALID)) ==
		(XRP_DSP_CMD_FLAG_REQUEST_VALID |
		 XRP_DSP_CMD_FLAG_RESPONSE_VALID);
}

static inline int xrp_report_comlete(struct xvp *xvp)
{
	struct xrp_dsp_cmd __iomem *cmd = xvp->comm;

	if(!xvp->reporter)
		return -1;

	u32 flags = xrp_comm_read32(&cmd->report_id);

	if(flags& XRP_DSP_REPORT_TO_HOST_FLAG )
	{
        // dev_err(xvp->dev, "%s,report_flag %x\n", __func__,flags);
	    flags &= (~XRP_DSP_REPORT_TO_HOST_FLAG);

        xrp_comm_write32(&cmd->report_id,flags);
		tasklet_schedule(&xvp->reporter->report_task);
		return 0;
	}
	return -1;
}


static inline int xrp_device_cmd_comlete(struct xvp *xvp)
{
	struct xrp_dsp_cmd __iomem *cmd = xvp->comm;

	u32 flags = xrp_comm_read32(&cmd->cmd_flag);

	if(flags& XRP_DSP_REPORT_TO_HOST_FLAG )
	{
        xrp_comm_write32(&cmd->cmd_flag,0);
		return 0;
	}
	return -1;
}

irqreturn_t xrp_irq_handler(int irq, struct xvp *xvp)
{
	unsigned i, n = 0;

	// dev_dbg(xvp->dev, "%s\n", __func__);
	if (!xvp->comm)
		return IRQ_NONE;

	if(!xrp_report_comlete(xvp))
	{
		dev_dbg(xvp->dev, "completing report\n");
		// return IRQ_HANDLED;
	}
    if(xrp_device_cmd_comlete(xvp))
    {
        dev_dbg(xvp->dev, "no cmd msg report\n");
        return IRQ_HANDLED;
    }
	for (i = 0; i < xvp->n_queues; ++i) {
		if (xrp_cmd_complete(xvp->queue + i)) {
			dev_dbg(xvp->dev, "completing queue %d\n", i);
			complete(&xvp->queue[i].completion);
			++n;
		}
	}

	return n ? IRQ_HANDLED : IRQ_NONE;
}
EXPORT_SYMBOL(xrp_irq_handler);

static inline void xvp_file_lock(struct xvp_file *xvp_file)
{
	spin_lock(&xvp_file->busy_list_lock);
}

static inline void xvp_file_unlock(struct xvp_file *xvp_file)
{
	spin_unlock(&xvp_file->busy_list_lock);
}

static void xrp_allocation_queue(struct xvp_file *xvp_file,
				 struct xrp_allocation *xrp_allocation)
{
	xvp_file_lock(xvp_file);

	xrp_allocation->next = xvp_file->busy_list;
	xvp_file->busy_list = xrp_allocation;

	xvp_file_unlock(xvp_file);
}

static struct xrp_allocation *xrp_allocation_dequeue(struct xvp_file *xvp_file,
						     phys_addr_t paddr, u32 size)
{
	struct xrp_allocation **pcur;
	struct xrp_allocation *cur;

	xvp_file_lock(xvp_file);

	for (pcur = &xvp_file->busy_list; (cur = *pcur); pcur = &((*pcur)->next)) {
		pr_debug("%s: %pap / %pap x %d\n", __func__, &paddr, &cur->start, cur->size);
		if (paddr >= cur->start && paddr + size - cur->start <= cur->size) {
			*pcur = cur->next;
			break;
		}
	}

	xvp_file_unlock(xvp_file);
	return cur;
}

static long xrp_ioctl_alloc(struct file *filp,
			    struct xrp_ioctl_alloc __user *p)
{
	struct xvp_file *xvp_file = filp->private_data;
	struct xrp_allocation *xrp_allocation;
	unsigned long vaddr;
	struct xrp_ioctl_alloc xrp_ioctl_alloc;
	long err;

	// pr_debug("%s: %p\n", __func__, p);
	if (copy_from_user(&xrp_ioctl_alloc, p, sizeof(*p)))
		return -EFAULT;

	// pr_debug("%s: size = %d, align = %x\n", __func__,
	// 	 xrp_ioctl_alloc.size, xrp_ioctl_alloc.align);

	err = xrp_allocate(xvp_file->xvp->pool,
			   xrp_ioctl_alloc.size,
			   xrp_ioctl_alloc.align,
			   &xrp_allocation);
	if (err)
		return err;

	xrp_allocation_queue(xvp_file, xrp_allocation);

	vaddr = vm_mmap(filp, 0, xrp_allocation->size,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			xrp_allocation_offset(xrp_allocation));

	xrp_ioctl_alloc.addr = vaddr;
    xrp_ioctl_alloc.paddr = xrp_allocation->start;
    pr_debug("%s: vaddr = %llx, paddr = %llx\n", __func__,
		 xrp_ioctl_alloc.addr, xrp_ioctl_alloc.paddr);
	if (copy_to_user(p, &xrp_ioctl_alloc, sizeof(*p))) {
		vm_munmap(vaddr, xrp_ioctl_alloc.size);
		return -EFAULT;
	}
	return 0;
}
static void xrp_report_tasklet(unsigned long arg)
{
	struct xvp *xvp=(struct xvp *)arg;
		struct xrp_dsp_cmd __iomem *cmd=xvp->comm;
	struct xrp_report_buffer *p_buf = xvp->reporter->buffer_virt;
	// pr_debug("%s,addr:%lx\n",__func__,arg);
	if(!xvp->reporter->fasync)
	{
		pr_debug("%s:fasync is not register in user space\n",__func__);
		return;
	}
	// pr_debug("%s,%d\n",__func__,xvp->reporter->fasync->magic);
	// if(!xvp->reporter->user_buffer_virt &&
	// 	!xvp->reporter->buffer_size)
	// {
	// 	pr_debug("%s:user_buffer_virt and buffer size is invalid\n",__func__);
	// 	return;
	// }
	// size_t s= xrp_comm_read32(&cmd->report_paylad_size);

	// unsigned int id = xrp_comm_read32(&cmd->report_id);
	// if(copy_to_user(&p_buf_user->report_id,&id,sizeof(p_buf_user->report_id)));
	// {
	// 	pr_debug("%s:copy report id to user fail\n",__func__);
	// 	return;
	// }

    // if(xvp->reporter->buffer_size>XRP_DSP_CMD_INLINE_DATA_SIZE)
	// {
	// 	if(xrp_copy_user_from_phys(xvp,&p_buf_user->data[0],s,xvp->reporter->buffer_phys,XRP_FLAG_READ_WRITE))
	// 		return;			 
	// }
	// else
	// {
	// 	char temp_buf[XRP_DSP_CMD_INLINE_DATA_SIZE];
	// 	xrp_comm_read(&cmd->report_data,temp_buf,s);
	// 	if(copy_to_user(&p_buf_user->data[0],temp_buf,s))
	// 	{
	// 		pr_debug("%s:copy report data to user fail\n",__func__);
	// 		return;
	// 	}
	// }
	/*****clear report*********************/
	p_buf->report_id = xrp_comm_read32(&cmd->report_id)&0xffff;

	//xrp_dma_sync_for_cpu(xvp,xvp->reporter->buffer_virt,xvp->reporter->buffer_phys,xvp->reporter->buffer_size,XRP_FLAG_WRITE);	
	kill_fasync(&(xvp->reporter->fasync), SIGIO, POLL_IN);
	xrp_comm_write32(&cmd->report_id,0x0);
    // pr_debug("%s,report_id:%d,report_data:%x\n",__func__,p_buf->report_id,p_buf->data[0]);
}
static long xrp_map_phy_to_virt(phys_addr_t paddr,unsigned long size,__u64 *vaddr)
{
		// if (pfn_valid(__phys_to_pfn(paddr))) {
		// 	struct page *page = pfn_to_page(__phys_to_pfn(paddr));
		// 	size_t page_offs = paddr & ~PAGE_MASK;
		// 	size_t offs;

		// 	// for (offs = 0; offs < size; ++page) {
		// 	// 	void *p = kmap(page);
		// 	// 	size_t sz = PAGE_SIZE - page_offs;
		// 	// 	size_t copy_sz = sz;
		// 	// 	unsigned long rc;
		// 	// }
		// 	if(page_offs+size>PAGE_SIZE)
		// 	{
		// 		pr_debug("%s,phys addr map to virt exceed one page",__func__);
		// 		return -EINVAL;
		// 	}
		// 	void *p  = kmap(page);
		// 	if(!p)
		// 	{
		// 		pr_debug("%s couldn't kmap %pap x 0x%08x\n",__func__,&paddr, (u32)size);
		// 		return -EINVAL;
		// 	}
		// 	*vaddr =p + page_offs;
		// 	pr_debug("%s map to mem",__func__);
		// 	return 0;

		// }
		// else
        {
				void __iomem *p = ioremap(paddr, size);
				unsigned long rc;

				if (!p) {
					pr_debug("%s,couldn't ioremap %pap x 0x%08x\n",__func__,&paddr, (u32)size);
					return -EINVAL;
				}
				*vaddr = p;
				pr_debug("%s map to io mem",__func__);
				return 0;
		}
				// 	iounmap(p);
				// if (rc)
				// 	return -EFAULT;

				// }
}

static long xrp_unmap_phy_to_virt(unsigned long *vaddr,phys_addr_t paddr,unsigned long size)
{
		if (pfn_valid(__phys_to_pfn(paddr))) {
			struct page *page = pfn_to_page(__phys_to_pfn(paddr));
			kunmap(page);
		}
		else{
			iounmap(*vaddr);
		}
		*vaddr=NULL;
		return 0;
}
static long xrp_ioctl_alloc_report(struct file *filp,
			    struct xrp_ioctl_alloc __user *p)
{
		struct xvp_file *xvp_file = filp->private_data;
		struct xrp_allocation *xrp_allocation;
		struct xvp *xvp = xvp_file->xvp;
		struct xrp_ioctl_alloc xrp_ioctl_alloc;
		struct xrp_dsp_cmd __iomem *cmd=xvp->comm;
		unsigned long vaddr;
		long err;

		pr_debug("%s: %p\n", __func__, p);
		if (copy_from_user(&xrp_ioctl_alloc, p, sizeof(*p)))
			return -EFAULT;
		pr_debug("%s: virtAddr = %lx.size = %d, align = %x\n", __func__,
		 		xrp_ioctl_alloc.addr,xrp_ioctl_alloc.size, 
				xrp_ioctl_alloc.align);
		// if(NULL == xrp_ioctl_alloc.addr)
		// {
		// 	return -EFAULT;
		// }
		xvp->reporter= kmalloc(sizeof(*(xvp->reporter)), GFP_KERNEL);
		if (!xvp->reporter)
			return -EFAULT;
		xvp->reporter->fasync=NULL;
		err = xrp_allocate(xvp_file->xvp->pool,
			xrp_ioctl_alloc.size,
			xrp_ioctl_alloc.align,
			&xrp_allocation);

		if (err)
			return err;
		xrp_allocation_queue(xvp_file, xrp_allocation);

		vaddr = vm_mmap(filp, 0, xrp_allocation->size,
							PROT_READ | PROT_WRITE, MAP_SHARED,
							xrp_allocation_offset(xrp_allocation));
		xrp_ioctl_alloc.addr=vaddr;		
		xvp->reporter->buffer_phys = xrp_allocation->start;

		if(xrp_map_phy_to_virt(xvp->reporter->buffer_phys,sizeof(__u32),&xvp->reporter->buffer_virt))
		{
			pr_debug("%s: map to kernel virt fail\n", __func__);
			kfree(xvp->reporter);
			return -EFAULT;
		}

		xrp_comm_write32(&cmd->report_addr, 
					xrp_translate_to_dsp(&xvp->address_map,xvp->reporter->buffer_phys+sizeof(__u32)));
		unsigned int dsp_addr = xrp_comm_read32(&cmd->report_addr);		
		pr_debug("%s: alloc_report buffer user virt:%llx,kernel virt:%lx, phys:%llx,dsp_addr:%x,size:%d\n", __func__,
					vaddr,xvp->reporter->buffer_virt,xvp->reporter->buffer_phys,dsp_addr,xrp_allocation->size);
		/*alloc report memory for DSP , alloc kernel memory for user get*/
		// if(xrp_ioctl_alloc.size>XRP_DSP_CMD_INLINE_DATA_SIZE)
		// {

		// 	err = xrp_allocate(xvp_file->xvp->pool,
		// 			xrp_ioctl_alloc.size,
		// 			xrp_ioctl_alloc.align,
		// 			&xrp_allocation);
		// 	if (err)
		// 		return err;

		// 	// xrp_allocation_queue(xvp_file, xrp_allocation);
		// 	xvp->reporter->buffer_phys = xrp_allocation->start;
		// 	xrp_comm_write32(&cmd->report_addr, 
		// 				xrp_translate_to_dsp(&xvp->address_map,xvp->reporter->buffer_phys));
		// 	// vaddr = vm_mmap(filp, 0, xrp_allocation->size,
		// 	// PROT_READ | PROT_WRITE, MAP_SHARED,
		// 	// xrp_allocation_offset(xrp_allocation));
		// 	// xrp_ioctl_alloc.addr=vaddr;
		// 	pr_debug("%s: kernel bufdfer:%lx\n", __func__, xvp->reporter->buffer_phys);
		// }
		// else{
		// 	xvp->reporter->buffer_phys = NULL;
		// }
        /*save the user addr ,which kernel copy the report to */
		// xvp->reporter->user_buffer_virt = xrp_ioctl_alloc.addr;	
		xvp->reporter->buffer_size = xrp_ioctl_alloc.size;
		xrp_comm_write32(&cmd->report_buffer_size,xvp->reporter->buffer_size);
		xrp_comm_write32(&cmd->report_status,XRP_DSP_REPORT_WORKING);
		xrp_comm_write32(&cmd->report_id,0);
		tasklet_init(&xvp->reporter->report_task,xrp_report_tasklet,(unsigned long)xvp);
		if (copy_to_user(p, &xrp_ioctl_alloc, sizeof(*p))) {
			vm_munmap(vaddr, xrp_ioctl_alloc.size);
			kfree(xvp->reporter);
			pr_debug("%s: copy to user fail\n", __func__);
			return -EFAULT;
		}
		pr_debug("%s: alloc_report %lx end\n", __func__,xvp);
	return 0;
}

static int xrp_report_fasync(int fd, struct file *filp, int on){
   struct xvp_file *xvp_file  = (struct xvp_file *)filp->private_data;
   
   pr_debug("%s: start,mode: %d\n", __func__,on);
   if(xvp_file->xvp->reporter == NULL)
   {
        pr_debug("%s: reporter is NULL\n", __func__,on);
        return 0;
   }
   if( fasync_helper(fd,filp,on,&(xvp_file->xvp->reporter->fasync)) < 0){
       pr_debug("%s: xrp_report_fasync fail\n", __func__);
       return -EIO;
   }
   pr_debug("%s: end\n", __func__);
   return 0;
}

static int xrp_report_fasync_release(struct file *filp){
	struct xvp_file *xvp_file  = (struct xvp_file *)filp->private_data;
	if(xvp_file->xvp->reporter)
		return  xrp_report_fasync(-1,filp,0); 
	return 0;

}
static long xrp_ioctl_release_report(struct file *filp,
						struct xrp_ioctl_alloc __user *p)
{
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
	struct mm_struct *mm = current->mm;
	struct xrp_ioctl_alloc xrp_ioctl_alloc;
	struct vm_area_struct *vma;
	unsigned long start;
	struct xrp_dsp_cmd __iomem *cmd=xvp->comm;

	tasklet_kill(&xvp->reporter->report_task);
	xrp_comm_write32(&cmd->report_status,XRP_DSP_REPORT_INVALID);

	if (copy_from_user(&xrp_ioctl_alloc, p, sizeof(*p)))
	return -EFAULT;

	start = xrp_ioctl_alloc.addr;
	pr_debug("%s: virt_addr = 0x%08lx\n", __func__, start);

    #if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	down_read(&mm->mmap_sem);
	#else
	down_read(&mm->mmap_lock);
	#endif
	vma = find_vma(mm, start);

	if (vma && vma->vm_file == filp &&
		vma->vm_start <= start && start < vma->vm_end) {
		size_t size;

		start = vma->vm_start;
		size = vma->vm_end - vma->vm_start;
        #if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
		up_read(&mm->mmap_sem);
		#else
		up_read(&mm->mmap_lock);		
		#endif
		pr_debug("%s: 0x%lx x %zu\n", __func__, start, size);
		vm_munmap(start, size);
	}
	else{
		pr_debug("%s: no vma/bad vma for vaddr = 0x%08lx\n", __func__, start);
        #if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
		up_read(&mm->mmap_sem);
		#else
		up_read(&mm->mmap_lock);		
		#endif
		return -EINVAL;
	}

	xrp_report_fasync_release(filp);	
	kfree(xvp->reporter);
	xvp->reporter =NULL;
	

	return 0;
}
// static struct struct_list timer;

// static void xrp_device_heartbeat_check(unsigned long arg)
// {
//     struct xvp *xvp = struct xvp *(arg);
//     if(xvp->reporter != NULL)
//     {
//        	xrp_comm_write32(&cmd->flags, 0);
//     }

//     mod_timer(&timer,jiffies + heartbeat_period * HZ);
// }
// static int xrp_device_heartbeat_init(void * arg)
// {
//     if(heartbeat_period > 0)
//     {
//         init_timer(&timer);
//         timer.function = xrp_device_heartbeat_check;
//         timer.expires = jiffies + heartbeat_period * HZ;
//         timer.data = arg;
//         add_timer(&timer);
//         pr_debug("%s enable heartbeat timer\n", __func__);
//     }

// }
static void xrp_put_pages(phys_addr_t phys, unsigned long n_pages)
{
	struct page *page;
	unsigned long i;

	page = pfn_to_page(__phys_to_pfn(phys));
	for (i = 0; i < n_pages; ++i)
		put_page(page + i);
}

static void xrp_alien_mapping_destroy(struct xrp_alien_mapping *alien_mapping)
{
	switch (alien_mapping->type) {
	case ALIEN_GUP:
		xrp_put_pages(alien_mapping->paddr,
			      PFN_UP(alien_mapping->vaddr +
				     alien_mapping->size) -
			      PFN_DOWN(alien_mapping->vaddr));
		break;
	case ALIEN_COPY:
		xrp_allocation_put(alien_mapping->allocation);
		break;
	default:
		break;
	}
}

static long xvp_pfn_virt_to_phys(struct xvp_file *xvp_file,
				 struct vm_area_struct *vma,
				 unsigned long vaddr, unsigned long size,
				 phys_addr_t *paddr,
				 struct xrp_alien_mapping *mapping)
{
	int ret;
	unsigned long i;
	unsigned long nr_pages = PFN_UP(vaddr + size) - PFN_DOWN(vaddr);
	unsigned long pfn;
	const struct xrp_address_map_entry *address_map;

	ret = follow_pfn(vma, vaddr, &pfn);
	if (ret)
		return ret;

	*paddr = __pfn_to_phys(pfn) + (vaddr & ~PAGE_MASK);
	address_map = xrp_get_address_mapping(&xvp_file->xvp->address_map,
					      *paddr);
	if (!address_map) {
		pr_debug("%s: untranslatable addr: %pap\n", __func__, paddr);
		return -EINVAL;
	}

	for (i = 1; i < nr_pages; ++i) {
		unsigned long next_pfn;
		phys_addr_t next_phys;

		ret = follow_pfn(vma, vaddr + (i << PAGE_SHIFT), &next_pfn);
		if (ret)
			return ret;
		if (next_pfn != pfn + 1) {
			pr_debug("%s: non-contiguous physical memory\n",
				 __func__);
			return -EINVAL;
		}
		next_phys = __pfn_to_phys(next_pfn);
		if (xrp_compare_address(next_phys, address_map)) {
			pr_debug("%s: untranslatable addr: %pap\n",
				 __func__, &next_phys);
			return -EINVAL;
		}
		pfn = next_pfn;
	}
	*mapping = (struct xrp_alien_mapping){
		.vaddr = vaddr,
		.size = size,
		.paddr = *paddr,
		.type = ALIEN_PFN_MAP,
	};
	pr_debug("%s: success, paddr: %pap\n", __func__, paddr);
	return 0;
}

static long xvp_gup_virt_to_phys(struct xvp_file *xvp_file,
				 unsigned long vaddr, unsigned long size,
				 phys_addr_t *paddr,
				 struct xrp_alien_mapping *mapping)
{
	int ret;
	int i;
	int nr_pages;
	struct page **page;
	const struct xrp_address_map_entry *address_map;

	if (PFN_UP(vaddr + size) - PFN_DOWN(vaddr) > INT_MAX)
		return -EINVAL;

	nr_pages = PFN_UP(vaddr + size) - PFN_DOWN(vaddr);
	page = kmalloc(nr_pages * sizeof(void *), GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	ret = get_user_pages_fast(vaddr, nr_pages, 1, page);
	if (ret < 0)
		goto out;

	if (ret < nr_pages) {
		pr_debug("%s: asked for %d pages, but got only %d\n",
			 __func__, nr_pages, ret);
		nr_pages = ret;
		ret = -EINVAL;
		goto out_put;
	}

	address_map = xrp_get_address_mapping(&xvp_file->xvp->address_map,
					      page_to_phys(page[0]));
	if (!address_map) {
		phys_addr_t addr = page_to_phys(page[0]);
		pr_debug("%s: untranslatable addr: %pap\n",
			 __func__, &addr);
		ret = -EINVAL;
		goto out_put;
	}

	for (i = 1; i < nr_pages; ++i) {
		phys_addr_t addr;

		if (page[i] != page[i - 1] + 1) {
			pr_debug("%s: non-contiguous physical memory\n",
				 __func__);
			ret = -EINVAL;
			goto out_put;
		}
		addr = page_to_phys(page[i]);
		if (xrp_compare_address(addr, address_map)) {
			pr_debug("%s: untranslatable addr: %pap\n",
				 __func__, &addr);
			ret = -EINVAL;
			goto out_put;
		}
	}

	*paddr = __pfn_to_phys(page_to_pfn(page[0])) + (vaddr & ~PAGE_MASK);
	*mapping = (struct xrp_alien_mapping){
		.vaddr = vaddr,
		.size = size,
		.paddr = *paddr,
		.type = ALIEN_GUP,
	};
	ret = 0;
	pr_debug("%s: success, paddr: %pap\n", __func__, paddr);

out_put:
	if (ret < 0)
		for (i = 0; i < nr_pages; ++i)
			put_page(page[i]);
out:
	kfree(page);
	return ret;
}

static long _xrp_copy_user_phys(struct xvp *xvp,
				unsigned long vaddr, unsigned long size,
				phys_addr_t paddr, unsigned long flags,
				bool to_phys)
{
	// if (pfn_valid(__phys_to_pfn(paddr))) {
	// 	struct page *page = pfn_to_page(__phys_to_pfn(paddr));
	// 	size_t page_offs = paddr & ~PAGE_MASK;
	// 	size_t offs;

	// 	if (!to_phys)
	// 		xrp_default_dma_sync_for_cpu(xvp, paddr, size, flags);
	// 	for (offs = 0; offs < size; ++page) {
	// 		void *p = kmap(page);
	// 		size_t sz = PAGE_SIZE - page_offs;
	// 		size_t copy_sz = sz;
	// 		unsigned long rc;

	// 		if (!p)
	// 			return -ENOMEM;

	// 		if (size - offs < copy_sz)
	// 			copy_sz = size - offs;

	// 		if (to_phys)
	// 			rc = copy_from_user(p + page_offs,
	// 					    (void __user *)(vaddr + offs),
	// 					    copy_sz);
	// 		else
	// 			rc = copy_to_user((void __user *)(vaddr + offs),
	// 					  p + page_offs, copy_sz);
    //         pr_debug("%s rc:%d,user addr :(%llx,%d) kernel:addr(%llx,%d) size:%d\n", __func__,rc,vaddr,offs,p,page_offs,copy_sz);
	// 		page_offs = 0;
	// 		offs += copy_sz;

	// 		kunmap(page);
	// 		if (rc)
	// 			return -EFAULT;
	// 	}
	// 	if (to_phys)
	// 		xrp_default_dma_sync_for_device(xvp, paddr, size, flags);
	// } else
     {
		void __iomem *p = ioremap(paddr, size);
		unsigned long rc;
        pr_debug("%s ioremap:to_phys %d-(%llx,%llx)\n", __func__,to_phys,paddr,p);
		if (!p) {
			dev_err(xvp->dev,
				"couldn't ioremap %pap x 0x%08x\n",
				&paddr, (u32)size);
			return -EINVAL;
		}
		if (to_phys)
        {
			rc = copy_from_user(__io_virt(p),
					    (void __user *)vaddr, size);
            /*fix  5.10 kernel  copy from vaddr in kernel to phy*/
            if(rc)
            {
                xrp_comm_write(p,(void *)vaddr,size);
                pr_debug("%s WR replease by copy to phy\n", __func__);
                rc =0 ;
            }
        }
		else
			rc = copy_to_user((void __user *)vaddr,
					  __io_virt(p), size);
        pr_debug("%s rc:%d,user addr :(%llx) kernel:addr(%llx) size:%d\n", __func__,rc,vaddr,p,size);
		iounmap(p);
		if (rc)
			return -EFAULT;
	}
	return 0;
}

static long xrp_copy_user_to_phys(struct xvp *xvp,
				  unsigned long vaddr, unsigned long size,
				  phys_addr_t paddr, unsigned long flags)
{
	return _xrp_copy_user_phys(xvp, vaddr, size, paddr, flags, true);
}

static long xrp_copy_user_from_phys(struct xvp *xvp,
				    unsigned long vaddr, unsigned long size,
				    phys_addr_t paddr, unsigned long flags)
{
	return _xrp_copy_user_phys(xvp, vaddr, size, paddr, flags, false);
}

static long xvp_copy_virt_to_phys(struct xvp_file *xvp_file,
				  unsigned long flags,
				  unsigned long vaddr, unsigned long size,
				  phys_addr_t *paddr,
				  struct xrp_alien_mapping *mapping)
{
	phys_addr_t phys;
	unsigned long align = clamp(vaddr & -vaddr, 16ul, PAGE_SIZE);
	unsigned long offset = vaddr & (align - 1);
	struct xrp_allocation *allocation;
	long rc;

	rc = xrp_allocate(xvp_file->xvp->pool,
			  size + align, align, &allocation);
	if (rc < 0)
		return rc;

	phys = (allocation->start & -align) | offset;
	if (phys < allocation->start)
		phys += align;

	if (flags & XRP_FLAG_READ) {
		if (xrp_copy_user_to_phys(xvp_file->xvp,
					  vaddr, size, phys, flags)) {
			xrp_allocation_put(allocation);
			return -EFAULT;
		}
	}

	*paddr = phys;
	*mapping = (struct xrp_alien_mapping){
		.vaddr = vaddr,
		.size = size,
		.paddr = *paddr,
		.allocation = allocation,
		.type = ALIEN_COPY,
	};
	pr_debug("%s: copying to pa: %pap\n", __func__, paddr);

	return 0;
}

static unsigned xvp_get_region_vma_count(unsigned long virt,
					 unsigned long size,
					 struct vm_area_struct *vma)
{
	unsigned i;
	struct mm_struct *mm = current->mm;

	if (virt + size < virt)
		return 0;
	if (vma->vm_start > virt)
		return 0;
	if (vma->vm_start <= virt &&
	    virt + size <= vma->vm_end)
		return 1;
	for (i = 2; ; ++i) {
		struct vm_area_struct *next_vma = find_vma(mm, vma->vm_end);

		if (!next_vma)
			return 0;
		if (next_vma->vm_start != vma->vm_end)
			return 0;
		vma = next_vma;
		if (virt + size <= vma->vm_end)
			return i;
	}
	return 0;
}

static long xrp_share_kernel(struct file *filp,
			     unsigned long virt, unsigned long size,
			     unsigned long flags, phys_addr_t *paddr,
			     struct xrp_mapping *mapping)
{
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
	phys_addr_t phys = __pa(virt);
	long err = 0;

	pr_debug("%s: sharing kernel-only buffer: %pap\n", __func__, &phys);
	if (xrp_translate_to_dsp(&xvp->address_map, phys) ==
	    XRP_NO_TRANSLATION) {
		#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
		mm_segment_t oldfs = get_fs();
		set_fs(KERNEL_DS);
		#else
		mm_segment_t oldfs =force_uaccess_begin();
		#endif

		pr_debug("%s: untranslatable addr, making shadow copy\n",
			 __func__);
	  
		err = xvp_copy_virt_to_phys(xvp_file, flags,
					    virt, size, paddr,
					    &mapping->alien_mapping);
		#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
		set_fs(oldfs);
		#else
		force_uaccess_end(oldfs);
		#endif
		mapping->type = XRP_MAPPING_ALIEN | XRP_MAPPING_KERNEL;
	} else {
		mapping->type = XRP_MAPPING_KERNEL;
		*paddr = phys;

		xrp_default_dma_sync_for_device(xvp, phys, size, flags);
	}
	pr_debug("%s: mapping = %p, mapping->type = %d\n",
		 __func__, mapping, mapping->type);
	return err;
}

static bool vma_needs_cache_ops(struct vm_area_struct *vma)
{
	pgprot_t prot = vma->vm_page_prot;

	return pgprot_val(prot) != pgprot_val(pgprot_noncached(prot)) &&
		pgprot_val(prot) != pgprot_val(pgprot_writecombine(prot));
}

/* Share blocks of memory, from host to IVP or back.
 *
 * When sharing to IVP return physical addresses in paddr.
 * Areas allocated from the driver can always be shared in both directions.
 * Contiguous 3rd party allocations need to be shared to IVP before they can
 * be shared back.
 */

static long __xrp_share_block(struct file *filp,
			      unsigned long virt, unsigned long size,
			      unsigned long flags, phys_addr_t *paddr,
			      struct xrp_mapping *mapping)
{
	phys_addr_t phys = ~0ul;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = find_vma(mm, virt);
	bool do_cache = true;
	long rc = -EINVAL;

	if (!vma) {
		pr_debug("%s: no vma for vaddr/size = 0x%08lx/0x%08lx\n",
			 __func__, virt, size);
		return -EINVAL;
	}
	/*
	 * Region requested for sharing should be within single VMA.
	 * That's true for the majority of cases, but sometimes (e.g.
	 * sharing buffer in the beginning of .bss which shares a
	 * file-mapped page with .data, followed by anonymous page)
	 * region will cross multiple VMAs. Support it in the simplest
	 * way possible: start with get_user_pages and use shadow copy
	 * if that fails.
	 */
	switch (xvp_get_region_vma_count(virt, size, vma)) {
	case 0:
		pr_debug("%s: bad vma for vaddr/size = 0x%08lx/0x%08lx\n",
			 __func__, virt, size);
		pr_debug("%s: vma->vm_start = 0x%08lx, vma->vm_end = 0x%08lx\n",
			 __func__, vma->vm_start, vma->vm_end);
		return -EINVAL;
	case 1:
		break;
	default:
		pr_debug("%s: multiple vmas cover vaddr/size = 0x%08lx/0x%08lx\n",
			 __func__, virt, size);
		vma = NULL;
		break;
	}
	/*
	 * And it need to be allocated from the same file descriptor, or
	 * at least from a file descriptor managed by the XRP.
	 */
	if (vma &&
	    (vma->vm_file == filp || xrp_is_known_file(vma->vm_file))) {
		struct xvp_file *vm_file = vma->vm_file->private_data;
		struct xrp_allocation *xrp_allocation = vma->vm_private_data;

		phys =  (vma->vm_pgoff << PAGE_SHIFT) +
			virt - vma->vm_start;
		pr_debug("%s: XRP allocation at 0x%08lx, paddr: %pap\n",
			 __func__, virt, &phys);
		/*
		 * If it was allocated from a different XRP file it may belong
		 * to a different device and not be directly accessible.
		 * Check if it is.
		 */
		if (vma->vm_file != filp) {
			const struct xrp_address_map_entry *address_map =
				xrp_get_address_mapping(&xvp->address_map,
							phys);

			if (!address_map ||
			    xrp_compare_address(phys + size - 1, address_map))
				pr_debug("%s: untranslatable addr: %pap\n",
					 __func__, &phys);
			else
				rc = 0;

		} else {
			rc = 0;
		}

		if (rc == 0) {
			mapping->type = XRP_MAPPING_NATIVE;
			mapping->native.xrp_allocation = xrp_allocation;
			mapping->native.vaddr = virt;
			xrp_allocation_get(xrp_allocation);
			do_cache = vma_needs_cache_ops(vma);
		}
	}
	if (rc < 0) {
		struct xrp_alien_mapping *alien_mapping =
			&mapping->alien_mapping;
		unsigned long n_pages = PFN_UP(virt + size) - PFN_DOWN(virt);

		/* Otherwise this is alien allocation. */
		pr_debug("%s: non-XVP allocation at 0x%08lx\n",
			 __func__, virt);

		/*
		 * A range can only be mapped directly if it is either
		 * uncached or HW-specific cache operations can handle it.
		 */
		if (vma && vma->vm_flags & (VM_IO | VM_PFNMAP)) {
			rc = xvp_pfn_virt_to_phys(xvp_file, vma,
						  virt, size,
						  &phys,
						  alien_mapping);
			if (rc == 0 && vma_needs_cache_ops(vma) &&
			    !xrp_cacheable(xvp, PFN_DOWN(phys), n_pages)) {
				pr_debug("%s: needs unsupported cache mgmt\n",
					 __func__);
				rc = -EINVAL;
			}
		} else {
			#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
			up_read(&mm->mmap_sem);
			#else
			up_read(&mm->mmap_lock);
			#endif
			rc = xvp_gup_virt_to_phys(xvp_file, virt,
						  size, &phys,
						  alien_mapping);
			if (rc == 0 &&
			    (!vma || vma_needs_cache_ops(vma)) &&
			    !xrp_cacheable(xvp, PFN_DOWN(phys), n_pages)) {
				pr_debug("%s: needs unsupported cache mgmt\n",
					 __func__);
				xrp_put_pages(phys, n_pages);
				rc = -EINVAL;
			}
			#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
			down_read(&mm->mmap_sem);
			#else
			down_read(&mm->mmap_lock);
			#endif
		}
		if (rc == 0 && vma && !vma_needs_cache_ops(vma))
			do_cache = false;

		/*
		 * If we couldn't share try to make a shadow copy.
		 */
		if (rc < 0) {
			rc = xvp_copy_virt_to_phys(xvp_file, flags,
						   virt, size, &phys,
						   alien_mapping);
			do_cache = false;
		}

		/* We couldn't share it. Fail the request. */
		if (rc < 0) {
			pr_debug("%s: couldn't map virt to phys\n",
				 __func__);
			return -EINVAL;
		}

		phys = alien_mapping->paddr +
			virt - alien_mapping->vaddr;

		mapping->type = XRP_MAPPING_ALIEN;
	}

	*paddr = phys;
	pr_debug("%s: mapping = %p, mapping->type = %d,do_cache = %d\n",
		 __func__, mapping, mapping->type,do_cache);

	if (do_cache)
		xrp_dma_sync_for_device(xvp,
					virt, phys, size,
					flags);
	return 0;
}

static long xrp_writeback_alien_mapping(struct xvp_file *xvp_file,
					struct xrp_alien_mapping *alien_mapping,
					unsigned long flags)
{
	struct page *page;
	size_t nr_pages;
	size_t i;
	long ret = 0;

	switch (alien_mapping->type) {
	case ALIEN_GUP:
		xrp_dma_sync_for_cpu(xvp_file->xvp,
				     alien_mapping->vaddr,
				     alien_mapping->paddr,
				     alien_mapping->size,
				     flags);
		pr_debug("%s: dirtying alien GUP @va = %p, pa = %pap\n",
			 __func__, (void __user *)alien_mapping->vaddr,
			 &alien_mapping->paddr);
		page = pfn_to_page(__phys_to_pfn(alien_mapping->paddr));
		nr_pages = PFN_UP(alien_mapping->vaddr + alien_mapping->size) -
			PFN_DOWN(alien_mapping->vaddr);
		for (i = 0; i < nr_pages; ++i)
			SetPageDirty(page + i);
		break;

	case ALIEN_COPY:
		pr_debug("%s: synchronizing alien copy @pa = %pap back to %p\n",
			 __func__, &alien_mapping->paddr,
			 (void __user *)alien_mapping->vaddr);
		if (xrp_copy_user_from_phys(xvp_file->xvp,
					    alien_mapping->vaddr,
					    alien_mapping->size,
					    alien_mapping->paddr,
					    flags))
			ret = -EINVAL;
		break;

	default:
		break;
	}
	return ret;
}

/*
 *
 */
static long __xrp_unshare_block(struct file *filp, struct xrp_mapping *mapping,
				unsigned long flags)
{
	long ret = 0;
	mm_segment_t oldfs ;

	if (mapping->type & XRP_MAPPING_KERNEL)
		#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
		oldfs = get_fs();
		set_fs(KERNEL_DS);
		#else
		oldfs =force_uaccess_begin();
		#endif

	switch (mapping->type & ~XRP_MAPPING_KERNEL) {
	case XRP_MAPPING_NATIVE:
		if (flags & XRP_FLAG_WRITE) {
			struct xvp_file *xvp_file = filp->private_data;

			xrp_dma_sync_for_cpu(xvp_file->xvp,
					     mapping->native.vaddr,
					     mapping->native.xrp_allocation->start,
					     mapping->native.xrp_allocation->size,
					     flags);

		}
		xrp_allocation_put(mapping->native.xrp_allocation);
		break;

	case XRP_MAPPING_ALIEN:
		if (flags & XRP_FLAG_WRITE)
			ret = xrp_writeback_alien_mapping(filp->private_data,
							  &mapping->alien_mapping,
							  flags);

		xrp_alien_mapping_destroy(&mapping->alien_mapping);
		break;

	case XRP_MAPPING_KERNEL:
		break;

	default:
		break;
	}

	if (mapping->type & XRP_MAPPING_KERNEL)
		#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
		set_fs(oldfs);
		#else
		force_uaccess_end(oldfs);
		#endif

	mapping->type = XRP_MAPPING_NONE;

	return ret;
}

static long xrp_ioctl_free(struct file *filp,
			   struct xrp_ioctl_alloc __user *p)
{
	struct mm_struct *mm = current->mm;
	struct xrp_ioctl_alloc xrp_ioctl_alloc;
	struct vm_area_struct *vma;
	unsigned long start;

	// pr_debug("%s: %p\n", __func__, p);
	if (copy_from_user(&xrp_ioctl_alloc, p, sizeof(*p)))
		return -EFAULT;

	start = xrp_ioctl_alloc.addr;
	// pr_debug("%s: virt_addr = 0x%08lx\n", __func__, start);
	#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	down_read(&mm->mmap_sem);
	#else
	down_read(&mm->mmap_lock);
	#endif
	vma = find_vma(mm, start);

	if (vma && vma->vm_file == filp &&
	    vma->vm_start <= start && start < vma->vm_end) {
		size_t size;

		start = vma->vm_start;
		size = vma->vm_end - vma->vm_start;
		#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
		up_read(&mm->mmap_sem);
		#else
		up_read(&mm->mmap_lock);		
		#endif
		pr_debug("%s: 0x%lx x %zu\n", __func__, start, size);
		return vm_munmap(start, size);
	}
	// pr_debug("%s: no vma/bad vma for vaddr = 0x%08lx\n", __func__, start);
	#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)	
	up_read(&mm->mmap_sem);
	#else
	up_read(&mm->mmap_lock);
	#endif

	return -EINVAL;
}

static long xvp_complete_cmd_irq(struct xvp *xvp, struct xrp_comm *comm,
				 bool (*cmd_complete)(struct xrp_comm *p))
{
	long timeout = firmware_command_timeout * HZ;

	if (cmd_complete(comm))
		return 0;
	if (xrp_panic_check(xvp))
		return -EBUSY;
	do {
		timeout = wait_for_completion_interruptible_timeout(&comm->completion,
								    timeout);
		if (cmd_complete(comm))
			return 0;
		if (xrp_panic_check(xvp))
			return -EBUSY;
	} while (timeout > 0);

	if (timeout == 0)
		return -EBUSY;
	return timeout;
}

static long xvp_complete_cmd_poll(struct xvp *xvp, struct xrp_comm *comm,
				  bool (*cmd_complete)(struct xrp_comm *p))
{
	unsigned long deadline = jiffies + firmware_command_timeout * HZ;

	do {
		if (cmd_complete(comm))
			return 0;
		if (xrp_panic_check(xvp))
			return -EBUSY;
		schedule();
	} while (time_before(jiffies, deadline));

	return -EBUSY;
}

struct xrp_request {
	struct xrp_ioctl_queue ioctl_queue;
	size_t n_buffers;
	struct xrp_mapping *buffer_mapping;
	struct xrp_dsp_buffer *dsp_buffer;
	phys_addr_t in_data_phys;
	phys_addr_t out_data_phys;
	phys_addr_t dsp_buffer_phys;
	union {
		struct xrp_mapping in_data_mapping;
		u8 in_data[XRP_DSP_CMD_INLINE_DATA_SIZE];
	};
	union {
		struct xrp_mapping out_data_mapping;
		u8 out_data[XRP_DSP_CMD_INLINE_DATA_SIZE];
	};
	union {
		struct xrp_mapping dsp_buffer_mapping;
		struct xrp_dsp_buffer buffer_data[XRP_DSP_CMD_INLINE_BUFFER_COUNT];
	};
	u8 nsid[XRP_DSP_CMD_NAMESPACE_ID_SIZE];
};

static void xrp_unmap_request_nowb(struct file *filp, struct xrp_request *rq)
{
	size_t n_buffers = rq->n_buffers;
	size_t i;

	if (rq->ioctl_queue.in_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
		__xrp_unshare_block(filp, &rq->in_data_mapping, 0);
	if (rq->ioctl_queue.out_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
		__xrp_unshare_block(filp, &rq->out_data_mapping, 0);
	for (i = 0; i < n_buffers; ++i)
		__xrp_unshare_block(filp, rq->buffer_mapping + i, 0);
	if (n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT)
		__xrp_unshare_block(filp, &rq->dsp_buffer_mapping, 0);

	if (n_buffers) {
		kfree(rq->buffer_mapping);
		if (n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT) {
			kfree(rq->dsp_buffer);
		}
	}
}

static long xrp_unmap_request(struct file *filp, struct xrp_request *rq)
{
	size_t n_buffers = rq->n_buffers;
	size_t i;
	long ret = 0;
	long rc;

	if (rq->ioctl_queue.in_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
		__xrp_unshare_block(filp, &rq->in_data_mapping, XRP_FLAG_READ);
	if (rq->ioctl_queue.out_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE) {
		rc = __xrp_unshare_block(filp, &rq->out_data_mapping,
					 XRP_FLAG_WRITE);

		if (rc < 0) {
			pr_debug("%s: out_data could not be unshared\n",
				 __func__);
			ret = rc;
		}
	} else {
		pr_debug("%s: out_data <%s> to copied\n",
			__func__,rq->out_data);
		if (copy_to_user((void __user *)(unsigned long)rq->ioctl_queue.out_data_addr,
				 rq->out_data,
				 rq->ioctl_queue.out_data_size)) {
			pr_debug("%s: out_data could not be copied\n",
				 __func__);
			ret = -EFAULT;
		}
	}

	if (n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT)
		__xrp_unshare_block(filp, &rq->dsp_buffer_mapping,
				    XRP_FLAG_READ_WRITE);

	for (i = 0; i < n_buffers; ++i) {
		rc = __xrp_unshare_block(filp, rq->buffer_mapping + i,
					 rq->dsp_buffer[i].flags);
		if (rc < 0) {
			pr_debug("%s: buffer %zd could not be unshared\n",
				 __func__, i);
			ret = rc;
		}
	}

	if (n_buffers) {
		kfree(rq->buffer_mapping);
		if (n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT) {
			kfree(rq->dsp_buffer);
		}
		rq->n_buffers = 0;
	}

	return ret;
}

static long xrp_map_request(struct file *filp, struct xrp_request *rq,
			    struct mm_struct *mm)
{
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
	struct xrp_ioctl_buffer __user *buffer;
	size_t n_buffers = rq->ioctl_queue.buffer_size /
						sizeof(struct xrp_ioctl_buffer);

	size_t i;
	long ret = 0;

	if ((rq->ioctl_queue.flags & XRP_QUEUE_FLAG_NSID) &&
	    copy_from_user(rq->nsid,
			   (void __user *)(unsigned long)rq->ioctl_queue.nsid_addr,
			   sizeof(rq->nsid))) {
		pr_debug("%s: nsid could not be copied\n ", __func__);
		return -EINVAL;
	}
	rq->n_buffers = n_buffers;
	if (n_buffers) {
		rq->buffer_mapping =
			kzalloc(n_buffers * sizeof(*rq->buffer_mapping),
				GFP_KERNEL);
		if (n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT) {
			rq->dsp_buffer =
				kmalloc(n_buffers * sizeof(*rq->dsp_buffer),
					GFP_KERNEL);
			if (!rq->dsp_buffer) {
				kfree(rq->buffer_mapping);
				return -ENOMEM;
			}
		} else {
			rq->dsp_buffer = rq->buffer_data;
		}
	}
	#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)	
	down_read(&mm->mmap_sem);
	#else
	down_read(&mm->mmap_lock);
	#endif

	if (rq->ioctl_queue.in_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE) {
		ret = __xrp_share_block(filp, rq->ioctl_queue.in_data_addr,
					rq->ioctl_queue.in_data_size,
					XRP_FLAG_READ, &rq->in_data_phys,
					&rq->in_data_mapping);
		if(ret < 0) {
			pr_debug("%s: in_data could not be shared\n",
				 __func__);
			goto share_err;
		}
	} else {
		if (copy_from_user(rq->in_data,
				   (void __user *)(unsigned long)rq->ioctl_queue.in_data_addr,
				   rq->ioctl_queue.in_data_size)) {
			pr_debug("%s: in_data could not be copied\n",
				 __func__);
			ret = -EFAULT;
			goto share_err;
		}
	}

	if (rq->ioctl_queue.out_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE) {
		ret = __xrp_share_block(filp, rq->ioctl_queue.out_data_addr,
					rq->ioctl_queue.out_data_size,
					XRP_FLAG_WRITE, &rq->out_data_phys,
					&rq->out_data_mapping);
		if (ret < 0) {
			pr_debug("%s: out_data could not be shared\n",
				 __func__);
			goto share_err;
		}
	}

	buffer = (void __user *)(unsigned long)rq->ioctl_queue.buffer_addr;

	for (i = 0; i < n_buffers; ++i) {
		struct xrp_ioctl_buffer ioctl_buffer;
		phys_addr_t buffer_phys = ~0ul;

		if (copy_from_user(&ioctl_buffer, buffer + i,
				   sizeof(ioctl_buffer))) {
			ret = -EFAULT;
			goto share_err;
		}
		if (ioctl_buffer.flags & XRP_FLAG_READ_WRITE) {
			ret = __xrp_share_block(filp, ioctl_buffer.addr,
						ioctl_buffer.size,
						ioctl_buffer.flags,
						&buffer_phys,
						rq->buffer_mapping + i);
			if (ret < 0) {
				pr_debug("%s: buffer %zd could not be shared\n",
					 __func__, i);
				goto share_err;
			}
		}

		rq->dsp_buffer[i] = (struct xrp_dsp_buffer){
			.flags = ioctl_buffer.flags,
			.size = ioctl_buffer.size,
			.addr = xrp_translate_to_dsp(&xvp->address_map,
						     buffer_phys),
		};
	}

	if (n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT) {
		ret = xrp_share_kernel(filp, (unsigned long)rq->dsp_buffer,
				       n_buffers * sizeof(*rq->dsp_buffer),
				       XRP_FLAG_READ_WRITE, &rq->dsp_buffer_phys,
				       &rq->dsp_buffer_mapping);
		if(ret < 0) {
			pr_debug("%s: buffer descriptors could not be shared\n",
				 __func__);
			goto share_err;
		}
	}
share_err:
	#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)	
	up_read(&mm->mmap_sem);
	#else
	up_read(&mm->mmap_lock);
	#endif
	if (ret < 0)
		xrp_unmap_request_nowb(filp, rq);
	return ret;
}

static void xrp_fill_hw_request(struct xrp_dsp_cmd __iomem *cmd,
				struct xrp_request *rq,
				const struct xrp_address_map *map)
{
	xrp_comm_write32(&cmd->in_data_size, rq->ioctl_queue.in_data_size);
	xrp_comm_write32(&cmd->out_data_size, rq->ioctl_queue.out_data_size);
	xrp_comm_write32(&cmd->buffer_size,
			 rq->n_buffers * sizeof(struct xrp_dsp_buffer));

	if (rq->ioctl_queue.in_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
		xrp_comm_write32(&cmd->in_data_addr,
				 xrp_translate_to_dsp(map, rq->in_data_phys));
	else
		xrp_comm_write(&cmd->in_data, rq->in_data,
			       rq->ioctl_queue.in_data_size);

	if (rq->ioctl_queue.out_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
		xrp_comm_write32(&cmd->out_data_addr,
				 xrp_translate_to_dsp(map, rq->out_data_phys));

	if (rq->n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT)
		xrp_comm_write32(&cmd->buffer_addr,
				 xrp_translate_to_dsp(map, rq->dsp_buffer_phys));
	else
		xrp_comm_write(&cmd->buffer_data, rq->dsp_buffer,
			       rq->n_buffers * sizeof(struct xrp_dsp_buffer));

	if (rq->ioctl_queue.flags & XRP_QUEUE_FLAG_NSID)
		xrp_comm_write(&cmd->nsid, rq->nsid, sizeof(rq->nsid));

#ifdef DEBUG
	{
		struct xrp_dsp_cmd dsp_cmd;
		xrp_comm_read(cmd, &dsp_cmd, sizeof(dsp_cmd));
		pr_debug("%s: cmd for DSP: %p: %*ph\n",
			 __func__, cmd,
			 (int)sizeof(dsp_cmd), &dsp_cmd);
	}
#endif

	wmb();
	/* update flags */
	xrp_comm_write32(&cmd->flags,
			 (rq->ioctl_queue.flags & ~XRP_DSP_CMD_FLAG_RESPONSE_VALID) |
			 XRP_DSP_CMD_FLAG_REQUEST_VALID);
}

static long xrp_complete_hw_request(struct xrp_dsp_cmd __iomem *cmd,
				    struct xrp_request *rq)
{
	u32 flags = xrp_comm_read32(&cmd->flags);

	if (rq->ioctl_queue.out_data_size <= XRP_DSP_CMD_INLINE_DATA_SIZE)
		xrp_comm_read(&cmd->out_data, rq->out_data,
			      rq->ioctl_queue.out_data_size);
	if (rq->n_buffers <= XRP_DSP_CMD_INLINE_BUFFER_COUNT)
		xrp_comm_read(&cmd->buffer_data, rq->dsp_buffer,
			      rq->n_buffers * sizeof(struct xrp_dsp_buffer));
	xrp_comm_write32(&cmd->flags, 0);

	return (flags & XRP_DSP_CMD_FLAG_RESPONSE_DELIVERY_FAIL) ? -ENXIO : 0;
}

static long xrp_ioctl_submit_sync(struct file *filp,
				  struct xrp_ioctl_queue __user *p)
{
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
	struct xrp_comm *queue = xvp->queue;
	struct xrp_request xrp_rq, *rq = &xrp_rq;
	long ret = 0;
	bool went_off = false;

	if (copy_from_user(&rq->ioctl_queue, p, sizeof(*p)))
		return -EFAULT;

	if (rq->ioctl_queue.flags & ~XRP_QUEUE_VALID_FLAGS) {
		dev_dbg(xvp->dev, "%s: invalid flags 0x%08x\n",
			__func__, rq->ioctl_queue.flags);
		return -EINVAL;
	}

	if (xvp->n_queues > 1) {
		unsigned n = (rq->ioctl_queue.flags & XRP_QUEUE_FLAG_PRIO) >>
			XRP_QUEUE_FLAG_PRIO_SHIFT;

		if (n >= xvp->n_queues)
			n = xvp->n_queues - 1;
		queue = xvp->queue_ordered[n];
		dev_dbg(xvp->dev, "%s: priority: %d -> %d\n",
			__func__, n, queue->priority);
	}

	ret = xrp_map_request(filp, rq, current->mm);
	if (ret < 0)
		return ret;

	if (loopback < LOOPBACK_NOIO) {
		int reboot_cycle;
retry:
		mutex_lock(&queue->lock);
		reboot_cycle = atomic_read(&xvp->reboot_cycle);
		if (reboot_cycle != atomic_read(&xvp->reboot_cycle_complete)) {
			mutex_unlock(&queue->lock);
			goto retry;
		}

		if (xvp->off) {
			ret = -ENODEV;
		} else {
			xrp_fill_hw_request(queue->comm, rq, &xvp->address_map);

			xrp_send_device_irq(xvp);

			if (xvp->host_irq_mode) {
				ret = xvp_complete_cmd_irq(xvp, queue,
							   xrp_cmd_complete);
			} else {
				ret = xvp_complete_cmd_poll(xvp, queue,
							    xrp_cmd_complete);
			}

			xrp_panic_check(xvp);

			/* copy back inline data */
			if (ret == 0) {
				ret = xrp_complete_hw_request(queue->comm, rq);
			} else if (ret == -EBUSY && firmware_reboot &&
				   atomic_inc_return(&xvp->reboot_cycle) ==
				   reboot_cycle + 1) {
				int rc;
				unsigned i;

				dev_dbg(xvp->dev,
					"%s: restarting firmware...\n",
					 __func__);
				for (i = 0; i < xvp->n_queues; ++i)
					if (xvp->queue + i != queue)
						mutex_lock(&xvp->queue[i].lock);
				rc = xrp_boot_firmware(xvp);
				atomic_set(&xvp->reboot_cycle_complete,
					   atomic_read(&xvp->reboot_cycle));
				for (i = 0; i < xvp->n_queues; ++i)
					if (xvp->queue + i != queue)
						mutex_unlock(&xvp->queue[i].lock);
				if (rc < 0) {
					ret = rc;
					went_off = xvp->off;
				}
			}
		}
		mutex_unlock(&queue->lock);
	}

	if (ret == 0)
		ret = xrp_unmap_request(filp, rq);
	else if (!went_off)
		xrp_unmap_request_nowb(filp, rq);
	/*
	 * Otherwise (if the DSP went off) all mapped buffers are leaked here.
	 * There seems to be no way to recover them as we don't know what's
	 * going on with the DSP; the DSP may still be reading and writing
	 * this memory.
	 */

	return ret;
}
// static void xrp_dam_buf_free(struct xrp_allocation *xrp_allocation) 
// {
//     dev_dbg(xvp->dev,"%s: release dma_buf allocation n",
// 					 __func__);
//     kfree(xrp_allocation->pool);
//     kfree(xrp_allocation);
//     return
// }

// static void xrp_dam_buf_offset(struct xrp_allocation *xrp_allocation)
// {
//     return 0;
// }
// static const struct xrp_allocation_ops xrp_dma_buf_pool_ops = {
// 	.alloc = NULL,
// 	.free = xrp_dam_buf_free,
// 	.free_pool = NULL,
// 	.offset = xrp_dam_buf_offset,
// };

// static inline struct xrp_dma_buf_item * xrp_get_dma_buf_tail(struct xrp_dma_buf_item **list)
// {
//     struct xrp_dma_buf_item ** item;

//     if(*list == NULLL)
//         return NULL;

//     for(item = list;(*item)->next != NULL;item= &(*item)->next)
//     {
//         ;
//     }
//     return *item;
// }

// static inline void xrp_dam_buf_add_item(struct xrp_dma_buf_item **list,struct xrp_dma_buf_item *entry)
// {
//     struct xrp_dma_buf_item * item = xrp_get_dma_buf_tail(list);
//     if(item == NULL)
//     {
//         *list=entry;
//     }
//     else{
//         item->next = entry;
//     }
// }

// static inline int xrp_get_dma_buf_remove(struct xrp_dma_buf_item **list,struct xrp_dma_buf_item *entry)
// {
//     {
//     struct xrp_dma_buf_item ** item;


//     for(item = list;(*item)->next != NULL;item= &(*item)->next)
//     {
//         struct xrp_dma_buf_item *cur = *item;
//         if();
//     }
// }
static void xrp_release_dma_buf_item(struct xrp_dma_buf_item * item)
{
    spin_lock(&xrp_dma_buf_lock);
    if(--item->ref==0)
    {
        list_del(&item->link);
        kfree(item);
    }
    spin_unlock(&xrp_dma_buf_lock);
}
static long xrp_ioctl_dma_buf_import(struct file *filp,
			                struct xrp_dma_buf __user *p)
{
    long ret;
    struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
    struct xrp_dma_buf  xrp_dma_buf;
    struct dma_buf *dmabuf = NULL;
    struct sg_table *sgt = NULL;
    struct xrp_dma_buf_item *dma_buf_item=NULL;
    struct xrp_dma_buf_item *temp=NULL;
    struct dma_buf_attachment *attachment = NULL;
    // struct xrp_allocation *xrp_allocation;
    // struct xrp_private_pool *pool;
    int npages = 0;
    int i;
    struct scatterlist *s;
    unsigned int size = 0;

    dev_dbg(xvp->dev,"%s: entry\n", __func__);
	if (copy_from_user(&xrp_dma_buf, p, sizeof(*p)))
    {
		return -EFAULT;
    }

    dmabuf = dma_buf_get(xrp_dma_buf.fd);

    if(!dmabuf)
    {
        return -EFAULT;
    }

    spin_lock(&xrp_dma_buf_lock);
    list_for_each_entry(temp,&xvp->dma_buf_list, link)
    {
        if(temp->dmabuf == dmabuf)
        {
            dma_buf_item = temp;
            dma_buf_item->ref++;
            break;
        } 
    }

   spin_unlock(&xrp_dma_buf_lock);

    if(dma_buf_item == NULL)
    {
        dev_dbg(xvp->dev,
					"%s: no exit same dma buf\n", __func__);
        attachment = dma_buf_attach(dmabuf, xvp->dev);
        if (!attachment)
        {
            goto One_Err;
        }

        sgt = dma_buf_map_attachment(attachment, xrp_dma_direction(xrp_dma_buf.flags));
        if (!sgt)
        {
        goto One_Err;
        }

        dma_buf_item = kzalloc(sizeof(*dma_buf_item),GFP_KERNEL);
        if(dma_buf_item == NULL)
        {
            goto One_Err;
        }
        
        dma_buf_item->attachment = attachment;
        dma_buf_item->dmabuf = dmabuf;
        dma_buf_item->sgt = sgt;
        dma_buf_item->ref = 1;
        spin_lock(&xrp_dma_buf_lock);
        list_add_tail(&dma_buf_item->link, &xvp->dma_buf_list);
        spin_unlock(&xrp_dma_buf_lock);
    }
    else
    {
        dev_dbg(xvp->dev,
					"%s: exit same dma buf\n", __func__);
        attachment = dma_buf_item->attachment;
        sgt = dma_buf_item->sgt;
        spin_lock(&xrp_dma_buf_lock);
        dma_buf_item->ref++;
        spin_unlock(&xrp_dma_buf_lock);
    }

    if(sgt->nents != 1)
    {
        dev_dbg(xvp->dev,
					"%s: sg table number (%d) is not 1, unspoort.\n",
					 __func__,sgt->nents);
        goto Two_Err;
    }
    /* Prepare page array. */
    /* Get number of pages. */
    for_each_sg(sgt->sgl, s, sgt->orig_nents, i)
    {
        npages += (sg_dma_len(s) + PAGE_SIZE - 1) / PAGE_SIZE;
        size += sg_dma_len(s);
    }
 
    xrp_dma_buf.size = size;
#ifdef VIDMEM_DMA_MAP
    xrp_dma_buf. = sg_dma_address(s) + j * PAGE_SIZE;
#else
    // xrp_dma_buf.paddr = page_to_phys(nth_page(sg_page(s), 0));
    xrp_dma_buf.paddr = sg_phys(sgt->sgl);
#endif
    // dev_dbg(xvp->dev,
	// 				"%s: import dma-buf phy addr:0x%lx,size:%d\n",
	// 				 __func__,xrp_dma_buf.paddr,xrp_dma_buf.size);
//    xrp_allocation = kzalloc(sizeof(*xrp_allocation), GFP_KERNEL | __GFP_NORETRY);
//    if(!xrp_allocation)
//    {
//        return -ENOMEM;
//    }
//    pool = kmalloc(sizeof(*pool), GFP_KERNEL);
//    if(!pool)
//    {
//        kfree(xrp_allocation);
//        return -ENOMEM;
//    }
//    *pool = (struct xrp_private_pool){
// 		.pool = {
// 			.ops = &xrp_dma_buf_pool_ops,
// 		},
// 		.start = xrp_dma_buf.paddr ,
// 		.size = xrp_dma_buf.size,
// 		.free_list = NULL,
// 	};
//    xrp_allocation->pool = pool;
//    xrp_allocation->start = xrp_dma_buf.paddr;
//    xrp_allocation->size = xrp_dma_buf.size;
   
//    xrp_allocation_queue(xvp_file, xrp_allocation);

//    xrp_dma_buf.addr = vm_mmap(filp, 0, xrp_allocation->size,
// 			PROT_READ | PROT_WRITE, MAP_SHARED,
// 			xrp_dam_buf_offset(xrp_allocation));


    struct file *export_filp = fget(xrp_dma_buf.fd);
    xrp_dma_buf.addr = vm_mmap(export_filp, 0, xrp_dma_buf.size,
			            PROT_READ | PROT_WRITE, MAP_SHARED,0); 
    
    fput(export_filp);
    dev_dbg(xvp->dev,
					"%s: import dma-buf phy addr:0x%lx,user addr:0x%lx,size:%d\n",
					 __func__,xrp_dma_buf.paddr,xrp_dma_buf.addr,xrp_dma_buf.size);


    if (copy_to_user(p, &xrp_dma_buf, sizeof(*p))) {
        dma_buf_put(dmabuf);
		vm_munmap(xrp_dma_buf.addr , xrp_dma_buf.size);
		goto Two_Err;
	}

    return 0;

Two_Err:
    xrp_release_dma_buf_item(dma_buf_item);
One_Err:
    dma_buf_put(dmabuf);
    return -EINVAL;
}

static struct xrp_dma_buf_item * xrp_search_dma_buf( struct list_head *list,int fd)
{
    struct xrp_dma_buf_item *loop;
    struct xrp_dma_buf_item *dma_buf_item=NULL;
    struct dma_buf *dmabuf = NULL;
    // pr_debug("%s: fd %d,entry\n", __func__,fd);
    dmabuf = dma_buf_get(fd);
    spin_lock(&xrp_dma_buf_lock);
    list_for_each_entry(loop,list, link)
    {
        if(loop->dmabuf == dmabuf)
        {
            dma_buf_item = loop;
            break;
        }

    }
    spin_unlock(&xrp_dma_buf_lock);
    dma_buf_put(dmabuf);
    pr_debug("%s: %p exit\n", __func__,fd,dma_buf_item);
    return dma_buf_item;
}

static long xrp_ioctl_dma_buf_release(struct file *filp,
			                        int __user *p)
{
    int fd;
    struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
    struct dma_buf *dmabuf = NULL;
    struct xrp_dma_buf_item *dma_buf_item=NULL;
    struct xrp_dma_buf_item *loop,*temp;
    
    if (copy_from_user(&fd, p, sizeof(*p)))
    {
		return -EFAULT;
    }

    // dmabuf = dma_buf_get(fd);
    // spin_lock(&xrp_dma_buf_lock);
    // list_for_each_entry_safe(loop, temp, &xvp->dma_buf_list, link)
    // {
    //     if(loop->dmabuf == dmabuf)
    //     {
    //         dma_buf_item = loop;
    //         if((--dma_buf_item->ref)==0)
    //             list_del(&dma_buf_item);
    //         break;
    //     }
 
    // }
    // spin_unlock(&xrp_dma_buf_lock);
    // dma_buf_put(dmabuf);
    dma_buf_item = xrp_search_dma_buf(&xvp->dma_buf_list,fd);
    if(dma_buf_item == NULL)
    {
        return -EFAULT;
    }
    
    dma_buf_unmap_attachment(dma_buf_item->attachment, dma_buf_item->sgt, DMA_BIDIRECTIONAL);
    dma_buf_detach(dma_buf_item->dmabuf, dma_buf_item->attachment);
    dma_buf_put(dma_buf_item->dmabuf);
    xrp_release_dma_buf_item(dma_buf_item);
    return 0;
}


static long xrp_ioctl_dma_buf_sync(struct file *filp,
			                struct xrp_dma_buf __user *p)
{
    struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
    struct xrp_dma_buf  xrp_dma_buf;
    struct xrp_dma_buf_item *dma_buf_item=NULL;
    if (copy_from_user(&xrp_dma_buf, p, sizeof(*p)))
    {
		return -EFAULT;
    }

    dma_buf_item = xrp_search_dma_buf(&xvp->dma_buf_list,xrp_dma_buf.fd);
    if(dma_buf_item == NULL)
    {
        return -EFAULT;
    }
    switch(xrp_dma_buf.flags)
    {   
        case XRP_FLAG_READ:
                    dma_sync_single_for_cpu(xvp->dev, phys_to_dma(xvp->dev, xrp_dma_buf.paddr), xrp_dma_buf.size,
				                             xrp_dma_direction(xrp_dma_buf.flags));
                   break;
        case XRP_FLAG_WRITE:
                    dma_sync_single_for_device(xvp->dev, phys_to_dma(xvp->dev,  xrp_dma_buf.paddr), xrp_dma_buf.size,
				                             xrp_dma_direction(xrp_dma_buf.flags));
                   break;
        case XRP_FLAG_READ_WRITE:
                    dma_sync_single_for_cpu(xvp->dev, phys_to_dma(xvp->dev,  xrp_dma_buf.paddr), xrp_dma_buf.size,
				                             xrp_dma_direction(xrp_dma_buf.flags));
                    dma_sync_single_for_device(xvp->dev, phys_to_dma(xvp->dev,  xrp_dma_buf.paddr),xrp_dma_buf.size,
				                             xrp_dma_direction(xrp_dma_buf.flags));
                    break;
        default:
            dev_dbg(xvp->dev,"%s: invalid type%x\n", __func__, xrp_dma_buf.flags);
            return -EFAULT;
    }
    return 0;
}
static long xvp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval;

	pr_debug("%s: %x\n", __func__, cmd);

	switch(cmd){
	case XRP_IOCTL_ALLOC:
		retval = xrp_ioctl_alloc(filp,
					 (struct xrp_ioctl_alloc __user *)arg);
		break;

	case XRP_IOCTL_FREE:
		retval = xrp_ioctl_free(filp,
					(struct xrp_ioctl_alloc __user *)arg);
		break;

	case XRP_IOCTL_QUEUE:
	case XRP_IOCTL_QUEUE_NS:
		retval = xrp_ioctl_submit_sync(filp,
					       (struct xrp_ioctl_queue __user *)arg);
		break;
	case XRP_IOCTL_REPORT_CREATE:
		retval = xrp_ioctl_alloc_report(filp,
					       (struct xrp_ioctl_alloc __user *)arg);
		break;
	case XRP_IOCTL_REPORT_RELEASE:
		retval = xrp_ioctl_release_report(filp,
					(struct xrp_ioctl_alloc __user *)arg);
		break;
    case XRP_IOCTL_DMABUF_IMPORT:
        retval = xrp_ioctl_dma_buf_import(filp,
                    (struct xrp_dma_buf __user *)arg);
        break;
    case XRP_IOCTL_DMABUF_RELEASE:
        retval = xrp_ioctl_dma_buf_release(filp,
                                    (int __user *)arg);
        break;
    case XRP_IOCTL_DMABUF_SYNC:
        retval = xrp_ioctl_dma_buf_sync(filp,
                    (struct xrp_dma_buf __user *)arg);
        break;
	default:
		retval = -EINVAL;
		break;
	}
	return retval;
}

static void xvp_vm_open(struct vm_area_struct *vma)
{
	// pr_debug("%s\n", __func__);
	xrp_allocation_get(vma->vm_private_data);
}

static void xvp_vm_close(struct vm_area_struct *vma)
{
	// pr_debug("%s\n", __func__);
	xrp_allocation_put(vma->vm_private_data);
}

static const struct vm_operations_struct xvp_vm_ops = {
	.open = xvp_vm_open,
	.close = xvp_vm_close,
};

static int xvp_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int err;
	struct xvp_file *xvp_file = filp->private_data;
	unsigned long pfn = vma->vm_pgoff;// + PFN_DOWN(xvp_file->xvp->pmem);
	struct xrp_allocation *xrp_allocation;

	xrp_allocation = xrp_allocation_dequeue(filp->private_data,
						pfn << PAGE_SHIFT,
						vma->vm_end - vma->vm_start);
	if (xrp_allocation) {
		struct xvp *xvp = xvp_file->xvp;
		pgprot_t prot = vma->vm_page_prot;

		if (!xrp_cacheable(xvp, pfn,
				   PFN_DOWN(vma->vm_end - vma->vm_start))) {
			prot = pgprot_writecombine(prot);
			// prot = pgprot_noncached(prot);
			vma->vm_page_prot = prot;
			dev_dbg(xvp->dev,"%s cache atribution set \n", __func__);
		}

		err = remap_pfn_range(vma, vma->vm_start, pfn,
				      vma->vm_end - vma->vm_start,
				      prot);

		vma->vm_private_data = xrp_allocation;
		vma->vm_ops = &xvp_vm_ops;
	} else {
		pr_err("%s no valid xrp allocate for %lx:\n", __func__,pfn);
		err = -EINVAL;
	}

	return err;
}

static int xvp_open(struct inode *inode, struct file *filp)
{
	struct xvp *xvp = container_of(filp->private_data,
				       struct xvp, miscdev);
	struct xvp_file *xvp_file;
	int rc;

	dev_dbg(xvp->dev,"%s\n", __func__);
	rc = pm_runtime_get_sync(xvp->dev);
	if (rc < 0)
    {
        dev_err(xvp->dev,"%s:pm_runtime_get_sync fail:%d\n", __func__,rc);
        return rc;
    }


	xvp_file = devm_kzalloc(xvp->dev, sizeof(*xvp_file), GFP_KERNEL);
	if (!xvp_file) {
        dev_err(xvp->dev,"%s:malloc fail\n", __func__);
		pm_runtime_put_sync(xvp->dev);
		return -ENOMEM;
	}

	xvp_file->xvp = xvp;
	spin_lock_init(&xvp_file->busy_list_lock);
	filp->private_data = xvp_file;
	xrp_add_known_file(filp);
	return 0;
}

static int xvp_close(struct inode *inode, struct file *filp)
{
	struct xvp_file *xvp_file = filp->private_data;

	pr_debug("%s\n", __func__);
	xrp_report_fasync_release(filp);
	xrp_remove_known_file(filp);
	pm_runtime_put_sync(xvp_file->xvp->dev);
	devm_kfree(xvp_file->xvp->dev, xvp_file);
	return 0;
}

static inline int xvp_enable_dsp(struct xvp *xvp)
{
	if (loopback < LOOPBACK_NOMMIO &&
	    xvp->hw_ops->enable)
		return xvp->hw_ops->enable(xvp->hw_arg);
	else
		return 0;
}

static inline void xvp_disable_dsp(struct xvp *xvp)
{
	if (loopback < LOOPBACK_NOMMIO &&
	    xvp->hw_ops->disable)
		xvp->hw_ops->disable(xvp->hw_arg);
}

static inline void xvp_remove_proc(struct xvp *xvp)
{
    if( xvp->proc_dir)
    {
        if(xvp->panic_log)
        {
            xrp_remove_panic_log_proc(xvp->panic_log);
            xvp->panic_log =NULL;
        }
        // remove_proc_entry(xvp->proc_dir,NULL);
        proc_remove(xvp->proc_dir);
    }
}

static inline void xrp_set_resetVec(struct xvp *xvp,u32 addr)
{
	if (loopback < LOOPBACK_NOMMIO &&
	    xvp->hw_ops->set_reset_vector)
		xvp->hw_ops->set_reset_vector(xvp->hw_arg,addr);
}
static inline void xrp_reset_dsp(struct xvp *xvp)
{
	if (loopback < LOOPBACK_NOMMIO &&
	    xvp->hw_ops->reset)
		xvp->hw_ops->reset(xvp->hw_arg);
}

static inline void xrp_halt_dsp(struct xvp *xvp)
{
	if (loopback < LOOPBACK_NOMMIO &&
	    xvp->hw_ops->halt)
		xvp->hw_ops->halt(xvp->hw_arg);
}

static inline void xrp_release_dsp(struct xvp *xvp)
{
	if (loopback < LOOPBACK_NOMMIO &&
	    xvp->hw_ops->release)
		xvp->hw_ops->release(xvp->hw_arg);
}

static int xrp_boot_firmware(struct xvp *xvp)
{
	int ret;
	u32 fm_entry_point=0;
	struct xrp_dsp_sync_v1 __iomem *shared_sync = xvp->comm;
	// dev_dbg(xvp->dev,"%s",__func__);
//#if 1      //LOAD_MODE_MANUAL  load release dsp by xplorer	
    if(load_mode == LOAD_MODE_AUTO)
    {
        xrp_halt_dsp(xvp);
        //xrp_reset_dsp(xvp);

        if (xvp->firmware_name) {
            if (loopback < LOOPBACK_NOFIRMWARE) {
                ret = xrp_request_firmware(xvp,&fm_entry_point);
                if (ret < 0)
                    return ret;
            }

            if (loopback < LOOPBACK_NOIO) {
                xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_IDLE);
                mb();
            }
            // fm_entry_point = xrp_get_firmware_entry_addr(xvp);
            dev_dbg(xvp->dev,"%s,firmware entry point :%x\n",__func__,fm_entry_point);
            if(fm_entry_point)
            {
                xrp_set_resetVec(xvp,fm_entry_point);
            }
        }
        xrp_reset_dsp(xvp);
    }

	xrp_release_dsp(xvp);
//#endif
	if (loopback < LOOPBACK_NOIO) {
		ret = xrp_synchronize(xvp);
		if (ret < 0) {
			xrp_halt_dsp(xvp);
			dev_err(xvp->dev,
				"%s: couldn't synchronize with the DSP core\n",
				__func__);
			dev_err(xvp->dev,
				"XRP device will not use the DSP until the driver is rebound to this device\n");
			xvp->off = true;
			return ret;
		}
	}
	return 0;
}

static const struct file_operations xvp_fops = {
	.owner  = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = xvp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = xvp_ioctl,
#endif
	.mmap = xvp_mmap,
	.open = xvp_open,
	.fasync = xrp_report_fasync,
	.release = xvp_close,
};

int xrp_runtime_suspend(struct device *dev)
{
	struct xvp *xvp = dev_get_drvdata(dev);

	xrp_halt_dsp(xvp);
    xrp_reset_dsp(xvp);
	xvp_disable_dsp(xvp);
    // release_firmware(xvp->firmware);
	return 0;
}
EXPORT_SYMBOL(xrp_runtime_suspend);

int xrp_runtime_resume(struct device *dev)
{
	struct xvp *xvp = dev_get_drvdata(dev);
	unsigned i;
	int ret = 0;

	for (i = 0; i < xvp->n_queues; ++i)
		mutex_lock(&xvp->queue[i].lock);

	if (xvp->off)
		goto out;
	ret = xvp_enable_dsp(xvp);
	if (ret < 0) {
		dev_err(xvp->dev, "couldn't enable DSP\n");
		goto out;
	}

	ret = xrp_boot_firmware(xvp);
	if (ret < 0)
		xvp_disable_dsp(xvp);

out:
	for (i = 0; i < xvp->n_queues; ++i)
		mutex_unlock(&xvp->queue[i].lock);

	return ret;
}
EXPORT_SYMBOL(xrp_runtime_resume);

static int xrp_init_regs_v0(struct platform_device *pdev, struct xvp *xvp,int mem_idx)
{
    struct resource res;
	struct device_node *np;
    int ret = 0;
	np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!np) {
		dev_err(&pdev->dev, "No memory-region specified\n");
		return -EINVAL;
	}
	ret = of_address_to_resource(np, 0, &res);
    dev_dbg(xvp->dev,"%s:dsp runing addr 0x%llx,size:0x%x\n", __func__,
		     res.start,resource_size(&res));


    ret = of_address_to_resource(np, 1, &res);
    if (ret)
    {
        dev_dbg(xvp->dev,"%s:get comm region fail\n", __func__);
		return -ENODEV;
    }

	xvp->comm_phys = res.start;
	xvp->comm = devm_ioremap_resource(&pdev->dev, &res);
    dev_dbg(xvp->dev,"%s:xvp->comm =0x%p, phy_addr base=0x%llx\n", __func__,
		     xvp->comm, xvp->comm_phys);
	// mem = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx);

    ret = of_address_to_resource(np, 2, &res);
    if(ret)
    {
        dev_dbg(xvp->dev,"%s:get paic region fail:%d\n", __func__,ret);
    }else
    {
        xvp->panic_phy = res.start;
        xvp->panic = devm_ioremap_resource(&pdev->dev, &res);
        xvp->panic_size = resource_size(&res);
        if(xvp->panic)
        {
            dev_dbg(xvp->dev,"%s:panic=0x%p, panic phy base=0x%llx,size:%d\n", __func__,
                    xvp->panic, xvp->panic_phy,xvp->panic_size);
        }else
        {
            dev_warn(xvp->dev,"%s:get paic region fail\n", __func__);
        }
    }

	ret = of_address_to_resource(np, 3, &res);
    if (ret)
    {
        dev_dbg(xvp->dev,"%s:get memory pool region fail\n", __func__);
		return -ENODEV;
    }
	xvp->pmem = res.start;
	xvp->shared_size = resource_size(&res);
    dev_dbg(xvp->dev,"%s,memory pool phy_addr base=0x%llx,size:0x%x\n", __func__,
		    xvp->pmem, xvp->shared_size);
	return xrp_init_private_pool(&xvp->pool, xvp->pmem,
				     xvp->shared_size);
}

static int xrp_init_regs_v1(struct platform_device *pdev, struct xvp *xvp,int mem_idx)
{
	struct resource *mem;
	struct resource r;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx);
	if (!mem)
		return -ENODEV;

	if (resource_size(mem) < 2 * PAGE_SIZE) {
		dev_err(xvp->dev,
			"%s: shared memory size is too small\n",
			__func__);
		return -ENOMEM;
	}

	xvp->comm_phys = mem->start;
	xvp->pmem = mem->start + PAGE_SIZE;
	xvp->shared_size = resource_size(mem) - PAGE_SIZE;

	r = *mem;
	r.end = r.start + PAGE_SIZE;
	xvp->comm = devm_ioremap_resource(&pdev->dev, &r);
	return xrp_init_private_pool(&xvp->pool, xvp->pmem,
				     xvp->shared_size);
}

static bool xrp_translate_base_mimo_to_dsp(struct xvp *xvp)
{

    if(!xvp->hw_ops->get_base_mimo || !xvp->hw_ops->get_hw_sync_data )
	{
		return true;
	}


	phys_addr_t mimo_addr = xvp->hw_ops->get_base_mimo(xvp->hw_arg);
   
	u32 device_mimo_addr  = xrp_translate_to_dsp(&xvp->address_map, mimo_addr);
	if(device_mimo_addr==XRP_NO_TRANSLATION)
	{
		dev_err(xvp->dev,
			"%s: 0x%x translate to dsp address fail\n",
			__func__,mimo_addr);
		return false;
	}
	xvp->hw_ops->update_device_base(xvp->hw_arg,device_mimo_addr);
	dev_dbg(xvp->dev,
	"%s: Base mimo translate to dsp address \n",__func__);
	return true;

}

static int xrp_init_regs_cma(struct platform_device *pdev, struct xvp *xvp,int mem_idx)
{
	dma_addr_t comm_phys;

	if (of_reserved_mem_device_init(xvp->dev) < 0)
		return -ENODEV;

	xvp->comm = dma_alloc_attrs(xvp->dev, PAGE_SIZE, &comm_phys,
				    GFP_KERNEL, 0);
	if (!xvp->comm)
		return -ENOMEM;

	xvp->comm_phys = dma_to_phys(xvp->dev, comm_phys);
	return xrp_init_cma_pool(&xvp->pool, xvp->dev);
}

static int compare_queue_priority(const void *a, const void *b)
{
	const void * const *ppa = a;
	const void * const *ppb = b;
	const struct xrp_comm *pa = *ppa, *pb = *ppb;

	if (pa->priority == pb->priority)
		return 0;
	else
		return pa->priority < pb->priority ? -1 : 1;
}

static long xrp_init_common(struct platform_device *pdev,
			    enum xrp_init_flags init_flags,
			    const struct xrp_hw_ops *hw_ops, void *hw_arg,
				int mem_idx,
			    int (*xrp_init_regs)(struct platform_device *pdev,
						 struct xvp *xvp,int mem_idx))
{
	long ret;
	char nodename[sizeof("xvp") + 3 * sizeof(int)];
	struct xvp *xvp;
	int nodeid;
	unsigned i;
    u32 value;
    char dir_name[32];
	xvp = devm_kzalloc(&pdev->dev, sizeof(*xvp), GFP_KERNEL);
	if (!xvp) {
		ret = -ENOMEM;
		goto err;
	}
    xvp->reporter = NULL;
	xvp->dev = &pdev->dev;
	xvp->hw_ops = hw_ops;
	xvp->hw_arg = hw_arg;
	if (init_flags & XRP_INIT_USE_HOST_IRQ)
		xvp->host_irq_mode = true;
	platform_set_drvdata(pdev, xvp);

	ret = xrp_init_regs(pdev, xvp,mem_idx);
	if (ret < 0)
		goto err;

	dev_dbg(xvp->dev,"%s: comm = %pap/%p\n", __func__, &xvp->comm_phys, xvp->comm);
	dev_dbg(xvp->dev,"%s: xvp->pmem = %pap\n", __func__, &xvp->pmem);
	// writel(0xdeadbeef,xvp->comm+0x4);
    // value = readl(xvp->comm+0x4);
	// pr_debug("offset=04, value is:0x%08x\n",value);

	ret = xrp_init_address_map(xvp->dev, &xvp->address_map);
	if (ret < 0)
		goto err_free_pool;

	if(false ==xrp_translate_base_mimo_to_dsp(xvp))
	{
		goto err_free_map;
	}
	ret = device_property_read_u32_array(xvp->dev, "queue-priority",
					     NULL, 0);
	if (ret > 0) {
		xvp->n_queues = ret;
		xvp->queue_priority = devm_kmalloc(&pdev->dev,
						   ret * sizeof(u32),
						   GFP_KERNEL);
		if (xvp->queue_priority == NULL)
			goto err_free_pool;
		ret = device_property_read_u32_array(xvp->dev,
						     "queue-priority",
						     xvp->queue_priority,
						     xvp->n_queues);
		if (ret < 0)
			goto err_free_pool;
		dev_dbg(xvp->dev,
			"multiqueue (%d) configuration, queue priorities:\n",
			xvp->n_queues);
		for (i = 0; i < xvp->n_queues; ++i)
			dev_dbg(xvp->dev, "  %d\n", xvp->queue_priority[i]);
	} else {
		xvp->n_queues = 1;
	}
	xvp->queue = devm_kmalloc(&pdev->dev,
				  xvp->n_queues * sizeof(*xvp->queue),
				  GFP_KERNEL);
	xvp->queue_ordered = devm_kmalloc(&pdev->dev,
					  xvp->n_queues * sizeof(*xvp->queue_ordered),
					  GFP_KERNEL);
	if (xvp->queue == NULL ||
	    xvp->queue_ordered == NULL)
		goto err_free_pool;

	for (i = 0; i < xvp->n_queues; ++i) {
		mutex_init(&xvp->queue[i].lock);
		xvp->queue[i].comm = xvp->comm + XRP_DSP_CMD_STRIDE * i;
		init_completion(&xvp->queue[i].completion);
		if (xvp->queue_priority)
			xvp->queue[i].priority = xvp->queue_priority[i];
		xvp->queue_ordered[i] = xvp->queue + i;
	}
	sort(xvp->queue_ordered, xvp->n_queues, sizeof(*xvp->queue_ordered),
	     compare_queue_priority, NULL);
	if (xvp->n_queues > 1) {
		dev_dbg(xvp->dev, "SW -> HW queue priority mapping:\n");
		for (i = 0; i < xvp->n_queues; ++i) {
			dev_dbg(xvp->dev, "  %d -> %d\n",
				i, xvp->queue_ordered[i]->priority);
		}
	}

	ret = device_property_read_string(xvp->dev, "firmware-name",
					  &xvp->firmware_name);
	if (ret == -EINVAL || ret == -ENODATA) {
		dev_dbg(xvp->dev,
			"no firmware-name property, not loading firmware\n");
	} else if (ret < 0) {
		dev_err(xvp->dev, "invalid firmware name (%ld)\n", ret);
		goto err_free_map;
	}

  	nodeid = ida_simple_get(&xvp_nodeid, 0, 0, GFP_KERNEL);
	if (nodeid < 0) {
		ret = nodeid;
		goto err_free_map;
	}

    sprintf(dir_name,"dsp%d_proc",nodeid);
    xvp->proc_dir = proc_mkdir(dir_name, NULL);
    if (NULL != xvp->proc_dir)
    {
        xvp->panic_log = xrp_create_panic_log_proc(xvp->proc_dir,xvp->panic,xvp->panic_size);
    }
    else
    {
        dev_err(xvp->dev, "create %s fail\n", dir_name);
        goto err_free_id;
    }
	pm_runtime_enable(xvp->dev);
	if (!pm_runtime_enabled(xvp->dev)) {
		ret = xrp_runtime_resume(xvp->dev);
		if (ret)
			goto err_pm_disable;
	}else
    {
        ret = xrp_runtime_resume(xvp->dev);
		if (ret)
			goto err_proc_remove;
        // xvp_enable_dsp(xvp);
        xrp_runtime_suspend(xvp->dev);
    }

	xvp->nodeid = nodeid;
	sprintf(nodename, "xvp%u", nodeid);

	xvp->miscdev = (struct miscdevice){
		.minor = MISC_DYNAMIC_MINOR,
		.name = devm_kstrdup(&pdev->dev, nodename, GFP_KERNEL),
		.nodename = devm_kstrdup(&pdev->dev, nodename, GFP_KERNEL),
		.fops = &xvp_fops,
	};

	ret = misc_register(&xvp->miscdev);
	if (ret < 0)
		goto err_pm_disable;
    // xrp_device_heartbeat_init(xvp);
    
    INIT_LIST_HEAD(&xvp->dma_buf_list);


	return PTR_ERR(xvp);


err_pm_disable:
	pm_runtime_disable(xvp->dev);
err_proc_remove:
    xvp_remove_proc(xvp);
err_free_id:
	ida_simple_remove(&xvp_nodeid, nodeid);
err_free_map:
	xrp_free_address_map(&xvp->address_map);
err_free_pool:
	xrp_free_pool(xvp->pool);
	if (xvp->comm_phys && !xvp->pmem) {
		dma_free_attrs(xvp->dev, PAGE_SIZE, xvp->comm,
			       phys_to_dma(xvp->dev, xvp->comm_phys), 0);
	}
err:
	dev_err(&pdev->dev, "%s: ret = %ld\n", __func__, ret);
	return ret;
}

typedef long xrp_init_function(struct platform_device *pdev,
			       enum xrp_init_flags flags,
			       const struct xrp_hw_ops *hw_ops, void *hw_arg,int mem_idx);

xrp_init_function xrp_init;
long xrp_init(struct platform_device *pdev, enum xrp_init_flags flags,
	      const struct xrp_hw_ops *hw_ops, void *hw_arg,int mem_idx)
{
	return xrp_init_common(pdev, flags, hw_ops, hw_arg, mem_idx,xrp_init_regs_v0);
}
EXPORT_SYMBOL(xrp_init);

xrp_init_function xrp_init_v1;
long xrp_init_v1(struct platform_device *pdev, enum xrp_init_flags flags,
		 const struct xrp_hw_ops *hw_ops, void *hw_arg,int mem_idx)
{
	return xrp_init_common(pdev, flags, hw_ops, hw_arg, mem_idx,xrp_init_regs_v1);
}
EXPORT_SYMBOL(xrp_init_v1);

xrp_init_function xrp_init_cma;
long xrp_init_cma(struct platform_device *pdev, enum xrp_init_flags flags,
		  const struct xrp_hw_ops *hw_ops, void *hw_arg,int mem_idx)
{
	return xrp_init_common(pdev, flags, hw_ops, hw_arg, mem_idx,xrp_init_regs_cma);
}
EXPORT_SYMBOL(xrp_init_cma);

int xrp_deinit(struct platform_device *pdev)
{
	struct xvp *xvp = platform_get_drvdata(pdev);

	pm_runtime_disable(xvp->dev);
	if (!pm_runtime_status_suspended(xvp->dev))
		xrp_runtime_suspend(xvp->dev);
    // xvp_clear_dsp(xvp);
    xvp_remove_proc(xvp);
	dev_dbg(xvp->dev,"%s:phase 1\n",__func__);
	misc_deregister(&xvp->miscdev);
	dev_dbg(xvp->dev,"%s:phase 2\n",__func__);
	// release_firmware(xvp->firmware);
	// dev_dbg(xvp->dev,"%s:phase 3\n",__func__);
	xrp_free_pool(xvp->pool);
	if (xvp->comm_phys && !xvp->pmem) {
		dma_free_attrs(xvp->dev, PAGE_SIZE, xvp->comm,
			       phys_to_dma(xvp->dev, xvp->comm_phys), 0);
	}
	dev_dbg(xvp->dev,"%s:phase 3\n",__func__);
	xrp_free_address_map(&xvp->address_map);
	dev_dbg(xvp->dev,"%s:phase 4\n",__func__);
	if(!ida_is_empty(&xvp_nodeid))
	{
		ida_simple_remove(&xvp_nodeid, xvp->nodeid);
		dev_dbg(xvp->dev,"%s:phase 5\n",__func__);
	}

	return 0;
}
EXPORT_SYMBOL(xrp_deinit);

int xrp_deinit_hw(struct platform_device *pdev, void **hw_arg)
{
	if (hw_arg) {
		struct xvp *xvp = platform_get_drvdata(pdev);
		*hw_arg = xvp->hw_arg;
	}
	return xrp_deinit(pdev);
}
EXPORT_SYMBOL(xrp_deinit_hw);
static void *get_hw_sync_data(void *hw_arg, size_t *sz)
{
	void *p = kzalloc(64, GFP_KERNEL);

	*sz = 64;
	return p;
}

static const struct xrp_hw_ops hw_ops = {
	.get_hw_sync_data = get_hw_sync_data,
};

#ifdef CONFIG_OF
static const struct of_device_id xrp_of_match[] = {
	{
		.compatible = "cdns,xrp",
		.data = xrp_init,
	}, {
		.compatible = "cdns,xrp,v1",
		.data = xrp_init_v1,
	}, {
		.compatible = "cdns,xrp,cma",
		.data = xrp_init_cma,
	}, {},
};
MODULE_DEVICE_TABLE(of, xrp_of_match);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id xrp_acpi_match[] = {
	{ "CXRP0001", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, xrp_acpi_match);
#endif

static int xrp_probe(struct platform_device *pdev)
{
	long ret = -EINVAL;

#ifdef CONFIG_OF
	const struct of_device_id *match;

	match = of_match_device(xrp_of_match, &pdev->dev);
	if (match) {
		xrp_init_function *init = match->data;

		ret = init(pdev, 0, &hw_ops, NULL,0);
		return IS_ERR_VALUE(ret) ? ret : 0;
	} else {
		pr_debug("%s: no OF device match found\n", __func__);
	}
#endif
#ifdef CONFIG_ACPI
	ret = xrp_init_v1(pdev, 0, &hw_ops, NULL,2);
	if (!IS_ERR_VALUE(ret)) {
		struct xrp_address_map_entry *entry;
		struct xvp *xvp = ERR_PTR(ret);

		ret = 0;
		/*
		 * On ACPI system DSP can currently only access
		 * its own shared memory.
		 */
		entry = xrp_get_address_mapping(&xvp->address_map,
						xvp->comm_phys);
		if (entry) {
			entry->src_addr = xvp->comm_phys;
			entry->dst_addr = (u32)xvp->comm_phys;
			entry->size = (u32)xvp->shared_size + PAGE_SIZE;
		} else {
			dev_err(xvp->dev,
				"%s: couldn't find mapping for shared memory\n",
				__func__);
			ret = -EINVAL;
		}
	}
#endif
	return ret;
}

static int xrp_remove(struct platform_device *pdev)
{
	return xrp_deinit(pdev);
}

static const struct dev_pm_ops xrp_pm_ops = {
	SET_RUNTIME_PM_OPS(xrp_runtime_suspend,
			   xrp_runtime_resume, NULL)
};

static struct platform_driver xrp_driver = {
	.probe   = xrp_probe,
	.remove  = xrp_remove,
	.driver  = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(xrp_of_match),
		.acpi_match_table = ACPI_PTR(xrp_acpi_match),
		.pm = &xrp_pm_ops,
	},
};

module_platform_driver(xrp_driver);

MODULE_AUTHOR("T-HEAD");
MODULE_DESCRIPTION("XRP: Linux device driver for Xtensa Remote Processing");
MODULE_LICENSE("Dual MIT/GPL");
