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
 * General  properties:
 * 1. API Handler DSP TASK CREATE
 * 2. API Handler DSP ALGO config
 * 3. API
 */

#ifndef _CSI_DSP_API_H
#define _CSI_DSP_API_H

#include <string.h>
#include <stdint.h>
#include "csi_dsp_task_defs.h"
#ifdef __cplusplus
extern "C" {
#endif


/*************  CPU  interafce*********************************/

/**
 * @description:  * Open DSP device by index.

 * @param {int} dsp_id dsp index to create
 * @return {*} pointer to the opened device or NULL in case of error
 */
void *csi_dsp_create_instance(int dsp_id);

/**
 * @description: Close DSP device by index.
 * @param {void} *dsp ,dsp index to close
 * @return {int} return 0 when delete success, not 0 in case of error 
 */
int csi_dsp_delete_instance(void *dsp);

/**
 * @description: create an task on an instance 
 * Task have a dependece Algo
 * for CSI_DSP_TASK_HW (CSI_DSP_TASK_SW_TO_HW|CSI_DSP_TASK_SW_TO_SW)
 * both csi_dsp_task_config_frontend and csi_dsp_task_config_backend are needed to call
 * for CSI_DSP_TASK_SW_TO_SW use task req to handle seperate process req on this task 
 * @param {void *} point to an dsp instance
 * @param {csi_dsp_task_mode_e} task_type 
 * @return {*}pointer to the createrf task or NULL in case of error
 */
void *csi_dsp_create_task(void * instance,csi_dsp_task_mode_e task_type);

/**
 * @description: create a report handler on an instance
 * @param {void*} point to an dsp instance
 * @return {int} return 0 when create success, not 0 in case of error 
 */
int csi_dsp_create_reporter(void* dsp);
/**
 * @description:  destroy a report handler on an instance
 * @param {void}  point to an dsp instance
 * @return {int}  return 0 when create success, not 0 in case of error 
 */
int csi_dsp_destroy_reporter(void *dsp);
/**
 * @description: destroy a task handler
 * @param {void} *task, point to an task handler
 * @return {int} return 0 when credestroy success, not 0 in case of error 
 */
void csi_dsp_destroy_task(void *task);

/**
 * @description: config a task's frontend ,for SW Task 
 * @param {void} *task
 * @param {csi_dsp_task_fe_para} *config_para
 * @return {*}
 */
int csi_dsp_task_config_frontend(void *task,struct csi_dsp_task_fe_para *config_para);

/**
 * @description: 
 * @param {void} *task
 * @param {csi_dsp_task_fe_para} *config_para
 * @return {*}
 */
int csi_dsp_task_get_frontend(void *task,struct csi_dsp_task_fe_para *config_para);
/**
 * @description: 
 * @param {void} *task
 * @param {csi_dsp_task_be_para} *config_para
 * @return {*}
 */
int csi_dsp_task_config_backend(void *task,struct csi_dsp_task_be_para *config_para);
/**
 * @description: 
 * @param {void} *task
 * @param {csi_dsp_task_be_para} *config_para
 * @return {*}
 */
int csi_dsp_task_get_backend(void *task,struct csi_dsp_task_be_para *config_para);
/**
 * @description: 
 * @param {void} *task
 * @param {csi_dsp_algo_config_par} *config_para
 * @return {*}
 */
int csi_dsp_task_config_algo(void *task,struct csi_dsp_algo_config_par *config_para);
/**
 * @description: 
 * @param {void} *task_ctx
 * @param {csi_dsp_algo_load_req_t*} config_para
 * @return {*}
 */
int csi_dsp_task_load_algo(void *task_ctx, csi_dsp_algo_load_req_t* config_para);
/**
 * @description: 
 * @param {void} *task_ctx
 * @return {*}
 */
int csi_dsp_task_acquire_algo(void *task_ctx,char*name);
/**
 * @description: 
 * @param {void} *task
 * @return {*}
 */
int csi_dsp_task_start(void *task);
/**
 * @description: 
 * @param {void} *task
 * @return {*}
 */
int csi_dsp_task_stop(void *task);
/**
 * @description: 
 * @param {void} *task
 * @param {  } int
 * @param {void*} context
 * @param {size_t} data_size
 * @return {*}
 */
int csi_dsp_task_register_cb(void *task,
                            int (*cb)(void*context,void*data),
                            void* context,
                            size_t data_size);
/**
 * @description: 
 * @param {void} *task
 * @return {*}
 */
int csi_dsp_ps_task_unregister_cb(void *task);

/**
 * @description: 
 * @param {void} *task
 * @return {*}
 */
struct csi_sw_task_req* csi_dsp_task_create_request(void *task);

/**
 * @description: 
 * @param {void *} task_ctx
 * @param {csi_dsp_buffer *} buffer
 * @return {*}
 */
int csi_dsp_task_create_buffer(void * task_ctx,struct csi_dsp_buffer * buffer);
/**
 * @description: 
 * @param {void *} task_ctx
 * @param {csi_dsp_buffer *} buffer
 * @return {*}
 */
int csi_dsp_task_free_buffer(void * task_ctx,struct csi_dsp_buffer * buffer);
/**
 * @description: 
 * @param {csi_sw_task_req*} req
 * @param {csi_dsp_buffer *} buffer
 * @return {*}
 */
int csi_dsp_request_add_buffer(struct csi_sw_task_req* req,struct csi_dsp_buffer * buffer);

/**
 * @description: 
 * @param {csi_sw_task_req*} req
 * @param {void*} property
 * @param {size_t} sz
 * @return {*}
 */
int csi_dsp_request_set_property(struct csi_sw_task_req* req,void* property,size_t sz);

/**
 * @description: 
 * @param {csi_sw_task_req*} req
 * @return {*}
 */
int csi_dsp_request_enqueue(struct csi_sw_task_req* req);
/**
 * @description: 
 * @param {void} *task
 * @return {*}
 */
struct csi_sw_task_req*  csi_dsp_request_dequeue(void *task);

/**
 * @description: 
 * @param {csi_sw_task_req*} req
 * @return {*}
 */
int csi_dsp_task_release_request(struct csi_sw_task_req* req);


int csi_dsp_task_update_backend_buf(void *task_ctx,struct csi_dsp_task_be_para* config_para);
int csi_dsp_test_config(void* dsp ,struct csi_dsp_ip_test_par* config_para,void* buf);
#ifdef __cplusplus
}
#endif

#endif