/*
 * Copyright (c) 2016 - 2018 Cadence Design Systems Inc.
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
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include "xrp_types.h"
#include "xrp_host_common.h"
#include "xrp_host_impl.h"
#include "xrp_kernel_defs.h"
#include "xrp_report.h"
#include "dsp_common.h"
struct xrp_request {
	struct xrp_queue_item q;
	void *in_data;
	void *out_data;
	size_t in_data_size;
	size_t out_data_size;
	struct xrp_buffer_group *buffer_group;
	struct xrp_event *event;
};

/* Device API. */

struct xrp_device *xrp_open_device(int idx, enum xrp_status *status)
{
	struct xrp_device *device;
	char name[sizeof("/dev/xvp") + sizeof(int) * 4];
	int fd;

	sprintf(name, "/dev/xvp%u", idx);
	fd = open(name, O_RDWR);
	if (fd == -1) {
		set_status(status, XRP_STATUS_FAILURE);
		return NULL;
	}
	device = alloc_refcounted(sizeof(*device));
	if (!device) {
		set_status(status, XRP_STATUS_FAILURE);
		return NULL;
	}
	device->impl.fd = fd;
	set_status(status, XRP_STATUS_SUCCESS);
	return device;
}

void xrp_impl_release_device(struct xrp_device *device)
{
	close(device->impl.fd);
}


/* Buffer API. */

void xrp_impl_create_device_buffer(struct xrp_device *device,
				   struct xrp_buffer *buffer,
				   size_t size,
				   enum xrp_status *status)
{
	struct xrp_ioctl_alloc ioctl_alloc = {
		.size = size,
	};
	int ret;

	xrp_retain_device(device);
	buffer->device = device;
	ret = ioctl(buffer->device->impl.fd, XRP_IOCTL_ALLOC, &ioctl_alloc);
	if (ret < 0) {
		xrp_release_device(buffer->device);
		set_status(status, XRP_STATUS_FAILURE);
		return;
	}
	buffer->ptr = (void *)(uintptr_t)ioctl_alloc.addr;
	buffer->size = size;
    buffer->phy_addr = ioctl_alloc.paddr;
	set_status(status, XRP_STATUS_SUCCESS);
}

void xrp_impl_release_device_buffer(struct xrp_buffer *buffer)
{
	struct xrp_ioctl_alloc ioctl_alloc = {
		.addr = (uintptr_t)buffer->ptr,
	};
	ioctl(buffer->device->impl.fd,
	      XRP_IOCTL_FREE, &ioctl_alloc);

	xrp_release_device(buffer->device);
}

/* Queue API. */

static void _xrp_run_command(struct xrp_queue *queue,
			     const void *in_data, size_t in_data_size,
			     void *out_data, size_t out_data_size,
			     struct xrp_buffer_group *buffer_group,
			     enum xrp_status *status)
{
	int ret;

	if (buffer_group)
		xrp_mutex_lock(&buffer_group->mutex);
	{
		size_t n_buffers = buffer_group ? buffer_group->n_buffers : 0;
		struct xrp_ioctl_buffer ioctl_buffer[n_buffers];/* TODO */
		struct xrp_ioctl_queue ioctl_queue = {
			.flags = (queue->use_nsid ? XRP_QUEUE_FLAG_NSID : 0) |
				((queue->priority << XRP_QUEUE_FLAG_PRIO_SHIFT) &
				 XRP_QUEUE_FLAG_PRIO),
			.in_data_size = in_data_size,
			.out_data_size = out_data_size,
			.buffer_size = n_buffers *
				sizeof(struct xrp_ioctl_buffer),
			.in_data_addr = (uintptr_t)in_data,
			.out_data_addr = (uintptr_t)out_data,
			.buffer_addr = (uintptr_t)ioctl_buffer,
			.nsid_addr = (uintptr_t)queue->nsid,
		};
		size_t i;

		for (i = 0; i < n_buffers; ++i) {
			ioctl_buffer[i] = (struct xrp_ioctl_buffer){
				.flags = buffer_group->buffer[i].access_flags,
				.size = buffer_group->buffer[i].buffer->size,
				.addr = (uintptr_t)buffer_group->buffer[i].buffer->ptr,
			};
		}
		if (buffer_group)
			xrp_mutex_unlock(&buffer_group->mutex);

		ret = ioctl(queue->device->impl.fd,
			    XRP_IOCTL_QUEUE, &ioctl_queue);
	}
    // printf("%s, user out data\n",__func__);
	if (ret < 0)
		set_status(status, XRP_STATUS_FAILURE);
	else
		set_status(status, XRP_STATUS_SUCCESS);
}

static void xrp_request_process(struct xrp_queue_item *q,
				void *context)
{
	enum xrp_status status;
	struct xrp_request *rq = (struct xrp_request *)q;

	_xrp_run_command(context,
			 rq->in_data, rq->in_data_size,
			 rq->out_data, rq->out_data_size,
			 rq->buffer_group,
			 &status);

	if (rq->buffer_group)
		xrp_release_buffer_group(rq->buffer_group);

	if (rq->event) {
		xrp_impl_broadcast_event(rq->event, status);
		xrp_release_event(rq->event);
	}
	// printf("%s,get resp with event %p!\n",__func__,rq->event);
	free(rq->in_data);
	free(rq);
}

void xrp_impl_create_queue(struct xrp_queue *queue,
			   enum xrp_status *status)
{
	xrp_queue_init(&queue->impl.queue, queue->priority,
		       queue, xrp_request_process);
	set_status(status, XRP_STATUS_SUCCESS);
}

void xrp_impl_release_queue(struct xrp_queue *queue)
{
	xrp_queue_destroy(&queue->impl.queue);
}

/* Communication API */

void xrp_enqueue_command(struct xrp_queue *queue,
			 const void *in_data, size_t in_data_size,
			 void *out_data, size_t out_data_size,
			 struct xrp_buffer_group *buffer_group,
			 struct xrp_event **evt,
			 enum xrp_status *status)
{
	struct xrp_request *rq;
	void *in_data_copy;

	rq = malloc(sizeof(*rq));
	in_data_copy = malloc(in_data_size);

	if (!rq || (in_data_size && !in_data_copy)) {
		free(in_data_copy);
		free(rq);
		set_status(status, XRP_STATUS_FAILURE);
		return;
	}

	memcpy(in_data_copy, in_data, in_data_size);
	rq->in_data = in_data_copy;
	rq->in_data_size = in_data_size;
	rq->out_data = out_data;
	rq->out_data_size = out_data_size;

	if (evt) {
		struct xrp_event *event = xrp_event_create();

		if (!event) {
			free(rq->in_data);
			free(rq);
			set_status(status, XRP_STATUS_FAILURE);
			return;
		}
		xrp_retain_queue(queue);
		event->queue = queue;
		*evt = event;
		xrp_retain_event(event);
		rq->event = event;
	} else {
		rq->event = NULL;
	}

	if (buffer_group)
		xrp_retain_buffer_group(buffer_group);
	rq->buffer_group = buffer_group;

	set_status(status, XRP_STATUS_SUCCESS);
	xrp_queue_push(&queue->impl.queue, &rq->q);
}

static struct xrp_report *reporter;

void xrp_reporter_sig_handler()
{
	// printf("%s\n",__func__);
	struct xrp_report_buffer *report_buffer;
	if(!reporter || !reporter->report_buf)
	{
		return;
	}
	report_buffer = (struct xrp_report_buffer *)reporter->report_buf;
	// printf("buffer:%lx,id:%d,data:%x,%x,%x,%x\n",report_buffer,report_buffer->report_id,report_buffer->data[0],report_buffer->data[1],report_buffer->data[2],report_buffer->data[3]);

	xrp_process_report(&reporter->list,report_buffer->data,report_buffer->report_id);
}


int xrp_add_report_item_with_id(struct xrp_report *report,
								int (*cb)(void*context,void*data),
								int report_id,
								void* context,
								size_t data_size)
{
	// int id;
	// // set_status(status, XRP_STATUS_SUCCESS);
	// id =xrp_alloc_report_id(&report->list);
	if(report_id<0 || !report)
	{
		// set_status(status, XRP_STATUS_FAILURE);
		return -1;
	}
	if(data_size>report->buf_size)
	{
		//realloc()
		DSP_PRINT(WARNING,"report instance size %d is exceed limit %d\n",data_size,report->buf_size);
		// set_status(status, XRP_STATUS_FAILURE);
		return -1;
	}
	char *report_buf = malloc(data_size);
	if(!report_buf)
	{
		// set_status(status, XRP_STATUS_FAILURE);
		return -1;
	}
	struct xrp_report_item new_item={
	     .report_id = report_id,
		 .buf = report_buf,
		 .size = data_size,
		 .context = context,
		 .fn = cb,
	};

	DSP_PRINT(DEBUG,"add report id:%d\n", report_id);
	if(xrp_add_report(&report->list,&new_item))
	{
		free(report_buf);
		return -1;
	}
	DSP_PRINT(INFO,"the report item: %d is added sucessfully\n",report_id);
	return report_id;

}
int xrp_add_report_item(struct xrp_report *report,
								int (*cb)(void*context,void*data),
								void* context,
								size_t data_size)
{
	int id;
	id =xrp_alloc_report_id(&report->list);
	if(id<0)
	{
		return -1;
	}
	return xrp_add_report_item_with_id(report,cb,id,context,data_size);
}




void xrp_remove_report_item(struct xrp_report *report,int report_id)
{
	int id;
	struct xrp_report_item* item=xrp_get_report_entry(&report->list,report_id);
	free(item->buf);
	xrp_remove_report(&report->list,report_id);
}

void xrp_impl_create_report(struct xrp_device *device,
				struct xrp_report *report,
				size_t size,
			    enum xrp_status *status)
{
	// char *report_buf = malloc(size+4);
	// if(!report_buf)
	// {
	// 	set_status(status, XRP_STATUS_FAILURE);
	// 	return;
	// }
	
	struct xrp_ioctl_alloc ioctl_alloc = {
		.addr = (uintptr_t)NULL,
		.size = size,
	};
	int ret;

	xrp_retain_device(device);
	report->device = device;
	ret = ioctl(report->device->impl.fd, XRP_IOCTL_REPORT_CREATE, &ioctl_alloc);
	if (ret < 0) {
		// free(report_buf);
		set_status(status, XRP_STATUS_FAILURE);
		return;
	}
	if(ioctl_alloc.addr ==NULL)
	{
		DSP_PRINT(INFO,"alloc report buffer fail\n");
        set_status(status, XRP_STATUS_FAILURE);
        return ;
	}
	report->report_buf = (void *)(uintptr_t)ioctl_alloc.addr;
	// printf("buf:%lx,report:x\n",ioctl_alloc.addr,report);
	report->buf_size = size;
	report->list.queue.head=NULL;
	reporter=report;
	signal(SIGIO, xrp_reporter_sig_handler); /* sigaction() is better */
	fcntl(report->device->impl.fd, F_SETOWN, getpid());
	int oflags = fcntl(report->device->impl.fd, F_GETFL);
	fcntl(report->device->impl.fd, F_SETFL, oflags | FASYNC);
	set_status(status, XRP_STATUS_SUCCESS);
	DSP_PRINT(INFO,"buf:%lx,user space report create\n",ioctl_alloc.addr);
}
void xrp_impl_release_report(struct xrp_device *device,
				struct xrp_report *report,enum xrp_status *status)
{
	struct xrp_ioctl_alloc ioctl_alloc = {
		.addr = (uintptr_t)report->report_buf,
		.size = report->buf_size,
	};
	int ret = ioctl(report->device->impl.fd, XRP_IOCTL_REPORT_RELEASE, &ioctl_alloc);
	if (ret < 0) {
		// free(report_buf);
		set_status(status, XRP_STATUS_FAILURE);
		return;
	}
	report->report_buf=NULL;
    xrp_release_device(device);
	set_status(status, XRP_STATUS_SUCCESS);
	return;
}

void xrp_import_dma_buf(struct xrp_device *device, int fd,enum xrp_access_flags flag,uint64_t *phy_addr,
                                uint64_t *user_addr,size_t* size,enum xrp_status *status)
{
    	struct xrp_dma_buf dma_buf;

        if(fd < 0 || !(flag&XRP_FLAG_READ_WRITE))
        {
            set_status(status, XRP_STATUS_FAILURE);
            DSP_PRINT(DEBUG,"param check fail\n");
            return;
        }
        dma_buf.fd = fd;
        dma_buf.flags = flag;
        int ret = ioctl(device->impl.fd, XRP_IOCTL_DMABUF_IMPORT,&dma_buf);

        if (ret < 0) {
            DSP_PRINT(DEBUG,"_DMABUF_IMPORT fail\n");
            set_status(status, XRP_STATUS_FAILURE);
	    }
        else
        {
            *phy_addr = dma_buf.paddr;
            *user_addr = dma_buf.addr;
            *size = dma_buf.size;
            set_status(status, XRP_STATUS_SUCCESS);
        }
        return;
}

void xrp_release_dma_buf(struct xrp_device *device, int fd,enum xrp_status *status)
{
        if(fd < 0)
        {
            set_status(status, XRP_STATUS_FAILURE);
            return;
        }
        int ret = ioctl(device->impl.fd, XRP_IOCTL_DMABUF_RELEASE,&fd);

        if (ret < 0) {
            set_status(status, XRP_STATUS_FAILURE);
	    }
        else
        {
            set_status(status, XRP_STATUS_SUCCESS);
	    }
        return ;

}

void xrp_flush_dma_buf(struct xrp_device *device, int fd,enum xrp_access_flags flag,enum xrp_status *status)
{
        struct xrp_dma_buf dma_buf;
        dma_buf.fd = fd;
        dma_buf.flags = flag;
        if(fd < 0)
        {
            set_status(status, XRP_STATUS_FAILURE);
            return;
        }
        
        int ret = ioctl(device->impl.fd, XRP_IOCTL_DMABUF_SYNC,&dma_buf);

        if (ret < 0) {
            set_status(status, XRP_STATUS_FAILURE);
	    }
        else
        {
            set_status(status, XRP_STATUS_SUCCESS);
	    }
}