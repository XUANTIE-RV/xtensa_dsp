/*

 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include "xrp_debug.h"
#include "xrp_report.h"
#include "xrp_thread_impl.h"
#include "dsp_common.h"



// void xrp_reporter_init(struct xrp_report_list *list)
// {

//    	g_list=list;
// 	g_list->queue.head=NULL;
// }
struct xrp_report_item* xrp_get_report_entry(struct xrp_report_list *list,int id)
{
	struct xrp_report_entry *cur_entry=list->queue.head;
	for(;cur_entry!=NULL;cur_entry=cur_entry->next)
	{
		if(((struct xrp_report_item *)cur_entry)->report_id == id)
		{
			
			return (struct xrp_report_item*)cur_entry;
		}
	}
	return NULL;
}

void xrp_process_report(struct xrp_report_list *list,void* data,unsigned int id)
{
	struct xrp_report_item* report_item= xrp_get_report_entry(list,id);
	if(!report_item)
	{
		DSP_PRINT(WARNING,"No valid report item by id (%d)\n",id);
		return;
	}
	if(!report_item->fn ||
		(report_item->size&&!report_item->buf)){
		return;
	}
	memcpy(report_item->buf,data,report_item->size);
    int *ptr=report_item->buf;
  
	report_item->fn(report_item->context,report_item->buf);
}

int xrp_add_report(struct xrp_report_list *list,
					struct xrp_report_item *item)
{
	// xrp_cond_lock(&queue->request_queue_cond);
	struct xrp_report_entry *entry_head=list->queue.head;
    struct xrp_report_entry *entry_cur=NULL;
	struct xrp_report_item *new_item;

	new_item = malloc(sizeof(struct xrp_report_item));
	if(!new_item)
	{	
		return -1;
	}
	memcpy(new_item,item,sizeof(struct xrp_report_item));
	new_item->entry.next=NULL;
	if(NULL==entry_head)
	{
		list->queue.head= &new_item->entry;
		return 0;
	}
	for(;entry_head!=NULL;entry_head=entry_head->next)
	{	
        entry_cur = entry_head;
		if(((struct xrp_report_item *)entry_head)->report_id == new_item->report_id)
		{
			DSP_PRINT(WARNING,"the report is already exist\n");
			return -1;
		}

	}
	entry_cur->next=&new_item->entry;
    DSP_PRINT(INFO,"add new report item %d\n",new_item->report_id);
	return 0;
	// xrp_cond_unlock(&queue->request_queue_cond);
}

int xrp_remove_report(struct xrp_report_list *list,int id)
{
	struct xrp_report_entry *pre_entry=NULL;
	struct xrp_report_entry *cur_entry=list->queue.head;
	for(;cur_entry!=NULL;pre_entry=cur_entry,cur_entry=cur_entry->next)
	{
		if(((struct xrp_report_item *)cur_entry)->report_id == id)
		{
			if(pre_entry==NULL)
			{
				list->queue.head = cur_entry->next;

			}
			else{
				pre_entry->next=cur_entry->next;
			}			
			free(cur_entry);
			return 0;
		}
	}
	return -1;
}


int xrp_alloc_report_id(struct xrp_report_list *list)
{
	int new_id;
	int retry=0;
	while(1)
	{
		new_id= rand()&0x7fffffff;
		struct xrp_report_entry *cur_entry=list->queue.head;
		for(;cur_entry!=NULL;cur_entry=cur_entry->next)
		{
			if(((struct xrp_report_item *)cur_entry)->report_id == new_id)
			{			
				retry++;
				if(retry>10)
				{
					DSP_PRINT(WARNING,"alloc report id fail");
					return -1;
				}
				break;
			}
		}
		return new_id;
		
	}

}