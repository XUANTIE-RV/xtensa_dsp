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
/*!
 * \file 
 * \brief This section defines CSI DSP API.
 *

 */

#ifndef _CSI_DSP_CORE_H
#define _CSI_DSP_CORE_H

#include <string.h>
#include "xrp_api.h"
#include "csi_dsp_task_defs.h"
#include "csi_dsp_post_process_defs.h"
#include "dsp_ps_ns.h"
#include "list.h"
#include <pthread.h>
#ifdef __cplusplus
extern "C" {
#endif



struct csi_dsp_logger{

    struct xrp_queue *comm_queue;
    struct log_ops{
        int (*create)(int mode);
        int(*enable)();
        int (*disable)();
    };
};

struct csi_dsp_monitor{
    
    struct xrp_queue *comm_queue;
    struct mon_ops{
        int(*enable)();
        int (*disable)();
    };
};



typedef struct dsp_handler_item{
    struct list_head head;
    void* handler;
}dsp_handler_item_t;

struct csi_dsp_instance{
    int  id;
    struct xrp_device *device;
    struct xrp_queue *comm_queue;
    struct csi_dsp_logger *logger_impl;
    struct csi_dsp_monitor *monitor_impl;
    struct xrp_report *report_impl;

    struct list_head task_list;

};

struct csi_dsp_task_handler{
    int  task_id;  
    char task_ns[TASK_NAME_LINE];
    
    csi_dsp_task_mode_e mode;
    struct csi_dsp_instance *instance;
    struct xrp_queue  *queue;
    struct csi_dsp_task_fe_para fe;

    struct csi_dsp_task_be_para be;

    struct csi_dsp_algo_config_par algo;

    struct xrp_buffer_group *buffers;    //store task level buf info
    int  priority;
    int  report_id;
    uint32_t  report_size;
    int (*cb)(void*context,void*data);
    void *context;
    void *private;

};

typedef struct task_event_item{
    struct list_head head;
    struct xrp_event * event;
    struct csi_sw_task_req* req;
    csi_dsp_status_e req_status;
}task_event_item_t;

typedef struct csi_dsp_sw_task_manager{

    struct list_head event_list;
    int event_num;
    pthread_mutex_t mutex;
}csi_dsp_sw_task_manager_t;

int csi_dsp_test_config(void* dsp ,struct csi_dsp_ip_test_par* config_para,void* buf);
int csi_dsp_cmd_send(const struct xrp_queue *queue,int cmd_type,void * cmd_payload,
                                    size_t payload_size,void *resp, size_t resp_size,struct xrp_buffer_group *buffer_group);
int csi_dsp_enable_heartbeat_check(struct csi_dsp_instance *dsp ,int secs);

int csi_dsp_disable_heartbeat_check();
int csi_dsp_buf_flush( struct xrp_device *device,struct csi_dsp_buffer *buffers);
#ifdef __cplusplus
}
#endif

#endif