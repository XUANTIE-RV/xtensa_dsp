/*
 * Copyright (c) 2021 Alibaba Group. All rights reserved.
 * License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/xrp_api.h"
#include "../include/dsp_ps_ns.h"
#include "../include/csi_dsp_api.h"
#include "csi_dsp_core.h"
#include "dsp_common.h"
// #include "../xrp-host/xrp_host_common.h"

#define DEV_ID  0

#define DSP_INVALID_ITEM   0xdead
int csi_dsp_cmd_send(const struct xrp_queue *queue,int cmd_type,void * cmd_payload,
                                    size_t payload_size,void *resp, size_t resp_size,struct xrp_buffer_group *buffer_group)
{
    enum xrp_status status;
    size_t cmd_size= payload_size+4;
    s_cmd_t *cmd=(s_cmd_t *)malloc(cmd_size);
    DSP_PRINT(DEBUG,"dsp common cmd send %d\n",cmd_type);
    if(!cmd)
    {
        DSP_PRINT(WARNING,"malloc fail\n",__func__);
        return -1;
    }
    cmd->cmd=cmd_type;
    if(payload_size>0)
    {
        memcpy(cmd->data,cmd_payload,payload_size);
    }

    xrp_run_command_sync(queue,cmd,cmd_size,resp,resp_size,buffer_group,&status);
    if(status!=XRP_STATUS_SUCCESS)
    {
        DSP_PRINT(WARNING,"cmd fail\n",__func__);
        free(cmd);
        return -1;
    }
    free(cmd);
    return 0;

}


/*********************************/
static int dsp_register_report_item_to_dsp(struct csi_dsp_task_handler *task)
{
    int resp =0;
    struct report_config_msg config;
    if(!task || task->report_id<0 || !task->instance || !task->instance->comm_queue)
    {
        DSP_PRINT(ERROR,"param check fail\n");
        return -1;
    }
    config.report_id=task->report_id;
    memcpy(config.task,task->task_ns,TASK_NAME_LINE);
    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_REPORT_CONFIG,&config,sizeof(struct report_config_msg),&resp,sizeof(resp),NULL))
    {
        DSP_PRINT(ERROR,"register_report_item_to_dsp fail\n");
        return -1;
    }
    return 0;
}

// int csi_dsp_register_report_fix_id(struct csi_dsp_task_handler *task,
//                             int (*cb)(void*context,void*data),
//                             void* context,
//                             size_t data_size)
// {
//     if(task->report_id<0)
//     {
//         printf("report id is invalid\n");
//         return -1;
//     }
//     if(xrp_add_report_item_with_id(task->instance->report_impl,
// 								        cb,task->report_id,context,data_size)<0)
//     {
//         return -1;
//     }
//     if(dsp_register_report_item_to_dsp(task))
//     {
//         return -1;
//     }
//     printf("new reprot %d is created and register to DSP\n",task->report_id);
//     return 0;

// }


int csi_dsp_delete_instance(void* dsp)
{   
    if(!dsp)
        return 0;
    // printf("%s,entry\n",__FUNCTION__);
    struct csi_dsp_instance *instance = (struct csi_dsp_instance *)dsp;
    csi_dsp_disable_heartbeat_check();
    xrp_release_queue(instance->comm_queue);
    xrp_release_device(instance->device);
    free(dsp);
    DSP_PRINT(INFO,"exit\n");
    return 0;
}
void* csi_dsp_create_instance(int dsp_id)
{
    enum xrp_status status;
	struct xrp_device *device;
	struct csi_dsp_instance *instance = NULL;
    struct xrp_queue * queue;
    unsigned char ns_id[]=XRP_PS_NSID_COMMON_CMD;
    
    dsp_InitEnv();

    instance=malloc(sizeof(*instance));
    if(!instance)
    {
        DSP_PRINT(ERROR,"malloc fail\n");
        return NULL;
    }
	device = xrp_open_device(dsp_id, &status);
    if(status!=XRP_STATUS_SUCCESS)
    {
       
        free(instance);
        DSP_PRINT(ERROR,"open device\n");
        return NULL;
    }
    instance->device=device;

   /* unsigned char XRP_NSID[] = XRP_PS_NSID_INITIALIZER;
   create a comon queue to handler the common message 
   */
	queue = xrp_create_ns_queue(device, ns_id, &status);
    if(status!=XRP_STATUS_SUCCESS)
    {
        xrp_release_device(device);
        free(instance);
        DSP_PRINT(ERROR,"create ns queue faile\n");
        return NULL;
    }
    instance->comm_queue=queue;
    INIT_LIST_HEAD(&instance->task_list);
    //csi_dsp_enable_heartbeat_check(instance,10);
    DSP_PRINT(INFO,"dsp instance create successulf\n");
    return instance;
}


int csi_dsp_create_reporter(void* dsp)
{
     struct csi_dsp_instance *instance = (struct csi_dsp_instance *)dsp;
     struct xrp_report *reporter = xrp_create_reporter(instance->device,MAX_REPORT_SIZE+32);
     if(reporter==NULL)
     {
         DSP_PRINT(ERROR,"create ns queue\n");
         return -1;
     }
     instance->report_impl=reporter;
     DSP_PRINT(INFO,"create reporter\n");
     return 0;
}

int csi_dsp_destroy_reporter(void *dsp)
{
    struct csi_dsp_instance *instance = (struct csi_dsp_instance *)dsp;
   if(0 == xrp_release_reporter(instance->device,instance->report_impl))
   {
       instance->report_impl=NULL;
   }
   else
   {
       DSP_PRINT(ERROR,"reporter destroy fail\n");
       return -1;
   }
    DSP_PRINT(INFO,"release reporter\n");
    return 0;
}


static void dsp_task_init(struct csi_dsp_task_handler * task)
{
    task->algo.algo_id = DSP_INVALID_ITEM;
    task->algo.task_id = DSP_INVALID_ITEM;
    task->fe.frontend_type = CSI_DSP_FE_TYPE_INVALID;
    task->be.backend_type = CSI_DSP_BE_TYPE_INVALID;
    task->report_id =-1;
} 
/* queue ns 应该通过DSP侧来分配保证唯一性，区分不同进程的task.*/
void *csi_dsp_create_task(void* dsp,csi_dsp_task_mode_e task_type)
{
        struct csi_dsp_task_handler * task;
        struct csi_dsp_task_create_resp resp;
        struct xrp_queue *queue;
        struct csi_dsp_task_create_req config_para;
        enum xrp_status status;
        struct csi_dsp_instance *instance = (struct csi_dsp_instance *)dsp;
        dsp_handler_item_t *task_item =NULL;
        if(!instance)
        {
            DSP_PRINT(ERROR,"param check fail\n");
            return NULL;
        }
        task_item = malloc(sizeof(*task_item));
        if(!task_item)
        {
            DSP_PRINT(ERROR,"malloc fail\n");
            goto error1;
        }
        task= malloc(sizeof(*task));
        if(!task)
        {
            DSP_PRINT(ERROR,"malloc fail\n");
            goto error;
        }
        task_item->handler = task;
        config_para.type=task_type;
        if(csi_dsp_cmd_send(instance->comm_queue,PS_CMD_TASK_ALLOC,&config_para,sizeof(struct csi_dsp_task_create_req),&resp,sizeof(resp),NULL))
        {
            DSP_PRINT(ERROR,"PS_CMD_TASK_ALLOC fail\n");
            goto error;
        }
        if(resp.status!=CSI_DSP_OK)
        {
            DSP_PRINT(ERROR,"task creat fail:%d\n",resp.status);
            goto error;
        }
        task->queue = NULL;
        if(task_type == CSI_DSP_TASK_SW_TO_SW || task_type == CSI_DSP_TASK_SW_TO_HW)
        {

            queue = xrp_create_ns_queue(instance->device, resp.task_ns, &status);
            if(status!=XRP_STATUS_SUCCESS)
            {
                DSP_PRINT(ERROR,"queue creat fail\n");
                goto error;
            }
            task->queue=queue;

            csi_dsp_sw_task_manager_t *sw_task_ctx = malloc(sizeof(*sw_task_ctx));
            if(sw_task_ctx ==NULL)
            {
                DSP_PRINT(ERROR,"malloc fail\n");
                xrp_release_queue(queue);
                goto error;
            }
            INIT_LIST_HEAD(&sw_task_ctx->event_list);
            sw_task_ctx->event_num = 0;
            pthread_mutex_init(&sw_task_ctx->mutex, NULL);
            task->private = sw_task_ctx;
            if(status != XRP_STATUS_SUCCESS)
            {
                DSP_PRINT(ERROR,"xrp_create_buffer_group fail\n");
                free(sw_task_ctx);
                xrp_release_queue(queue);
                goto error;
            }     
        }
        else{
            // HW  task create report
        }
        task->buffers = xrp_create_buffer_group(&status);
        memcpy(task->task_ns,resp.task_ns,TASK_NAME_LINE);
        task->task_id=resp.task_id;
        task->instance=instance;
        task->mode=task_type;
        list_add_tail(&task_item->head,&instance->task_list);
        dsp_task_init(task);
        // task->report_id=-1;
        DSP_PRINT(INFO,"task(%d) ,ns(%x)create successful!\n",task->task_id,task->task_ns[0]);
        return task;

error:
       free(task);
error1:
       free(task_item);
       return NULL;
}
void csi_dsp_destroy_task(void *task_ctx)
{
    csi_dsp_status_e resp;
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    if(task_ctx ==NULL)
    {
        return;
    }
    if(task->queue)
        xrp_release_queue(task->queue);

    if(task->report_id>=0)
    {
        xrp_remove_report_item(task->instance->report_impl,task->report_id);
    }

    if(task->buffers)
    {
        xrp_release_buffer_group(task->buffers);
    }


    struct csi_dsp_task_free_req config_para;

    config_para.task_id = task->task_id;
    memcpy(config_para.task_ns,task->task_ns,sizeof(config_para.task_ns));

    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_TASK_FREE,&config_para,sizeof(struct csi_dsp_task_free_req),&resp,sizeof(resp),NULL))
    {
         DSP_PRINT(ERROR,"send PS_CMD_TASK_FREE fail\n");
    }

    if(resp!=CSI_DSP_OK)
    {
         DSP_PRINT(ERROR,"TASK FREE Fail due to %d\n",resp);
    }
    DSP_PRINT(INFO,"task(%d) ,ns(%x) destroy successful!\n",task->task_id,task->task_ns[0]);
    free(task);
}

int csi_dsp_task_config_frontend(void *task_ctx,struct csi_dsp_task_fe_para* config_para)
{
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    csi_dsp_status_e resp =0;
    if(!task || !task->instance || !task->instance->comm_queue)
    {
        DSP_PRINT(ERROR,"param check fail\n");
        return -1;
    }
    config_para->task_id= task->task_id ;
    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_FE_CONFIG,config_para,sizeof(struct csi_dsp_task_fe_para),&resp,sizeof(resp),NULL))
    {
        DSP_PRINT(ERROR,"config_frontend cmd send fail\n");
        return -1;
    }
    if(resp != CSI_DSP_OK)
    {
        DSP_PRINT(ERROR,"PS_CMD_FE_CONFIG fail %d\n",resp);
        return -1; 
    }
    memcpy(&task->fe,config_para,sizeof(task->fe));
    DSP_PRINT(INFO,"task(%d) set frontend %d!\n",config_para->task_id,config_para->frontend_type);
    return 0;
}

int csi_dsp_task_get_frontend(void *task_ctx,struct csi_dsp_task_fe_para *config_para)
{
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    csi_dsp_status_e resp =0;
    if(!config_para|!task || !task->instance || !task->instance->comm_queue)
    {
        DSP_PRINT(ERROR,"param check fail\n");
        return -1;
    }
    memcpy(config_para,&task->fe,sizeof(task->fe));
    return 0;
}
int csi_dsp_task_config_backend(void *task_ctx,struct csi_dsp_task_be_para* config_para)
{
    csi_dsp_status_e resp =0;
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    size_t sz;
    if(!task || !task->instance || !task->instance->comm_queue)
    {
        DSP_PRINT(ERROR,"param check fail\n");
        return -1;
    }
    config_para->task_id= task->task_id ;
    if(config_para->backend_type == CSI_DSP_BE_TYPE_HOST)
    {
        sz= sizeof(struct csi_dsp_task_be_para)+sizeof(struct csi_dsp_buffer)*config_para->sw_param.num_buf;
    }
    else
    {
        sz= sizeof(struct csi_dsp_task_be_para);
    }
    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_BE_CONFIG, config_para,sz,&resp,sizeof(resp),NULL))
    {
        DSP_PRINT(ERROR,"send cmd fail\n");
        return -1;
    }
    if(resp != CSI_DSP_OK)
    {
        DSP_PRINT(ERROR,"resp ERROR: %d\n",resp);
        return -1; 
    }
    memcpy(&task->be,config_para,sizeof(task->be));
    DSP_PRINT(INFO,"task(%d) set backend %d!\n",task->task_id,config_para->backend_type);
    return 0;
}

int csi_dsp_task_update_backend_buf(void *task_ctx,struct csi_dsp_task_be_para* config_para)
{
    csi_dsp_status_e resp =0;
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    size_t sz;
    if(!task || !task->instance || !task->instance->comm_queue)
    {
        DSP_PRINT(ERROR,"param check fail\n");
        return -1;
    }
    if(config_para->backend_type !=CSI_DSP_BE_TYPE_HOST)
    {
        DSP_PRINT(ERROR,"unsport backend type\n");
        return -1;
    }
    config_para->task_id= task->task_id ;
    if(config_para->backend_type == CSI_DSP_BE_TYPE_HOST)
    {
        sz= sizeof(struct csi_dsp_task_be_para)+sizeof(struct csi_dsp_buffer)*config_para->sw_param.num_buf;
    }
    else
    {
        sz= sizeof(struct csi_dsp_task_be_para);
    }
    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_BE_ASSGIN_BUF, config_para,sz,&resp,sizeof(resp),NULL))
    {
        DSP_PRINT(ERROR,"send cmd fail\n");
        return -1;
    }
    if(resp != CSI_DSP_OK)
    {
        DSP_PRINT(ERROR,"resp ERROR: %d\n",resp);
        return -1; 
    }
    DSP_PRINT(INFO,"task(%d) set backend %d!\n",task->task_id,config_para->backend_type);
    return 0;
}

int csi_dsp_task_get_backend(void *task_ctx,struct csi_dsp_task_be_para* config_para)
{
    csi_dsp_status_e resp =0;
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    if(!task || !task->instance || !task->instance->comm_queue)
    {
        DSP_PRINT(ERROR,"param check fail\n");
        return -1;
    }
    memcpy(config_para,&task->be,sizeof(task->be));
    return 0;
}
int csi_dsp_task_config_algo(void *task_ctx,struct csi_dsp_algo_config_par* config_para)
{
    csi_dsp_status_e resp =0;
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    struct csi_dsp_algo_config_par config;
    struct xrp_buffer *buffer = NULL;
    struct xrp_buffer *algo_buffer = NULL;
    struct xrp_buffer *algo_set_buffer = NULL;
    enum xrp_status status;
    void* buf_ptr;
    struct xrp_buffer_group *buffer_group =NULL;
    if(!task || !task->instance || !task->instance->comm_queue)
    {
        DSP_PRINT(ERROR,"param check fail\n");
        return -1;
    }
    config.task_id= task->task_id ;
    config.algo_id = config_para->algo_id;
    config.algo_ptr = 0;
    config.sett_length = config_para->sett_length;
    config.sett_ptr = 0;
    config.bufs_ptr = 0;
    config.buf_num = config_para->buf_num;
    config.algo_size = config_para->algo_size;
    buffer_group = xrp_create_buffer_group(&status);
    if(status != XRP_STATUS_SUCCESS)
    {
        DSP_PRINT(ERROR,"malloc buf group fail\n");
        return -1;
    }
    if(config_para->sett_ptr && (config_para->sett_length !=0))
    {

        buffer = xrp_create_buffer(task->instance->device,config.sett_length,NULL,&status);
        if(status != XRP_STATUS_SUCCESS)
        {
            DSP_PRINT(ERROR,"malloc buf fail\n");
            goto Err3;
        }
        xrp_add_buffer_to_group(buffer_group,buffer,XRP_READ,&status);
        xrp_buffer_get_info(buffer,XRP_BUFFER_PHY_ADDR,&config.sett_ptr,sizeof(config.sett_ptr),&status);
        xrp_buffer_get_info(buffer,XRP_BUFFER_USER_ADDR,&buf_ptr,sizeof(buf_ptr),&status);
        memcpy(buf_ptr,(void*)config_para->sett_ptr,config_para->sett_length);
        DSP_PRINT(INFO," setting buf phy:%llx\n",config.sett_ptr);

    }

    if(config_para->bufs_ptr && (config_para->buf_num !=0))
    {

        algo_set_buffer = xrp_create_buffer(task->instance->device,config_para->buf_num*sizeof(struct csi_dsp_buffer),NULL,&status);
        if(status != XRP_STATUS_SUCCESS)
        {
            DSP_PRINT(ERROR,"malloc buf fail\n");
            goto Err3;
        }
        xrp_add_buffer_to_group(buffer_group,algo_set_buffer,XRP_READ,&status);
        xrp_buffer_get_info(algo_set_buffer,XRP_BUFFER_PHY_ADDR,&config.bufs_ptr,sizeof(config.sett_ptr),&status);
        xrp_buffer_get_info(algo_set_buffer,XRP_BUFFER_USER_ADDR,&buf_ptr,sizeof(buf_ptr),&status);
        memcpy(buf_ptr,(void*)config_para->bufs_ptr,config_para->buf_num*sizeof(struct csi_dsp_buffer));
        DSP_PRINT(INFO,"  algo_set_buffer phy:%llx\n",config.bufs_ptr);

    }

    if(config_para->algo_ptr && (config_para->algo_size !=0))
    {

        algo_buffer = xrp_create_buffer(task->instance->device,config_para->algo_size,NULL,&status);
        if(status != XRP_STATUS_SUCCESS)
        {
            DSP_PRINT(ERROR,"malloc buf fail\n");
            goto Err2;
        }
        xrp_add_buffer_to_group(buffer_group,algo_buffer,XRP_READ,&status);
        xrp_buffer_get_info(algo_buffer,XRP_BUFFER_PHY_ADDR,&config.algo_ptr,sizeof(config.algo_ptr),&status);
        xrp_buffer_get_info(algo_buffer,XRP_BUFFER_USER_ADDR,&buf_ptr,sizeof(buf_ptr),&status);
        memcpy(buf_ptr,(void*)config_para->algo_ptr,config_para->algo_size);
        DSP_PRINT(INFO,"algo buf phy:%llx\n",config_para->algo_ptr);

    }

    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_ALGO_CONFIG, &config,sizeof(struct csi_dsp_algo_config_par),&resp,sizeof(resp),buffer_group))
    {
        DSP_PRINT(ERROR,"send cmd fail\n");
        goto Err1;
    }

    if(resp != CSI_DSP_OK)
    {
        DSP_PRINT(ERROR,"resp fail %d\n",resp);
        goto Err1;
    }
    memcpy(&task->algo,&config,sizeof(config));

    if(buffer)
    {
        xrp_release_buffer(buffer);
    }
    if(algo_set_buffer)
    {
         xrp_release_buffer(algo_set_buffer);
    }
    if(algo_buffer)
    {
        xrp_release_buffer(algo_buffer);
    }
    xrp_release_buffer_group(buffer_group);

    DSP_PRINT(INFO,"task %d sucessful!\n",task->task_id);
    return 0;

Err1:
    if(algo_buffer) xrp_release_buffer(algo_buffer);
Err2:
    if(buffer) xrp_release_buffer(buffer);
Err3:
    xrp_release_buffer_group(buffer_group);
    return -1;
}

int csi_dsp_task_load_algo(void *task_ctx, csi_dsp_algo_load_req_t* config_para)
{
    csi_dsp_algo_load_resp_t resp;
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    if(!task || !task->instance || !task->instance->comm_queue)
    {
        DSP_PRINT(ERROR,"param check fail\n");
        return -1;
    }
    config_para->task_id= task->task_id;
    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_ALGO_LOAD, config_para,sizeof(csi_dsp_algo_load_req_t),&resp,sizeof(resp),NULL))
    {
        DSP_PRINT(ERROR,"send cmd fail\n");
        return -1;
    }
    if(resp.status != CSI_DSP_OK)
    {
        DSP_PRINT(ERROR,"resp fail %d\n",resp.status);
        return -1; 
    }
    task->algo.algo_id=config_para->algo_id;
    task->algo.algo_ptr = config_para->algo_ptr;
    // task->algo.buf_num = resp.buf_desc_num;
    // task->algo.sett_length = resp. 
    DSP_PRINT(INFO,"task %d load algo sucessful!\n",task->task_id);
    return 0;
}

int csi_dsp_task_acquire_algo(void *task_ctx,char*name)
{
    FILE * fp;
    int   size;
    int rev = 0;
    char file[128];
    struct xrp_buffer *buffer = NULL;
    enum xrp_status status;
    void* buf_ptr;
    uint64_t buf_phy;
    struct xrp_buffer_group *buffer_group =NULL;
    csi_dsp_algo_load_resp_t resp;
    csi_dsp_algo_load_req_t config_para;
    if(task_ctx == NULL || name ==NULL || strlen(name)>100 )
    {
        DSP_PRINT(ERROR,"param check fail\n");
        return -1;
    }

    sprintf(file,"/lib/firmware/%s.lib",name);
    DSP_PRINT(DEBUG,"open file:%s\n",file);
    fp = fopen(file, "rb");
    
    if(fp==NULL)
    {
        DSP_PRINT(ERROR,"open file fail\n");
        return -1;
    }
    fseek(fp , 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    if(size ==0)
    {
        return -1;
    }

    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    buffer_group = xrp_create_buffer_group(&status);
    if(status != XRP_STATUS_SUCCESS)
    {
        DSP_PRINT(ERROR,"malloc buf group fail\n");
        return -1;
    }
    buffer = xrp_create_buffer(task->instance->device,size,NULL,&status);
    if(status != XRP_STATUS_SUCCESS)
    {
       DSP_PRINT(ERROR,"malloc buf fail\n");
        return -1;
    }
    xrp_add_buffer_to_group(buffer_group,buffer,XRP_READ,&status);
    xrp_buffer_get_info(buffer,XRP_BUFFER_PHY_ADDR,&buf_phy,sizeof(buf_phy),&status);
    xrp_buffer_get_info(buffer,XRP_BUFFER_USER_ADDR,&buf_ptr,sizeof(buf_ptr),&status);
    DSP_PRINT(DEBUG,"algo buf virtual:0x%p, phy:0x%lx\n", buf_ptr,buf_phy);

    rev = fread(buf_ptr, 1, size, fp);
    if(rev != size) 
    {
      DSP_PRINT(ERROR,"Loading file failed\n");
      return -1;
    }
    fclose(fp);
    config_para.task_id= task->task_id;
    config_para.algo_id =-1;
    config_para.algo_ptr = buf_phy;
    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_ALGO_LOAD, &config_para,sizeof(csi_dsp_algo_load_req_t),&resp,sizeof(resp),buffer_group))
    {
        DSP_PRINT(ERROR,"send cmd fail\n");
        return -1;
    }
    if(resp.status != CSI_DSP_OK)
    {
        DSP_PRINT(ERROR,"resp fail %d\n",resp.status);
        return -1; 
    }

    if(buffer)
    {
        // memset(buf_ptr,0xee,config_para->sett_length);
        xrp_release_buffer(buffer);
        xrp_release_buffer_group(buffer_group);
    }
    task->algo.algo_id=config_para.algo_id;
    task->algo.algo_ptr = config_para.algo_ptr;
    DSP_PRINT(INFO,"task %d acquire algo:%s sucessful!\n",task->task_id,name);
    return 0;

}
int csi_dsp_task_start(void *task_ctx)
{
    csi_dsp_status_e resp;
    struct csi_dsp_task_start_req req;
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    req.task_id = task->task_id;
    if(task== NULL)
    {
        DSP_PRINT(ERROR,"ERR Invalid task \n");
        return -1;
    }
    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_TASK_START,&req,sizeof(struct csi_dsp_task_start_req),&resp,sizeof(resp),NULL))
    {
        DSP_PRINT(ERROR,"csi_dsp_task_start fail \n",resp);
        return -1;
    }

    if(resp != CSI_DSP_OK )
    {
        DSP_PRINT(ERROR,"csi_dsp_task_start resp due to %d\n",resp);
        return -1;
    }
    DSP_PRINT(INFO,"task start sucessful!\n",task->task_id);
    return 0;
}

int csi_dsp_task_stop(void *task_ctx)
{
    csi_dsp_status_e resp;
    struct csi_dsp_task_start_req req;
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;

    if(task== NULL)
    {
        DSP_PRINT(ERROR,"ERR Invalid task \n");
        return -1;
    }
    req.task_id = task->task_id;

    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_TASK_STOP,&req,sizeof(req),&resp,sizeof(resp),NULL))
    {
        return -1;
    }
    if(resp != CSI_DSP_OK)
    {
        printf("csi_dsp_task_start fail due to %d\n",resp);
    }
    return 0;

}

static int csi_dsp_config_report_item_to_dsp(void *task_ctx,enum cmd_type flag)
{
    csi_dsp_status_e resp;
    struct report_config_msg config;
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    if(!task || task->report_id<0 || !task->instance || !task->instance->comm_queue)
    {
        DSP_PRINT(ERROR,"param check fail\n");
        return -1;
    }
    config.report_id=task->report_id;
    config.flag = flag;
    memcpy(config.task,task->task_ns,TASK_NAME_LINE);
    config.addr = 0xdeadbeef;
    config.size = task->report_size; 
    if(csi_dsp_cmd_send(task->instance->comm_queue,PS_CMD_REPORT_CONFIG,&config,sizeof(struct report_config_msg),&resp,sizeof(resp),NULL))
    {
        DSP_PRINT(ERROR,"send PS_CMD_REPORT_CONFIG fail\n");
        return -1;
    }
    if(resp != CSI_DSP_OK)
    {
        DSP_PRINT(ERROR,"PS_CMD_REPORT_CONFIG fail due to %d\n",resp);
        return -1;
    }
    return 0;
}


int csi_dsp_task_register_cb(void *task_ctx,
                            int (*cb)(void*context,void*data),
                            void* context,
                            size_t data_size)
{
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    task->report_id = task->task_id;
    if(task->report_id<0)
    {
       DSP_PRINT(WARNING,"report id is invalid\n");
       return -1;
    }
    task->report_size = data_size;
    if(xrp_add_report_item_with_id(task->instance->report_impl,
								        cb,task->report_id,context,data_size)<0)
    {
        DSP_PRINT(WARNING,"report id is invalid\n");
        return -1;
    }
    if(csi_dsp_config_report_item_to_dsp(task,CMD_SETUP))
    {
        DSP_PRINT(WARNING,"report id is invalid\n");
        return -1;
    }
    task->cb = cb;
    task->context = context;
    DSP_PRINT(INFO,"new reprot %d is created and register to DSP\n",task->report_id);
    return 0;
}

int csi_dsp_ps_task_unregister_cb(void *task_ctx)
{
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    if(task->report_id<0)
    {
        DSP_PRINT(WARNING,"report id is invalid\n");
        return -1;
    }
    if(csi_dsp_config_report_item_to_dsp(task,CMD_RELEASE))
    {
        DSP_PRINT(WARNING,"report release fail\n");
        return -1;
    }

    xrp_remove_report_item(task->instance->report_impl,task->report_id);
    DSP_PRINT(INFO,"new reprot %d is unregister to DSP\n",task->report_id);
    task->report_id =-1;
   
    return 0;
    
}

struct csi_sw_task_req* csi_dsp_task_create_request(void *task_ctx)
{
    struct csi_dsp_task_handler * task = (struct csi_dsp_task_handler *)task_ctx;
    struct csi_sw_task_req*  req = NULL;
    void* req_ptr,*event_ptr;
    enum xrp_status status;
    if(task->mode != CSI_DSP_TASK_SW_TO_SW)
    {
        DSP_PRINT(WARNING,"un-support for task type:%d\n",task->mode);
        return NULL;
    }
    if(task->algo.algo_id == DSP_INVALID_ITEM)
    {
        DSP_PRINT(WARNING,"algo is not loaded:%d\n",task->algo.algo_id);
        return NULL;
    }
    req = malloc(sizeof(*req));
    if(req == NULL)
    {
        DSP_PRINT(WARNING,"memroy alloc fail\n");
        return NULL;
    }

    req->priv = xrp_create_buffer_group(&status);
    if(status != XRP_STATUS_SUCCESS)
    {
        DSP_PRINT(WARNING,"buffer group create fail\n");
        free(req);
        return NULL;
    }
    req->status = CSI_DSP_SW_REQ_IDLE;
    req->algo_id = task->algo.algo_id;
    req->task = task_ctx;
    req->buffer_num =0;
    req->request_id = rand();
    req->sett_length =0;
    req->sett_ptr = NULL;
    DSP_PRINT(DEBUG,"new req %d is created in task %d\n",req->request_id,task->task_id);
    return req;

}


int csi_dsp_task_release_request(struct csi_sw_task_req*  req)
{
    struct xrp_buffer_group *group;
    enum xrp_status status;
    int buf_idx,plane_idx;;
    struct xrp_buffer *buffer;
    int index;
    size_t buf_num;
    struct csi_dsp_task_handler * task =(struct csi_dsp_task_handler *) req->task;
    if(req == NULL)
    {
        return 0;
    }
    group = (struct xrp_buffer_group *)req->priv;
    if(group == NULL)
    {
        free(req);
        DSP_PRINT(WARNING,"buffer group create fail\n");
        return -1;
    }
    
    for(buf_idx=0;buf_idx<req->buffer_num;buf_idx++)
    {
        if(req->buffers[buf_idx].type == CSI_DSP_BUF_TYPE_DMA_BUF_IMPORT ||
            req->buffers[buf_idx].type == CSI_DSP_BUF_TYPE_DMA_BUF_EXPORT)
        {
            for(plane_idx =0 ;plane_idx<req->buffers[buf_idx].plane_count;plane_idx++)
            {
                 xrp_release_dma_buf(task->instance->device,req->buffers[buf_idx].planes[plane_idx].fd,&status);
            }
        }
        else
        {
            for(plane_idx =0 ;plane_idx<req->buffers[buf_idx].plane_count;plane_idx++)
            {
                // printf("release buf %d plane %d\n",buf_idx,plane_idx);
                buffer = xrp_get_buffer_from_group(group,req->buffers[buf_idx].planes[plane_idx].fd,&status);
                xrp_unmap_buffer(buffer,(void *)req->buffers[buf_idx].planes[plane_idx].buf_vir,&status);
                xrp_release_buffer(buffer);
            }
        }

    }
    /*free sett buf   */
    xrp_buffer_group_get_info(group,XRP_BUFFER_GROUP_SIZE_SIZE_T, 0,&buf_num,sizeof(buf_num),&status);
    if(status!=XRP_STATUS_SUCCESS)
    {
        DSP_PRINT(WARNING,"Get sett buffer release fail\n");
        return -1;
    }
    uint64_t buf_phy;
    for(index=0; index<buf_num; index++)
    {
        buffer = xrp_get_buffer_from_group(group,index,&status);
        xrp_buffer_get_info(buffer,XRP_BUFFER_PHY_ADDR,&buf_phy,sizeof(buf_phy),&status);
        if(buf_phy == req->sett_ptr)
        {
            // xrp_unmap_buffer(buffer,(void *)req->buffers[buf_idx].planes[plane_idx].buf_vir,&status);
            xrp_release_buffer(buffer);
            break;
        }
    }

    xrp_release_buffer_group(group);
    DSP_PRINT(DEBUG,"req %d is release successful!\n",req->request_id);
    free(req);
    return 0;
}
int csi_dsp_task_create_buffer(void * task_ctx,struct csi_dsp_buffer * buffer)
{
    struct csi_dsp_task_handler * task= (struct csi_dsp_task_handler *)task_ctx;
    int i,j;
    struct xrp_buffer * buf=NULL;
    size_t buf_size;
    int fail_release =0;
    uint64_t phy_addr;
    enum xrp_status status;
     if(task == NULL || buffer==NULL)
     {
          DSP_PRINT(WARNING,"param check fail\n");
         return -1;
     }
     if(task->buffers==NULL)
     {
         DSP_PRINT(WARNING,"Buffrs not init\n");
         return -1;
     }
     switch(buffer->type)
     {
         case CSI_DSP_BUF_ALLOC_DRV:
                for(i=0;i<buffer->plane_count;i++)
                {
                    buf = xrp_create_buffer(task->instance->device,buffer->planes[i].size,NULL,&status);
                    if(status != XRP_STATUS_SUCCESS)
                    {
                         DSP_PRINT(WARNING,"create xrp buffer fail\n");
                         goto err_1;
                    }
                    else
                    {
                        int flag = buffer->dir == CSI_DSP_BUFFER_IN ?XRP_READ:XRP_WRITE;
                        buffer->planes[i].buf_vir = xrp_map_buffer(buf,0,buffer->planes[i].size,flag,&status);
                        if(status != XRP_STATUS_SUCCESS)
                        {

                            xrp_release_buffer(buf);
                            DSP_PRINT(WARNING,"Error Map Buffrs not fail\n");
                            goto err_1;
                        }
                        // printf("%s,Debug V:%llx,P:%llx\n",__FUNCTION__,buf->ptr,buf->phy_addr);
                        xrp_buffer_get_info(buf,XRP_BUFFER_PHY_ADDR,&buffer->planes[i].buf_phy,sizeof(phy_addr),&status);
                        if(status != XRP_STATUS_SUCCESS)
                        {
                            
                            xrp_unmap_buffer(buf,buffer->planes[i].buf_vir,&status);
                            xrp_release_buffer(buf);
                            DSP_PRINT(WARNING,"Error get phy addr fail\n");
                            goto err_1;
                        }
                        buffer->planes[i].fd = xrp_add_buffer_to_group(task->buffers,buf,flag,&status);
                        if(status !=XRP_STATUS_SUCCESS)
                        {
                            xrp_unmap_buffer(buf,buffer->planes[i].buf_vir,&status);
                            xrp_release_buffer(buf);
                            DSP_PRINT(WARNING,"add to buffer group fail\n");
                            goto err_1;
                        }                        
                    }
                    DSP_PRINT(DEBUG,"create buffer:Vaddr(0x%llx),Paddr(0x%llx)\n", buffer->planes[i].buf_vir,buffer->planes[i].buf_phy);
                }
                return 0;

                err_1:
                    for(j=0;j<i;j++)
                    {
                         buf = xrp_get_buffer_from_group(task->buffers, buffer->planes[j].fd,&status);
                         xrp_unmap_buffer(buf,buffer->planes[j].buf_vir,&status);
                         xrp_release_buffer(buf);
                    }
                    return -1;

         case CSI_DSP_BUF_ALLOC_APP:
                 return 0;
         case  CSI_DSP_BUF_TYPE_DMA_BUF_EXPORT:
                  DSP_PRINT(INFO,"ERR DMA buffer export not support\n");
                 return 0; 
         case CSI_DSP_BUF_TYPE_DMA_BUF_IMPORT:
                    
                for(i=0;i<buffer->plane_count;i++)
                {
                    int flag = buffer->dir == CSI_DSP_BUFFER_IN ?XRP_READ:XRP_WRITE;


                    xrp_import_dma_buf(task->instance->device,buffer->planes[i].fd,flag,&buffer->planes[i].buf_phy,
                                               &buffer->planes[i].buf_vir,&buffer->planes[i].size,&status);
                    if(status !=XRP_STATUS_SUCCESS )
                    {
                        DSP_PRINT(WARNING,"dma buf import fail\n");
                        goto err_2;
                    }

                }
                break;
                err_2:
                    for(j=0;j<i;j++)
                    {
                            xrp_release_dma_buf(task->instance->device,buffer->planes[i].fd,&status);
                    }
                    return -1;
         default:
                DSP_PRINT(WARNING,"buffer type:%d not support\n",buffer->type);
            return -1;
     }
     return 0;

}

int csi_dsp_task_free_buffer(void * task_ctx,struct csi_dsp_buffer * buffer)
{
    struct csi_dsp_task_handler * task= (struct csi_dsp_task_handler *)task_ctx;
    int i,j;
    struct xrp_buffer * buf=NULL;
    size_t buf_size;
    int fail_release =0;
    uint64_t phy_addr;
    enum xrp_status status;

     if(task == NULL||task->buffers==NULL||buffer==NULL)
     {
         DSP_PRINT(WARNING,"param check fail\n")
         return -1;
     }


     switch(buffer->type)
     {
         case CSI_DSP_BUF_ALLOC_DRV:
                for(i=0;i<buffer->plane_count;i++)
                {
                   
                         buf = xrp_get_buffer_from_group(task->buffers,buffer->planes[i].fd,&status);
                         xrp_unmap_buffer(buf,buffer->planes[i].buf_vir,&status);
                         xrp_release_buffer(buf);
                }
                break;

         case CSI_DSP_BUF_ALLOC_APP:
                break;
         case CSI_DSP_BUF_TYPE_DMA_BUF_EXPORT:
                DSP_PRINT(WARNING,"buffer type not support\n");
                break;
         case CSI_DSP_BUF_TYPE_DMA_BUF_IMPORT:
                for(i=0;i<buffer->plane_count;i++)
                {
                    xrp_release_dma_buf(task->instance->device,buffer->planes[i].fd,&status);
                    if(status != XRP_STATUS_SUCCESS)
                    {
                        DSP_PRINT(WARNING,"ERR DMA Buffrs(%d) Release fail\n",buffer->planes[i].fd);
                        return -1;
                    }                   
                }
                break;
         default:
            DSP_PRINT(WARNING,"buffer type:%d not support\n",buffer->type);
            return -1;
     }
     return 0;

}

int csi_dsp_request_add_buffer(struct csi_sw_task_req* req,struct csi_dsp_buffer * buffer)
{
    struct csi_dsp_task_handler * task=NULL;
    struct xrp_buffer * buf=NULL;
    enum xrp_status status;
    int i,j;
    struct xrp_buffer_group *buf_gp ;
    if(!req )
    {
        return -1;
    }
    buf_gp = (struct xrp_buffer_group *)req->priv;
    task = req->task;
    int flag = buffer->dir == CSI_DSP_BUFFER_IN ?XRP_READ:XRP_WRITE;
    switch(buffer->type)
    {
         case CSI_DSP_BUF_ALLOC_DRV:
                for(i=0;i<buffer->plane_count;i++)
                {
                    buf = xrp_create_buffer(task->instance->device,buffer->planes[i].size,NULL,&status);
                    if(status != XRP_STATUS_SUCCESS)
                    {
                         DSP_PRINT(WARNING,"create buffer failed\n");
                         goto err_1;
                    }
                    else
                    {

                        buffer->planes[i].buf_vir = xrp_map_buffer(buf,0,buffer->planes[i].size,flag,&status);
                        if(status != XRP_STATUS_SUCCESS)
                        {
                            DSP_PRINT(WARNING,"Map Buffrs not init\n");
                            xrp_release_buffer(buf);
                            goto err_1;
                        }
                        xrp_buffer_get_info(buf,XRP_BUFFER_PHY_ADDR,&buffer->planes[i].buf_phy,sizeof(uint64_t),&status);
                        if(status != XRP_STATUS_SUCCESS)
                        {
                            DSP_PRINT(WARNING,"xrp_buffer_get_info failed\n");
                            xrp_unmap_buffer(buf,buffer->planes[i].buf_vir,&status);
                            xrp_release_buffer(buf);
                            goto err_1;
                        }
                        buffer->planes[i].fd = xrp_add_buffer_to_group(buf_gp,buf,flag,&status);
                        if(status !=XRP_STATUS_SUCCESS)
                        {
                            DSP_PRINT(WARNING,"xrp_add_buffer_to_group failed\n");
                            xrp_unmap_buffer(buf,buffer->planes[i].buf_vir,&status);
                            xrp_release_buffer(buf);
                            goto err_1;
                        }                        
                    }
                   DSP_PRINT(DEBUG,"create buffer:Vaddr(0x%llx),Paddr(0x%llx)\n", buffer->planes[i].buf_vir,buffer->planes[i].buf_phy);
                }
                memcpy(&req->buffers[req->buffer_num++],buffer,sizeof(*buffer));
                return 0;

                err_1:
                    for(j=0;j<i;j++)
                    {
                         buf = xrp_get_buffer_from_group(buf_gp,buffer->planes[j].fd,&status);
                         xrp_unmap_buffer(buf,buffer->planes[j].buf_vir,&status);
                         xrp_release_buffer(buf);
                    }
                return -1;
         case  CSI_DSP_BUF_TYPE_DMA_BUF_EXPORT:
                 DSP_PRINT(INFO,"ERR DMA buffer export not support\n");
                 return -1; 
         case CSI_DSP_BUF_TYPE_DMA_BUF_IMPORT:
                    
                for(i=0;i<buffer->plane_count;i++)
                {
                    xrp_import_dma_buf(task->instance->device,buffer->planes[i].fd,flag,&buffer->planes[i].buf_phy,
                                               & buffer->planes[i].buf_vir,&buffer->planes[i].size,&status);
                    if(status !=XRP_STATUS_SUCCESS )
                    {
                        DSP_PRINT(WARNING,"dma buf import fail\n");
                        goto err_2;
                    }

                }
                memcpy(&req->buffers[req->buffer_num++],buffer,sizeof(*buffer));
                break;
                err_2:
                    for(j=0;j<i;j++)
                    {
                        xrp_release_dma_buf(task->instance->device,buffer->planes[i].fd,&status);
                    }
                    return -1;
         case CSI_DSP_BUF_ALLOC_APP:
                return 0;
         default:
            DSP_PRINT(WARNING,"buffer type not support\n");
            return -1;
     }
    return 0;
}

int csi_dsp_request_set_property(struct csi_sw_task_req* req,void* property,size_t sz)
{
    if(!req || !property || sz==0)
    {
        return -1;
    }
    csi_dsp_status_e status;
    struct csi_dsp_task_handler * task=req->task;
    struct xrp_buffer * buf=NULL;
    void *sett_virt_addr =NULL;
    struct xrp_buffer_group *buf_gp ;

    buf_gp = (struct xrp_buffer_group *)req->priv;
    req->sett_length = sz;
    buf = xrp_create_buffer(task->instance->device,req->sett_length,NULL,&status);
    if(status != XRP_STATUS_SUCCESS)
    {
        DSP_PRINT(WARNING,"create buffer failed\n");
        return -1;
    }
    else
    {
    
        xrp_buffer_get_info(buf,XRP_BUFFER_USER_ADDR,&sett_virt_addr,sizeof(uint64_t),&status);
        if(status != XRP_STATUS_SUCCESS)
        {
            DSP_PRINT(WARNING,"ERROR get virtual addr\n");
            xrp_release_buffer(buf);
            return -1;
        }
        xrp_buffer_get_info(buf,XRP_BUFFER_PHY_ADDR,&req->sett_ptr,sizeof(uint64_t),&status);
        if(status != XRP_STATUS_SUCCESS)
        {
            DSP_PRINT(WARNING,"Get PHY_ADDR failed\n");
            xrp_release_buffer(buf);
            return -1;
        }
        xrp_add_buffer_to_group(buf_gp,buf,XRP_READ,&status);
        if(status !=XRP_STATUS_SUCCESS)
        {
           DSP_PRINT(WARNING,"xrp_add_buffer_to_group failed\n");
            xrp_release_buffer(buf);
            return -1;
        }                        
    }
    memcpy(sett_virt_addr,property,req->sett_length);
    DSP_PRINT(DEBUG,"setting propert in %p succeful!\n",req->sett_ptr);   
    return 0;
}


int csi_dsp_request_enqueue(struct csi_sw_task_req* req)
{
    struct csi_dsp_task_handler * task=NULL;
    struct xrp_buffer * buf=NULL;
    task_event_item_t *event_item;
    csi_dsp_status_e status;
    struct xrp_event *evt;
    csi_dsp_sw_task_manager_t * sw_task_ctx;
	enum xrp_status s;
    int loop;
    if(!req )
    {
        return -1;
    }
    task = (struct csi_dsp_task_handler *)req->task;

    sw_task_ctx = (csi_dsp_sw_task_manager_t *)task->private;
    event_item = malloc(sizeof(task_event_item_t));
    if(event_item==NULL)
    {
        DSP_PRINT(WARNING,"malloc fail\n");
		return -1;
    }

    for(loop =0;loop<req->buffer_num;loop++)
    {
        csi_dsp_buf_flush(task->instance->device,&req->buffers[loop]);
    }

	xrp_enqueue_command(task->queue, req, sizeof(struct csi_sw_task_req),
			    &event_item->req_status, sizeof(event_item->req_status),
			    req->priv, &evt, &s);
	if (s != XRP_STATUS_SUCCESS) {
		DSP_PRINT(WARNING,"enqueue task to dsp fail\n");
		return -1;
	}

    req->status = CSI_DSP_SW_REQ_RUNNING;
    event_item->event = evt;
    event_item->req = req;
    pthread_mutex_lock(&sw_task_ctx->mutex);
    list_add_tail(&event_item->head,&sw_task_ctx->event_list);
    sw_task_ctx->event_num++;
    pthread_mutex_unlock(&sw_task_ctx->mutex);
    DSP_PRINT(DEBUG,"Req %d is enqueue \n",req->request_id);
    return 0;
}

struct csi_sw_task_req*  csi_dsp_request_dequeue(void *task_ctx)
{
    struct csi_dsp_task_handler * task=(struct csi_dsp_task_handler *)task_ctx;
    csi_dsp_sw_task_manager_t * sw_task_ctx = (csi_dsp_sw_task_manager_t *)task->private;
    struct csi_sw_task_req*  req=NULL;
    int id=0;
    task_event_item_t *item;
    enum xrp_status status;
    int loop;
    pthread_mutex_lock(&sw_task_ctx->mutex);
    struct xrp_event **evts = malloc(sw_task_ctx->event_num*sizeof(struct xrp_event*));
    list_for_each_entry(item,&sw_task_ctx->event_list,head){
            evts[id++]=item->event;
    }
    pthread_mutex_unlock(&sw_task_ctx->mutex);
    DSP_PRINT(DEBUG,"Wait for Req event \n");
    id= xrp_wait_any(evts,sw_task_ctx->event_num,&status);
    if(id>=sw_task_ctx->event_num || status !=XRP_STATUS_SUCCESS)
    {
        DSP_PRINT(WARNING,"id invalid:%d\n",id);
        return NULL;
    }
    list_for_each_entry(item,&sw_task_ctx->event_list,head){
        if(item->event == evts[id])
            break;
    }
    pthread_mutex_lock(&sw_task_ctx->mutex);
    list_del(&item->head);
    sw_task_ctx->event_num--;
    pthread_mutex_unlock(&sw_task_ctx->mutex);
    req = item->req;
    xrp_release_event(item->event);
    if(item->req_status !=CSI_DSP_OK)
    {
        DSP_PRINT(WARNING,"req fail with resp:%d\n",item->req_status);
        req->status = CSI_DSP_SW_REQ_FAIL;
    }
    else{
        req->status = CSI_DSP_SW_REQ_DONE;
    }
    free(evts);
    free(item);

    for(loop =0;loop<req->buffer_num;loop++)
    {
        csi_dsp_buf_flush(task->instance->device,&req->buffers[loop]);
    }

    DSP_PRINT(DEBUG,"Req %d is deuque \n",req->request_id);
    return req;
}


int csi_dsp_test_config(void* dsp ,struct csi_dsp_ip_test_par* config_para,void* buf)
{
    struct csi_dsp_instance * instance = (struct csi_dsp_instance *)dsp;
    if(!dsp || !buf || !config_para->result_buf_size)
    {
        DSP_PRINT(WARNING,"param check fail\n");
        return -1;
    }

    if(csi_dsp_cmd_send(instance->comm_queue,PS_CMD_DSP_IP_TEST, config_para,sizeof(struct csi_dsp_ip_test_par),buf,config_para->result_buf_size,NULL))
    {
        DSP_PRINT(WARNING," send cmd fail\n");
        return -1;
    }

    return 0;
}