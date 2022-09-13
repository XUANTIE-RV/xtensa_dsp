#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include "../include/xrp_api.h"
#include "../include/dsp_ps_ns.h"
#include "../include/csi_dsp_api.h"
#include "csi_dsp_core.h"
#include "csi_dsp_task_defs.h"
#include "dsp_common.h"
// #include "../xrp-host/xrp_host_common.h"
struct csi_dsp_instance *instance = NULL;
void csi_dsp_heartbeak_polling()
{
     dsp_handler_item_t *task_item =NULL;
     struct itimerval val,oval;
     struct csi_dsp_task_handler *task;
    //  static counter =0;

    DSP_PRINT(INFO,"heartbeat checking\n");
     if(instance == NULL)
     {
         DSP_PRINT(WARNING,"dsp instance is NULL\n");
         return;
     }
    // counter++;
     /*close timer*/
     val.it_value.tv_sec = 0;
     val.it_value.tv_usec =0;
     val.it_interval.tv_sec = 0;
     val.it_interval.tv_usec =0;
     setitimer(ITIMER_REAL,&val,&oval);

     if(csi_dsp_cmd_send(instance->comm_queue,PS_CMD_HEART_BEAT_REQ,NULL,0,NULL,0,NULL))
     {
            DSP_PRINT(WARNING,"PS_CMD_TASK_ALLOC fail\n");
            s_cmd_t cmd = 
            {
                .cmd=CSI_DSP_REPORT_HEARTBEAT_ERR,
            };
            list_for_each_entry(task_item,&instance->task_list,head)
            {
                task = task_item->handler;
                if(task->mode & CSI_DSP_TASK_HW)
                {
                    if(task->cb)
                        task->cb(task->context,&cmd);
                }
            }
     }
    /*restore  timer*/
     setitimer(ITIMER_REAL,&oval,NULL);
     return;
}


int csi_dsp_enable_heartbeat_check(struct csi_dsp_instance *dsp ,int secs)
{
    struct itimerval val,oval;
    signal(SIGALRM,csi_dsp_heartbeak_polling);
    val.it_value.tv_sec = secs;
    val.it_value.tv_usec =0;
    val.it_interval.tv_sec = secs;
    val.it_interval.tv_usec =0;
    instance = dsp;
    DSP_PRINT(DEBUG,"period:%d sec hearbeat working\n",secs);
    return setitimer(ITIMER_REAL,&val,&oval);

}

int csi_dsp_disable_heartbeat_check()
{
    struct itimerval val;
    val.it_value.tv_sec = 0;
    val.it_value.tv_usec =0;
    val.it_interval.tv_sec = 0;
    val.it_interval.tv_usec =0;
    instance = NULL;
   DSP_PRINT(INFO,"sec hearbeat disable\n");
    return setitimer(ITIMER_REAL,&val,NULL);

}
void isp_algo_result_handler(void *context,void *data)
{
    s_cmd_t *msg=(s_cmd_t *)data;
    printf("report recived:%x\n",msg->cmd);
    switch(msg->cmd)
    {
        case CSI_DSP_REPORT_ISP_ERR:
                printf("ISP error:%d\n",msg->data[0]);
            break;
        case CSI_DSP_REPORT_RY_ERR:
                printf("Post ISP error\n",msg->data[0]);
            break;
        case CSI_DSP_REPORT_ALGO_ERR:
                printf("algo err\n");
            break;
        case CSI_DSP_REPORT_VI_PRE_ERR:
            break;
        case  CSI_DSP_REPORT_RESULT:
            break;
        case CSI_DSP_REPORT_HEARTBEAT_ERR:
                 printf("heartbeat not detect\n");
            break;
        default:
            break;
           
    }

}


int csi_dsp_buf_flush( struct xrp_device *device,struct csi_dsp_buffer *buffers)
{
    int loop;
    enum xrp_status status;
    int flag = buffers->dir == CSI_DSP_BUFFER_IN ?XRP_READ:XRP_WRITE;
    if(buffers->type == CSI_DSP_BUF_TYPE_DMA_BUF_IMPORT ||
          buffers->type == CSI_DSP_BUF_TYPE_DMA_BUF_IMPORT  )
    {
        for(loop=0;loop<buffers->plane_count;loop++)
        {
            xrp_flush_dma_buf(device, buffers->planes[loop].fd,flag,&status);
        }
        
    }

}

// void hw_task_result_handler(void *context,void *data)
// {
//     s_cmd_t *msg=(s_cmd_t *)data;
//     struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)context;
//     printf("report recived:%x\n",msg->cmd);
//     switch(msg->cmd)
//     {
//         case VI_SYS_REPORT_ISP_ERR:
//                 printf("ISP error:%d\n",msg->data[0]);
//                 break;
//         case VI_SYS_REPORT_RY_ERR:
//                 printf("Post ISP error\n",msg->data[0]);
//                 break;
//         case VI_SYS_REPORT_ALGO_ERR:
//                 printf("algo err\n");
//                 break;
//         case VI_SYS_REPORT_RESULT:
//             if(task->cb)
//             {
//                 task->cb(task->context,)
//             }
//             break;
//         default:
//             break;      
//     }
// }