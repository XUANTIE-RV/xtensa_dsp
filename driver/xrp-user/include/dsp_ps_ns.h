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
 * \brief This section defines DSP and host shared data and strcut.
 *
 * General  properties:
 * 1. Define the common shared struct for both DSP and Host
 * 2. The shared strcut only visible internal, for those open to APP shou be define in dsp_post_process_defs
 */

#ifndef _DSP_PS_NS_H
#define _DSP_PS_NS_H

#include <stdint.h>

#include "xrp_api.h"
#include "csi_dsp_task_defs.h"
#ifdef __cplusplus
extern "C" {
#endif
// enum {
//   PS_CMD_IDLE = 0,
//   PS_CMD_INLINE_START =PS_CMD_IDLE,
//   PS_CMD_LOOPBACK_TEST,
//   PS_CMD_ISP_CONFIG,
//   PS_CMD_RY_CONFIG,
//   PS_CMD_ISP_ALGO_CONFIG,
//   PS_CMD_QUERY_DRAM_SIZE,
//   PS_CMD_SET_IO_BUFFER,
//   PS_CMD_WITH_BUFFER_START,
// };

#define TASK_NAME_LINE  XRP_NAMESPACE_ID_SIZE
#define XRP_PS_NSID_COMMON_CMD \
    {0xf9, 0x3c, 0x09, 0x61, 0x9d, 0x3f, 0x45, 0x29, \
    0xbd, 0xe9, 0x7a, 0x4b, 0x18, 0x91, 0xdb, 0x15}


#define XRP_PS_NSID_ISP_ALGO \
    {0xf9, 0x3c, 0x09, 0x61, 0x9d, 0x3f, 0x45, 0x29, \
    0xbd, 0xe9, 0x7a, 0x4b, 0x18, 0x91, 0xdb, 0x14}

typedef struct {
	uint32_t  cmd;
	uint32_t  data[1];
}s_cmd_t;
typedef enum {  
  PS_CMD_IDLE = 0,
  PS_CMD_INLINE_START =PS_CMD_IDLE,
  PS_CMD_TASK_ALLOC,
  PS_CMD_TASK_FREE,
  PS_CMD_LOOPBACK_TEST,
  PS_CMD_REPORT_CONFIG,
  PS_CMD_LOG_CONFIG,
  PS_CMD_HEART_BEAT_REQ,
  PS_CMD_LOAD_MINITOR_REQ,
  PS_CMD_FE_CONFIG,
  PS_CMD_BE_CONFIG,

  PS_CMD_ALGO_CONFIG,
  PS_CMD_TASK_START,

  PS_CMD_TASK_STOP,

  PS_CMD_DATA_MOVE,

  PS_CMD_DSP_IP_TEST,
  PS_CMD_BE_ASSGIN_BUF,
  PS_CMD_ALGO_LOAD,
}ECOMMON_CMD;

enum cmd_type{
    CMD_SETUP,
    CMD_RELEASE,
    CMD_INVALID,
};

struct report_config_msg{
  enum cmd_type flag;
  int32_t  report_id;
  char task[TASK_NAME_LINE];
  uint32_t  size;
  uint64_t  addr;
};

struct data_move_msg{
  uint64_t  src_addr;
  uint64_t  dst_addr;
  uint32_t  size;
};

struct csi_dsp_task_start_req{
    int  task_id;
    char task_ns[TASK_NAME_LINE];
};

struct csi_dsp_task_create_resp{

    csi_dsp_status_e status;
    char task_ns[TASK_NAME_LINE];
    int  task_id;
    
};

struct csi_dsp_task_free_req{
    int  task_id;
    char task_ns[TASK_NAME_LINE];
};

struct csi_dsp_ip_test_par{
	char case_name[32];
    char case_goup[32];
    int  result_buf_size;
};

//  struct csi_dsp_algo_config_par{
// 	uint16_t algo_id;
//     char* algo;
//     union
//     {
//         float  gam_coef[4];
//         short  beta_coef[4];
//     };
// };
#ifdef __cplusplus
}
#endif
#endif
