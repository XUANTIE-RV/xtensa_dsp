/*
 * xrp_hw_simple: Simple xtensa/arm low-level XRP driver
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

#include <linux/delay.h>
// #include <linux/dma-noncoherent.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#include "xrp_kernel_defs.h"
#include "xrp_hw.h"
#include "xrp_hw_simple_dsp_interface.h"

#define DRIVER_NAME "xrp-hw-simple"

#define XRP_REG_RESET		(0x28)
#define RESET_BIT_MASK      (0x1<<8)

#define CDNS_DSP_RRG_OFFSET  (0x4000)
#define XRP_REG_RUNSTALL	(CDNS_DSP_RRG_OFFSET+0x20)
#define START_VECTOR_SEL    (CDNS_DSP_RRG_OFFSET+0x1C)
#define ALT_RESET_VEC      (CDNS_DSP_RRG_OFFSET+0x18)
// #define DSP_INT_MASK    (0x1<<1)
// #define INT_DSP_MASK (0x1<<1)
#define VI_SYS_OFFSET_MASK   (0x00000FFF)

#ifdef WITH_VISYS_KO
extern int k_bm_visys_write_reg(uint32_t offset, uint32_t value);
extern int k_bm_visys_read_reg(uint32_t offset, uint32_t *value);
#endif
enum xrp_irq_mode {
	XRP_IRQ_NONE,
	XRP_IRQ_LEVEL,
	XRP_IRQ_EDGE,
	XRP_IRQ_EDGE_SW,
	XRP_IRQ_MAX,
};

struct xrp_hw_simple {
	struct xvp *xrp;
    phys_addr_t dev_regs_phys;
	void __iomem *dev_regs;
	/*  IRQ register base phy address on device side */
	phys_addr_t irq_regs_dev_phys;
	/*  Device IRQ register phy base addr on host side */
	phys_addr_t device_irq_regs_phys;
	/*  Device IRQ register virtual base  addr on host side */
	void __iomem *device_irq_regs;
	/*  host IRQ register phy base addr on host side */
	phys_addr_t host_irq_regs_phys;
	/*  host IRQ register virtual base addr on host side */
	void __iomem *host_irq_regs;
	/* how IRQ is used to notify the device of incoming data */
	enum xrp_irq_mode device_irq_mode;
	/*
	 * offset of device IRQ register in MMIO region (device side)
	 * bit number
	 * device IRQ#
	 */
	u32 device_irq[3];
	/* offset of devuce IRQ register in MMIO region (host side) */
	u32 device_irq_host_offset;
	/* how IRQ is used to notify the host of incoming data */
	enum xrp_irq_mode host_irq_mode;
	/*
	 * offset of IRQ register (host side)
	 * bit number
	 */
	u32 host_irq[2];
	/*
	   offset of IRQ register  (device side to trigger)
	*/
	u32 host_irq_offset;
	u32 device_id;

    struct xrp_hw_panic __iomem *panic;
    phys_addr_t panic_phys;
    u32 last_read;
    
    struct proc_dir_entry *log_proc_file;
    struct clk *cclk;
    // struct clk *aclk;
    struct clk *pclk;
};

// static inline void irq_reg_write32(struct xrp_hw_simple *hw, unsigned addr, u32 v)
// {
// 	if (hw->irq_regs)
// 	   // pr_debug("%s,irq Addr %llx\n",__func__,(unsigned long long)(hw->regs + addr));
// 		__raw_writel(v, hw->irq_regs + addr);
// }

static inline void host_irq_reg_write32(struct xrp_hw_simple *hw, unsigned int addr, u32 v)
{
    if( IOMEM_ERR_PTR(-EBUSY) ==hw->host_irq_regs )
    {
        #ifdef WITH_VISYS_KO
        uint32_t offset = ((uint32_t)(hw->host_irq_regs_phys&VI_SYS_OFFSET_MASK)+addr);
        // pr_debug("%s,vi sys write (%x,%d)\n",__func__,offset,v);
        k_bm_visys_write_reg(offset, v);
        #else
            pr_debug("%s,vi sys Error,need enable VISYS KO\n",__func__);
        #endif
        return;
    }
	if (hw->host_irq_regs)
	    pr_debug("%s,irq Addr %llx\n",__func__,(unsigned long long)(hw->host_irq_regs + addr));
		__raw_writel(v, hw->host_irq_regs + addr);
}

static inline void device_irq_reg_write32(struct xrp_hw_simple *hw, unsigned int addr, u32 v)
{
	if( IOMEM_ERR_PTR(-EBUSY) ==hw->device_irq_regs )
    {
        #ifdef WITH_VISYS_KO
        uint32_t offset =  ((uint32_t)(hw->device_irq_regs_phys&VI_SYS_OFFSET_MASK)+addr);
        pr_debug("%s,vi sys write (%lx,%x,0x%x,%d)\n",__func__,hw->device_irq_regs_phys,addr,offset,v);
        k_bm_visys_write_reg(offset, v);
        #else
            pr_debug("%s,vi sys Error,need enable VISYS KO\n",__func__);
        #endif
        return;
    }
    if (hw->device_irq_regs)
	    pr_debug("%s,irq Addr %llx\n",__func__,(unsigned long long)(hw->device_irq_regs + addr));
		__raw_writel(v, hw->device_irq_regs + addr);
}

// static inline u32 irq_reg_read32(struct xrp_hw_simple *hw, unsigned addr)
// {
// 	if (hw->irq_regs)
// 		return __raw_readl(hw->irq_regs + addr);
// 	else
// 		return 0;
// }

static inline u32 host_irq_reg_read32(struct xrp_hw_simple *hw, unsigned addr)
{
    if( IOMEM_ERR_PTR(-EBUSY) ==hw->host_irq_regs )
    {

        uint32_t offset = ((uint32_t)(hw->host_irq_regs_phys&VI_SYS_OFFSET_MASK)+(uint32_t)addr);
        uint32_t v=0;
        #ifdef WITH_VISYS_KO
        k_bm_visys_read_reg(offset,&v);
        // pr_debug("%s,vi sys read (%x,%d)\n",__func__,offset,v);
        #else
            pr_err("%s,vi sys Error,need enable VISYS KO\n",__func__);
        #endif
        return v;

    }
       
    if (hw->host_irq_regs)
		return __raw_readl(hw->host_irq_regs + addr);
	else
		return 0;
}

static inline u32 device_irq_reg_read32(struct xrp_hw_simple *hw, unsigned addr)
{
    if( IOMEM_ERR_PTR(-EBUSY) ==hw->device_irq_regs )
    {

        uint32_t offset =  ((uint32_t)(hw->device_irq_regs_phys&VI_SYS_OFFSET_MASK)+(uint32_t)addr);
        uint32_t v = 0;
        #ifdef WITH_VISYS_KO
        k_bm_visys_read_reg(offset,&v);
        // pr_debug("%s,vi sys write (%x,%d)\n",__func__,offset,v);
        #else
            pr_error("%s,vi sys Error,need enable VISYS KO\n",__func__);
        #endif
        return v;

    }

    if (hw->device_irq_regs)
		return __raw_readl(hw->device_irq_regs + addr);
	else
		return 0;
}

static inline void dev_reg_write32(struct xrp_hw_simple *hw, unsigned addr, u32 v)
{
	if (hw->dev_regs)

		//pr_debug("%s,write to dev Addr %p,value:%x\n",__func__,(hw->dev_regs + addr),v);
		__raw_writel(v, hw->dev_regs + addr);
}

static inline u32 dev_reg_read32(struct xrp_hw_simple *hw, unsigned addr)
{
	if (hw->dev_regs)
		return __raw_readl(hw->dev_regs + addr);
	else
		return 0;
}

static void dump_regs(const char *fn, void *hw_arg)
{
	struct xrp_hw_simple *hw = hw_arg;

	if (!hw->panic)
		return;

	pr_debug("%s: panic = 0x%08x, ccount = 0x%08x\n",
		 fn,
		 __raw_readl(&hw->panic->panic),
		 __raw_readl(&hw->panic->ccount));
	pr_debug("%s: read = 0x%08x, write = 0x%08x, size = 0x%08x\n",
		 fn,
		 __raw_readl(&hw->panic->rb.read),
		 __raw_readl(&hw->panic->rb.write),
		 __raw_readl(&hw->panic->rb.size));
}

static void dump_log_page(struct xrp_hw_simple *hw)
{
	char *buf;
	size_t i;

	if (!hw->panic)
		return;

	dump_regs(__func__, hw);
	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (buf) {
		memcpy_fromio(buf, hw->panic, hw->panic->rb.size);
        buf+=sizeof(struct xrp_hw_panic);
		for (i = 0; i < hw->panic->rb.size; i += 64)
			pr_debug("  %*pEhp\n", 64, buf + i);
		kfree(buf);
	} else {
		pr_debug("(couldn't allocate copy buffer)\n");
	}
}

static int log_proc_show(struct seq_file *file, void *v)
{
	struct xrp_hw_simple *hw = file->private;
    char *buf;
   	size_t i; 
    dump_regs(__func__, hw);
    buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (buf) {
		memcpy_fromio(buf, hw->panic->rb.data, hw->panic->rb.size);
        seq_printf(file,"****************** device log >>>>>>>>>>>>>>>>>\n");
        for (i = 0; i < hw->panic->rb.size; i += 64)
            seq_printf(file," %*pEp", 64,buf+i);
			// pr_debug("  %*pEhp\n", 64, buf + i);
        // seq_printf(file," %*pEp\n",buf);
        kfree(buf);

        uint32_t	write = __raw_readl(&hw->panic->rb.write);
        __raw_writel(write, &hw->panic->rb.read);

        return 0;
    }
    else
    {
        pr_debug("Fail to alloc buf\n");
        return -1;
    }
    return 0;
}

static int log_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, log_proc_show, NULL);
}

static const struct file_operations log_proc_fops = {
        .open      = log_proc_open,
        .read      = seq_read,
        .llseek    = seq_lseek,
        .release   = single_release,
};

int xrp_hw_create_log_proc(struct xrp_hw_simple *hw)
{
    int rv = 0;
    char file_name[32];
    sprintf(file_name,"dsp%d_proc",hw->device_id);
    // hw->log_proc_file = create_proc_entry(file_name,0644,NULL);
    hw->log_proc_file = proc_mkdir(file_name, NULL);
    if (NULL == hw->log_proc_file )
    {
        pr_debug("Error: Could not create dir\n");
        return -ENODEV;
    }

    // hw->log_proc_file = proc_create_data(file_name,0644,S_IFREG | S_IRUGO,&log_proc_fops,hw);
    hw->log_proc_file=proc_create_single_data("dsp_log",0644,hw->log_proc_file,&log_proc_show,hw);

    if(hw->log_proc_file == NULL) {
        rv = -ENOMEM;
        pr_debug("Error: Could not initialize %s\n","dsp_log");
    } else {

        pr_debug("%s create Success!\n","dsp_log");
    }   
    return rv; 
}

void xrp_hw_remove_log_proc(void *hw_arg)
{
    char file_name[32];
    struct xrp_hw_simple *hw = hw_arg;
    sprintf(file_name,"dsp%d_proc",hw->device_id);
    remove_proc_entry(file_name,NULL);
    // proc_remove(hw->log_proc_file);
    pr_debug("%s,proc removed\n",file_name);
}

// int xrp_hw_log_read(char *buffer,char** buffer_location,off_t offset,
//                 int buffer_length,int *eof,void *data)
// {
//     int len = 0;
//     struct xrp_hw_simple *hw = data;

//     if(offset > 0) {
//         printk(KERN_INFO "offset %d: /proc/test1: profile_read,\
//             wrote %d Bytes\n",(int)(offset),len);
//         *eof = 1;
//         return len;
//     }
//     //填充buffer并获取其长度
//     len = sprintf(buffer,
//              "For the %d %s time,go away!\n",count,
//               (count % 100 > 10 && count % 100 < 14)?"th":
//               (count % 10 == 1)?"st":
//               (count % 10 == 2)?"nd":
//               (count % 10 == 3)?"rd":"th");
//     count++;
//     printk(KERN_INFO "leasving /proc/test1: profile_read,wrote %d Bytes\n",len);
//     return len;
// }

static void *get_hw_sync_data(void *hw_arg, size_t *sz)
{
	static const u32 irq_mode[] = {
		[XRP_IRQ_NONE] = XRP_DSP_SYNC_IRQ_MODE_NONE,
		[XRP_IRQ_LEVEL] = XRP_DSP_SYNC_IRQ_MODE_LEVEL,
		[XRP_IRQ_EDGE] = XRP_DSP_SYNC_IRQ_MODE_EDGE,
		[XRP_IRQ_EDGE_SW] = XRP_DSP_SYNC_IRQ_MODE_EDGE,
	};
	struct xrp_hw_simple *hw = hw_arg;
	struct xrp_hw_simple_sync_data *hw_sync_data =
		kmalloc(sizeof(*hw_sync_data), GFP_KERNEL);

	if (!hw_sync_data)
		return NULL;
	u32 device_host_offset=0;
	u32 host_device_offset=0;
	if(hw->device_irq_regs_phys > hw->host_irq_regs_phys)
	{
		device_host_offset = hw->device_irq_regs_phys-hw->host_irq_regs_phys;
	}
	else
	{
		host_device_offset = hw->host_irq_regs_phys-hw->device_irq_regs_phys;
	}
	*hw_sync_data = (struct xrp_hw_simple_sync_data){
		.device_mmio_base = hw->irq_regs_dev_phys,
		.host_irq_mode = hw->host_irq_mode,
		.host_irq_offset = hw->host_irq_offset+host_device_offset,
		.host_irq_bit = hw->host_irq[1],
		.device_irq_mode = irq_mode[hw->device_irq_mode],
		.device_irq_offset = hw->device_irq[0]+device_host_offset,
		.device_irq_bit = hw->device_irq[1],
		.device_irq = hw->device_irq[2],
        // .panic_base = hw->panic_phys,
	};
	*sz = sizeof(*hw_sync_data);
	return hw_sync_data;
}
 
static void reset(void *hw_arg)
{
	// dev_reg_write32(hw_arg, XRP_REG_RESET, (dev_reg_read32(hw_arg, XRP_REG_RESET))^RESET_BIT_MASK);
	// udelay(10000);
	// dev_reg_write32(hw_arg, XRP_REG_RESET, (dev_reg_read32(hw_arg, XRP_REG_RESET))^RESET_BIT_MASK);
	struct xrp_hw_simple *hw = hw_arg;
	xrp_set_reset_reg(hw->device_id);
}

static void halt(void *hw_arg)
{
	dev_reg_write32(hw_arg, XRP_REG_RUNSTALL, 1);

	pr_debug("%s: halt value:%x\n",__func__,dev_reg_read32(hw_arg, XRP_REG_RUNSTALL));
	// dump_log_page(hw_arg);
}

static void set_reset_vector(void *hw_arg,u32 addr)
{
	struct xrp_hw_simple *hw = hw_arg;
	// if(hw->device_id ==0)
	// {
	// 	addr = 0x80000000;
	// }
	// else{
	// 	addr = 0x70000000;
	// }
	addr = addr&0xffffff00;
	dev_reg_write32(hw_arg, ALT_RESET_VEC, addr);
	pr_debug("%s: reset_vector:%x\n",__func__,dev_reg_read32(hw_arg, ALT_RESET_VEC));
}
static void release(void *hw_arg)
{
	dev_reg_write32(hw_arg, XRP_REG_RUNSTALL, 0);
}

static void send_irq(void *hw_arg)
{
	struct xrp_hw_simple *hw = hw_arg;
	pr_debug("%s: Enter\n",__func__);
	switch (hw->device_irq_mode) {
	case XRP_IRQ_EDGE_SW:
		device_irq_reg_write32(hw, hw->device_irq_host_offset,
			    BIT(hw->device_irq[1]));
		while ((device_irq_reg_read32(hw, hw->device_irq_host_offset) &
			BIT(hw->device_irq[1])))
			mb();
		break;
	case XRP_IRQ_EDGE:
		device_irq_reg_write32(hw, hw->device_irq_host_offset, 0);
		/* fallthrough */
	case XRP_IRQ_LEVEL:
		wmb();
		device_irq_reg_write32(hw, hw->device_irq_host_offset,
			    BIT(hw->device_irq[1]));

		break;
	default:
		break;
	}
}
static int enable(void *hw_arg)
{
	struct xrp_hw_simple *hw = hw_arg;
    int ret;
    ret = clk_prepare_enable(hw->cclk);
	if (ret < 0) {
		pr_err("could not prepare or enable core clock\n");
		return ret;
	}

	// ret = clk_prepare_enable(hw->aclk);
	// if (ret < 0) {
	// 	pr_err("could not prepare or enable axi clock\n");
	// 	clk_disable_unprepare(hw->cclk);
	// 	return ret;
	// }

	ret = clk_prepare_enable(hw->pclk);
	if (ret < 0) {
		pr_err("could not prepare or enable apb clock\n");
		clk_disable_unprepare(hw->cclk);
		// clk_disable_unprepare(hw->aclk);
		return ret;
	}
    pr_debug("%s: enable dsp\n",__func__);
    return ret;
}

static void disable(void *hw_arg)
{
	struct xrp_hw_simple *hw = hw_arg;
 	clk_disable_unprepare(hw->pclk);
	// clk_disable_unprepare(hw->aclk);
	clk_disable_unprepare(hw->cclk);
    pr_debug("%s: disable dsp\n",__func__);
    return;
}
static inline void ack_irq(void *hw_arg)
{
	struct xrp_hw_simple *hw = hw_arg;

	if (hw->host_irq_mode == XRP_IRQ_LEVEL)
		host_irq_reg_write32(hw, hw->host_irq[0], BIT(hw->host_irq[1]));
		//__raw_writel(DSP_INT_MASK,0xFFE4040190);  //DSP0
}
static inline bool is_expect_irq(struct xrp_hw_simple *hw)
{
		return host_irq_reg_read32(hw,hw->host_irq_offset)&BIT(hw->host_irq[1]);
}
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	irqreturn_t ret=IRQ_NONE;
	struct xrp_hw_simple *hw = dev_id;

	if(is_expect_irq(hw))
	{
		ret = xrp_irq_handler(irq, hw->xrp);

		if (ret == IRQ_HANDLED)
			ack_irq(hw);

	}
	else{
		pr_err("%s: unexpect irq，%x\n",__func__,host_irq_reg_read32(hw,hw->host_irq_offset));
	}
	

	return ret;
}

phys_addr_t get_irq_base_mimo(void *hw_arg)
{
	struct xrp_hw_simple *hw = hw_arg;
	pr_debug("%s: dev_regs\n",__func__);
	return hw->device_irq_regs_phys < hw->host_irq_regs_phys?hw->device_irq_regs_phys:hw->host_irq_regs_phys;
}

void  update_device_base(void *hw_arg,phys_addr_t addr)
{
	struct xrp_hw_simple *hw = hw_arg;
	 hw->irq_regs_dev_phys = addr;
	 pr_debug("%s:dev_regs，%p\n",__func__,hw->irq_regs_dev_phys);
}
void memcpy_tohw(volatile void __iomem *dst, const void *src, size_t sz)
{
	
	int i;
	u32 *s_ptr = src;
	volatile u32 * d_ptr=dst;
    pr_debug("%s,dst:0x%llx,src:0x%llx,size:%d",__FUNCTION__,dst,src,sz);
    udelay(10000);
	for(i=0;i<sz/4;i++)
	{
		__raw_writel(s_ptr[i], d_ptr++);

	}
}

void memcpy_toio_local(volatile void __iomem *to, const void *from, size_t count)
{
	while (count && !IS_ALIGNED((unsigned long)to, 8)) {
		__raw_writeb(*(u8 *)from, to);
		from++;
		to++;
		count--;
	}

	while (count >= 8) {
		__raw_writeq(*(u64 *)from, to);
		from += 8;
		to += 8;
		count -= 8;
	}

	while (count) {
		__raw_writeb(*(u8 *)from, to);
		from++;
		to++;
		count--;
	}
}

void memset_hw_local(volatile void __iomem *dst, int c, size_t count)
{
	u64 qc = (u8)c;

	qc |= qc << 8;
	qc |= qc << 16;
	qc |= qc << 32;
	while (count && !IS_ALIGNED((unsigned long)dst, 8)) {
		__raw_writeb(c, dst);
		dst++;
		count--;
	}

	while (count >= 8) {
		__raw_writeq(qc, dst);
		dst += 8;
		count -= 8;
	}

	while (count) {
		__raw_writeb(c, dst);
		dst++;
		count--;
	}
}
void memset_hw(void __iomem *dst, int c, size_t sz)
{
   	int i;
	volatile u32 * d_ptr=dst;
	for(i=0;i<sz/4;i++)
	{
		__raw_writel(c, d_ptr++);

	}
}

#if defined(__XTENSA__)
static bool cacheable(void *hw_arg, unsigned long pfn, unsigned long n_pages)
{
	return true;
}

static void dma_sync_for_device(void *hw_arg,
				void *vaddr, phys_addr_t paddr,
				unsigned long sz, unsigned flags)
{
	switch (flags) {
	case XRP_FLAG_READ:
		__flush_dcache_range((unsigned long)vaddr, sz);
		break;

	case XRP_FLAG_READ_WRITE:
		__flush_dcache_range((unsigned long)vaddr, sz);
		__invalidate_dcache_range((unsigned long)vaddr, sz);
		break;

	case XRP_FLAG_WRITE:
		__invalidate_dcache_range((unsigned long)vaddr, sz);
		break;
	}
}

static void dma_sync_for_cpu(void *hw_arg,
			     void *vaddr, phys_addr_t paddr,
			     unsigned long sz, unsigned flags)
{
	switch (flags) {
	case XRP_FLAG_READ_WRITE:
	case XRP_FLAG_WRITE:
		__invalidate_dcache_range((unsigned long)vaddr, sz);
		break;
	}
}

#elif defined(__arm__)
static bool cacheable(void *hw_arg, unsigned long pfn, unsigned long n_pages)
{
	return true;
}

static void dma_sync_for_device(void *hw_arg,
				void *vaddr, phys_addr_t paddr,
				unsigned long sz, unsigned flags)
{
	switch (flags) {
	case XRP_FLAG_READ:
		__cpuc_flush_dcache_area(vaddr, sz);
		outer_clean_range(paddr, paddr + sz);
		break;

	case XRP_FLAG_WRITE:
		__cpuc_flush_dcache_area(vaddr, sz);
		outer_inv_range(paddr, paddr + sz);
		break;

	case XRP_FLAG_READ_WRITE:
		__cpuc_flush_dcache_area(vaddr, sz);
		outer_flush_range(paddr, paddr + sz);
		break;
	}
}

static void dma_sync_for_cpu(void *hw_arg,
			     void *vaddr, phys_addr_t paddr,
			     unsigned long sz, unsigned flags)
{
	switch (flags) {
	case XRP_FLAG_WRITE:
	case XRP_FLAG_READ_WRITE:
		__cpuc_flush_dcache_area(vaddr, sz);
		outer_inv_range(paddr, paddr + sz);
		break;
	}
}
#else
static bool cacheable(void *hw_arg, unsigned long pfn, unsigned long n_pages)
{
	return true;
}

// static void dma_sync_for_device(void *hw_arg,
// 				void *vaddr, phys_addr_t paddr,
// 				unsigned long sz, unsigned flags)
// {
// 	struct xrp_hw_simple *hw = hw_arg;
// 	switch (flags) {
// 	case XRP_FLAG_READ:
// 	case XRP_FLAG_WRITE:
// 	case XRP_FLAG_READ_WRITE:
// 		arch_sync_dma_for_cpu(hw->xrp->dev, paddr, sz,DMA_TO_DEVICE);
// 		break;
// 	}
// }

// static void dma_sync_for_cpu(void *hw_arg,
// 			     void *vaddr, phys_addr_t paddr,
// 			     unsigned long sz, unsigned flags)
// {
// 	struct xrp_hw_simple *hw = hw_arg;
// 	switch (flags) {
// 	case XRP_FLAG_WRITE:
// 	case XRP_FLAG_READ_WRITE:
// 		arch_sync_dma_for_cpu(hw->xrp->dev, paddr, sz,DMA_FROM_DEVICE);
// 		break;
// }
// }
#endif

static bool panic_check(void *hw_arg)
{
	struct xrp_hw_simple *hw = hw_arg;
	uint32_t panic;
	uint32_t ccount;
	uint32_t read;
	uint32_t write;
	uint32_t size;

	if (!hw->panic)
		return false;

	panic = __raw_readl(&hw->panic->panic);
	ccount = __raw_readl(&hw->panic->ccount);
	read = __raw_readl(&hw->panic->rb.read);
	write = __raw_readl(&hw->panic->rb.write);
	size = __raw_readl(&hw->panic->rb.size);

	if (read == 0 && read != hw->last_read) {
		pr_debug( "****************** device restarted >>>>>>>>>>>>>>>>>\n");
		dump_log_page(hw);
		pr_debug ("<<<<<<<<<<<<<<<<<< device restarted *****************\n");
	}
	if (write < size && read < size && size < PAGE_SIZE) {
		uint32_t tail;
		uint32_t total;
		char *buf = NULL;

		hw->last_read = read;
		if (read < write) {
			tail = write - read;
			total = tail;
		} else if (read == write) {
			tail = 0;
			total = 0;
		} else {
			tail = size - read;
			total = write + tail;
		}

		if (total)
			buf = kmalloc(total, GFP_KERNEL);

		if (buf) {
			uint32_t off = 0;

			pr_debug("panic = 0x%08x, ccount = 0x%08x read = %d, write = %d, size = %d, total = %d",
				panic, ccount, read, write, size, total);

			while (off != total) {
				memcpy_fromio(buf + off,
					      hw->panic->rb.data + read,
					      tail);
				read = 0;
				off += tail;
				tail = total - tail;
			}
			__raw_writel(write, &hw->panic->rb.read);
			hw->last_read = write;
			pr_debug("<<<\n%.*s\n>>>\n",
				 total, buf);
			kfree(buf);
		} else if (total) {
			pr_debug(
				"%s: couldn't allocate memory (%d) to read the dump\n",
				__func__, total);
		}
	} else {
		if (read != hw->last_read) {
			pr_debug(
				 "nonsense in the log buffer: read = %d, write = %d, size = %d\n",
				 read, write, size);
			hw->last_read = read;
		}
	}
	if (panic == 0xdeadbabe) {
		pr_debug("%s: panic detected, log dump:\n", __func__);
		dump_log_page(hw);
	}

	return panic == 0xdeadbabe;
}


static bool xrp_panic_init(struct xrp_hw_panic* panic,size_t size)
{
	if(size < sizeof(struct xrp_hw_panic))
    {
        return false;
    }
    memset_hw(panic,0x0,size);
    panic->panic = 0;
	panic->ccount = 0;
	panic->rb.read = 0;
	panic->rb.write = 0;
	panic->rb.size = size - sizeof(struct xrp_hw_panic);
    sprintf(panic->rb.data,"Inition dsp log\n");
    return true;
}
static const struct xrp_hw_ops hw_ops = {
	.halt = halt,
	.release = release,
	.reset = reset,
    .enable = enable,
    .disable = disable,
	.get_hw_sync_data = get_hw_sync_data,

	.send_irq = send_irq,
    .get_base_mimo = get_irq_base_mimo,
	.update_device_base = update_device_base,
	.set_reset_vector = set_reset_vector,
	.memcpy_tohw= memcpy_toio_local,
	.memset_hw = memset_hw_local,
    .clear_hw = xrp_hw_remove_log_proc,
#if defined(__XTENSA__) || defined(__arm__)
	.cacheable = cacheable,
	.dma_sync_for_device = dma_sync_for_device,
	.dma_sync_for_cpu = dma_sync_for_cpu,
#endif
};

static long init_hw_irq(struct platform_device *pdev, struct xrp_hw_simple *hw,
		    int mem_idx, enum xrp_init_flags *init_flags)
{
	struct resource *mem;
	int irq;
	long ret;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx++);
	if (!mem) {
		ret = -ENODEV;
		goto err;
	}
	hw->host_irq_regs_phys = mem->start;
	// hw->irq_regs_dev_phys =hw->irq_regs_phys;
	hw->host_irq_regs = devm_ioremap_resource(&pdev->dev, mem);
	pr_debug("%s:host irq regs = %pap/%p\n",
		 __func__, &mem->start, hw->host_irq_regs);


	ret = of_property_read_u32_array(pdev->dev.of_node, "host-irq",
					 hw->host_irq,
					 ARRAY_SIZE(hw->host_irq));
	if (ret == 0) {
		u32 host_irq_mode;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "host-irq-mode",
					   &host_irq_mode);
		if (host_irq_mode < XRP_IRQ_MAX)
			hw->host_irq_mode =host_irq_mode;
		else
			ret = -ENOENT;
		u32 host_irq_offset;
		ret = of_property_read_u32(pdev->dev.of_node,
				"host-irq-offset",
				&host_irq_offset);

		if(ret == 0)
		{
			hw->host_irq_offset=host_irq_offset;
		}

		dev_dbg(&pdev->dev,
			"%s: Host IRQ MMIO: device offset = 0x%08x,host offset = 0x%08x, bit = %d,IRQ mode = %d",
			__func__, hw->host_irq_offset,
			hw->host_irq[0], hw->host_irq[1],
			hw->host_irq_mode);
		
	}
	
	mem = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx);
	if (!mem) {
		ret = -ENODEV;
		goto err;
	}
	hw->device_irq_regs_phys = mem->start;
	// hw->irq_regs_dev_phys =hw->irq_regs_phys;
	hw->device_irq_regs = devm_ioremap_resource(&pdev->dev, mem);
	pr_debug("%s:Device irq regs = %pap/%lx\n",
		 __func__, &mem->start, hw->device_irq_regs_phys);

	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "device-irq",
					 hw->device_irq,
					 ARRAY_SIZE(hw->device_irq));
	if (ret == 0) {
		u32 device_irq_host_offset;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "device-irq-host-offset",
					   &device_irq_host_offset);
		if (ret == 0) {
			hw->device_irq_host_offset = device_irq_host_offset;
		} else {
			hw->device_irq_host_offset = hw->device_irq[0];
			ret = 0;
		}
	}
	if (ret == 0) {
		u32 device_irq_mode;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "device-irq-mode",
					   &device_irq_mode);
		if (device_irq_mode < XRP_IRQ_MAX)
			hw->device_irq_mode = device_irq_mode;
		else
			ret = -ENOENT;
	}
	if (ret == 0) {
		dev_dbg(&pdev->dev,
			"%s: device IRQ MMIO host offset = 0x%08x, offset = 0x%08x, bit = %d, device IRQ = %d, IRQ mode = %d",
			__func__, hw->device_irq_host_offset,
			hw->device_irq[0], hw->device_irq[1],
			hw->device_irq[2], hw->device_irq_mode);
	} else {
		dev_info(&pdev->dev,
			 "using polling mode on the device side\n");
	}


	if (ret == 0 && hw->host_irq_mode != XRP_IRQ_NONE)
		irq = platform_get_irq(pdev, 0);
	else
		irq = -1;

	if (irq >= 0) {
		dev_dbg(&pdev->dev, "%s: host IRQ = %d, ",
			__func__, irq);
		ret = devm_request_irq(&pdev->dev, irq, irq_handler,
				       IRQF_SHARED, pdev->name, hw);
		if (ret < 0) {
			dev_err(&pdev->dev, "request_irq %d failed\n", irq);
			goto err;
		}
		*init_flags |= XRP_INIT_USE_HOST_IRQ;
	} else {
		dev_info(&pdev->dev, "using polling mode on the host side\n");
	}

	ret = 0;
err:
	return ret;
}

static long init_hw_device(struct platform_device *pdev, struct xrp_hw_simple *hw,int mem_idx)
{
	struct resource *mem;

	long ret;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx++);
	if (!mem) {
		ret = -ENODEV;
		goto err;
	}
	hw->dev_regs_phys = mem->start;
	hw->dev_regs = devm_ioremap_resource(&pdev->dev, mem);
	pr_debug("%s: regs = %pap/%p\n",
		 __func__, &mem->start, hw->dev_regs);



    hw->cclk = devm_clk_get(&pdev->dev, "cclk");
    if (hw->cclk==NULL) {
        dev_err(&pdev->dev, "failed to get core clock\n");
        ret = -ENOENT;
        goto err;
    }
    dev_dbg(&pdev->dev, "get core clock\n");
    // hw->aclk = devm_clk_get(&pdev->dev, "aclk");
    // if (hw->aclk==NULL) {
    //     dev_err(&pdev->dev, "failed to get axi clock\n");
    //     ret = -ENOENT;
    //     goto err;
    // }

    hw->pclk  = devm_clk_get(&pdev->dev, "pclk");
    if ( hw->pclk ==NULL) {
        dev_err(&pdev->dev, "failed to get apb clock\n");
        ret = -ENOENT;
        goto err;
    }
    dev_dbg(&pdev->dev, "get apb clock\n");
	u32 device_id;

	ret = of_property_read_u32(pdev->dev.of_node,"dsp",&device_id);
	if(ret ==0 )
	{
		hw->device_id = device_id;
		pr_debug("%s: device_id = %d\n",
		 			__func__,hw->device_id);
	}
	else{
		pr_debug("%s: no device_id \n",__func__);
        ret = -ENODEV;
	}
    // xrp_hw_create_log_proc(hw);

err:
	return ret;
}
static long init(struct platform_device *pdev, struct xrp_hw_simple *hw)
{
	long ret;
	enum xrp_init_flags init_flags = 0;

	ret = init_hw_irq(pdev, hw, 0, &init_flags);
	if (ret < 0)
		return ret;
	ret =init_hw_device(pdev, hw, 2);
	if (ret < 0)
		return ret;
	return xrp_init(pdev, init_flags, &hw_ops, hw,4);
}

static long init_v1(struct platform_device *pdev, struct xrp_hw_simple *hw)
{
	long ret;
	enum xrp_init_flags init_flags = 0;

	ret = init_hw_irq(pdev, hw, 0, &init_flags);
	if (ret < 0)
		return ret;
	ret =init_hw_device(pdev, hw, 1);
	if (ret < 0)
		return ret;
	return xrp_init_v1(pdev, init_flags, &hw_ops, hw,2);
}

static long init_cma(struct platform_device *pdev, struct xrp_hw_simple *hw)
{
	long ret;
	enum xrp_init_flags init_flags = 0;

	ret = init_hw_irq(pdev, hw, 0, &init_flags);
	if (ret < 0)
		return ret;
	ret =init_hw_device(pdev, hw, 1);
	if (ret < 0)
		return ret;
	return xrp_init_cma(pdev, init_flags, &hw_ops, hw,2);
}

#ifdef CONFIG_OF
static const struct of_device_id xrp_hw_simple_match[] = {
	{
		.compatible = "cdns,xrp-hw-simple",
		.data = init,
	}, {
		.compatible = "cdns,xrp-hw-simple,v1",
		.data = init_v1,
	}, {
		.compatible = "cdns,xrp-hw-simple,cma",
		.data = init_cma,
	}, {},
};
// MODULE_DEVICE_TABLE(of, xrp_hw_simple_match);
#endif

static int xrp_hw_simple_probe(struct platform_device *pdev)
{
	struct xrp_hw_simple *hw =
		devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	const struct of_device_id *match;
	long (*init)(struct platform_device *pdev, struct xrp_hw_simple *hw);
	long ret;

	if (!hw)
		return -ENOMEM;

	match = of_match_device(of_match_ptr(xrp_hw_simple_match),
				&pdev->dev);
	if (!match)
		return -ENODEV;

	init = match->data;
	ret = init(pdev, hw);
	if (IS_ERR_VALUE(ret)) {
		//xrp_deinit(pdev);
		pr_debug("init fail\n");
		return ret;
	} else {
		hw->xrp = ERR_PTR(ret);
		return 0;
	}

}


static int xrp_hw_simple_remove(struct platform_device *pdev)
{
	// xrp_hw_remove_log_proc();
    return xrp_deinit(pdev);
}

static const struct dev_pm_ops xrp_hw_simple_pm_ops = {
	SET_RUNTIME_PM_OPS(xrp_runtime_suspend,
			   xrp_runtime_resume, NULL)
};

static struct platform_driver xrp_hw_simple_driver = {
	.probe   = xrp_hw_simple_probe,
	.remove  = xrp_hw_simple_remove,
	.driver  = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(xrp_hw_simple_match),
		.pm = &xrp_hw_simple_pm_ops,
	},
};

module_platform_driver(xrp_hw_simple_driver);

MODULE_AUTHOR("Max Filippov");
MODULE_DESCRIPTION("XRP: low level device driver for Xtensa Remote Processing");
MODULE_LICENSE("Dual MIT/GPL");
