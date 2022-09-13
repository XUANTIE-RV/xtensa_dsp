/*
 * Copyright (c) 2018 Cadence Design Systems Inc.
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

#ifndef _XRP_REPORT_H
#define _XRP_REPORT_H


struct xrp_report_entry {
	struct xrp_report_entry * next;
};
struct xrp_report_item {

	struct xrp_report_entry entry;
	int report_id;
	
	void *buf;

	int  size;

	void (*fn)( void *context,void * data);
	void * context;

};

struct xrp_report_list{

	struct {
		struct xrp_report_entry *head;
	} queue;

};



extern void xrp_process_report(struct xrp_report_list *list,void* data,unsigned int id);
extern int xrp_add_report(struct xrp_report_list *list,
					struct xrp_report_item *item);
extern int xrp_remove_report(struct xrp_report_list *list,int id);
extern int xrp_alloc_report_id(struct xrp_report_list *list);
extern struct xrp_report_item* xrp_get_report_entry(struct xrp_report_list *list,int id);
#endif
