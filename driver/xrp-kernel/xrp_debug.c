/*
 * xrp_debug: 
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
#include <asm/cacheflush.h>
#include "xrp_kernel_defs.h"
#include "xrp_hw.h"
#include "xrp_hw_simple_dsp_interface.h"

#define GET_PAGE_NUM(size, offset)     ((((size) + ((offset) & ~PAGE_MASK)) + PAGE_SIZE - 1) >> PAGE_SHIFT)
struct xrp_panic_log{

    struct xrp_hw_panic __iomem *panic;
    phys_addr_t panic_phys;
    u32 last_read;
    struct proc_dir_entry *log_proc_file;
};
static void memset_hw(void __iomem *dst, int c, size_t sz)
{
   	int i;
	volatile u32 * d_ptr=dst;
	for(i=0;i<sz/4;i++)
	{
		__raw_writel(c, d_ptr++);

	}
}
static void dump_regs(const char *fn, void *hw_arg)
{
	struct xrp_panic_log *panic_log = hw_arg;

	if (!panic_log->panic)
		return;

	pr_debug("%s: panic = 0x%08x, ccount = 0x%08x\n",
		 fn,
		 __raw_readl(&panic_log->panic->panic),
		 __raw_readl(&panic_log->panic->ccount));
	pr_debug("%s: read = 0x%08x, write = 0x%08x, size = 0x%08x\n",
		 fn,
		 __raw_readl(&panic_log->panic->rb.read),
		 __raw_readl(&panic_log->panic->rb.write),
		 __raw_readl(&panic_log->panic->rb.size));
}

static void dump_log_page(struct xrp_panic_log *hw)
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
	struct xrp_panic_log *hw = file->private;
    char *buf;
   	size_t i; 
    int page_num = GET_PAGE_NUM(hw->panic->rb.size,0);
    dump_regs(__func__, hw);
    buf = kmalloc(PAGE_SIZE*page_num, GFP_KERNEL);
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

void* xrp_create_panic_log_proc(void* dir,void * panic_addr,size_t size)
{
    struct xrp_panic_log *panic_log = kmalloc(sizeof(struct xrp_panic_log),GFP_KERNEL);

    if(panic_log == NULL)
        return NULL;

    panic_log->last_read = 0;
    panic_log->panic = panic_addr;
    xrp_panic_init(panic_log->panic,size);

    panic_log->log_proc_file=proc_create_single_data("dsp_log",0644,dir,&log_proc_show,panic_log);
    if(panic_log->log_proc_file == NULL) {
        pr_debug("Error: Could not initialize %s\n","dsp_log");
        kfree(panic_log);
        panic_log =NULL;
    } else {

        pr_debug("%s create Success!\n","dsp_log");
    }   
    return panic_log; 
}

void xrp_remove_panic_log_proc(void *arg)
{
    // char file_name[32];
    struct xrp_panic_log *panic_log = arg;
    // remove_proc_entry(panic_log->log_proc_file,NULL);

    proc_remove(panic_log->log_proc_file);
    kfree(arg);
    pr_debug("dsp proc removed\n");
}

bool panic_check(void *arg)
{
	struct xrp_panic_log *panic_log = arg;
	uint32_t panic;
	uint32_t ccount;
	uint32_t read;
	uint32_t write;
	uint32_t size;

	if (!panic_log || !panic_log->panic)
		return true;

	panic = __raw_readl(&panic_log->panic->panic);
	ccount = __raw_readl(&panic_log->panic->ccount);
	read = __raw_readl(&panic_log->panic->rb.read);
	write = __raw_readl(&panic_log->panic->rb.write);
	size = __raw_readl(&panic_log->panic->rb.size);

	if (read == 0 && read != panic_log->last_read) {
		pr_debug( "****************** device restarted >>>>>>>>>>>>>>>>>\n");
		dump_log_page(panic_log);
		pr_debug ("<<<<<<<<<<<<<<<<<< device restarted *****************\n");
	}
	if (write < size && read < size && size < PAGE_SIZE) {
		uint32_t tail;
		uint32_t total;
		char *buf = NULL;

		panic_log->last_read = read;
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
					      panic_log->panic->rb.data + read,
					      tail);
				read = 0;
				off += tail;
				tail = total - tail;
			}
			__raw_writel(write, &panic_log->panic->rb.read);
			panic_log->last_read = write;
			pr_debug("<<<\n%.*s\n>>>\n",
				 total, buf);
			kfree(buf);
		} else if (total) {
			pr_debug(
				"%s: couldn't allocate memory (%d) to read the dump\n",
				__func__, total);
		}
	} else {
		if (read != panic_log->last_read) {
			pr_debug(
				 "nonsense in the log buffer: read = %d, write = %d, size = %d\n",
				 read, write, size);
			panic_log->last_read = read;
		}
	}
	if (panic == 0xdeadbabe) {
		pr_debug("%s: panic detected, log dump:\n", __func__);
		dump_log_page(panic_log);
	}

	return panic == 0xdeadbabe;
}

