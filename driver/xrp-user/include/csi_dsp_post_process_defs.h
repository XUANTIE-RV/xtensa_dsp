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
 * \brief This section defines DSP shared strcut for CPU&DSP&APP.
 *
 * General  properties:
 * 1. Post porcess define data and strcut, visiable for APP
 * 2. user define data shared bedtween DSP ans host
 */

#ifndef _CSI_DSP_POST_DEFS_H
#define _CSI_DSP_POST_DEFS_H

#include <string.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
#define CSI_DSP_MAX_BUFFER   8
typedef uint8_t dsp_id_t;



typedef enum csi_dsp_img_fmt{
	CSI_DSP_IMG_FMT_RAW8 =0,
	CSI_DSP_IMG_FMT_RAW10_UNALGIN,
	CSI_DSP_IMG_FMT_RAW10_ALGIN,
	CSI_DSP_IMG_FMT_RAW12_UNALGIN,
	CSI_DSP_IMG_FMT_RAW12_ALGIN,
	CSI_DSP_IMG_FMT_RAW16_UNALGIN,
	CSI_DSP_IMG_FMT_RAW16_ALGIN,
	CSI_DSP_IMG_FMT_NV12,
	CSI_DSP_IMG_FMT_NV21,
	CSI_DSP_IMG_FMT_I420,         	/* 420 3P Y/U/V */
	CSI_DSP_IMG_FMT_YV12,         	/* 420 3P Y/V/U */
	CSI_DSP_IMG_FMT_YUY2,         	/* 422 1P YUYV  */
	CSI_DSP_IMG_FMT_YVYU,         	/* 422 1P YVYU  */
	CSI_DSP_IMG_FMT_YV16,         	/* 422 3P Y/V/U */
	CSI_DSP_IMG_FMT_Y,         	/* Y only  		*/
	CSI_DSP_IMG_FMT_420_CHROME,  	/* u or v only	*/
	CSI_DSP_IMG_FMT_422_CHROME,   	/* u or v only	*/
	CSI_DSP_IMG_FMT_420_UV_IL,   	/* uv interleave*/
	CSI_DSP_IMG_FMT_422_UV_IL,   	/* uv interleave*/
	CSI_DSP_IMG_FMT_INVALID,


} csi_dsp_img_fmt_e;

enum buffer_property{
    CSI_DSP_BUFFER_IN,
    CSI_DSP_BUFFER_OUT,
    CSI_DSP_BUFFER_IN_OUT,

};

struct csi_dsp_plane {

	int      fd;
    uint32_t stride;         /* if buffer type is image */
	uint32_t size;
	uint64_t buf_phy;            
    uint64_t buf_vir;
};

typedef enum csi_dsp_buf_type {
	CSI_DSP_BUF_TYPE_DMA_BUF_IMPORT,		// memory allocated via dma-buf from extern
    CSI_DSP_BUF_TYPE_DMA_BUF_EXPORT,     // memory allocated via dma-buf from internal
	CSI_DSP_BUF_ALLOC_APP,	// memory allocated via APP malloc
    CSI_DSP_BUF_ALLOC_DRV,  // memory allocated via DSP Drvier
    CSI_DSP_BUF_ALLOC_FM,   // memory will auto acquire by DSP FM
} csi_dsp_buf_type_e;

struct csi_dsp_buffer {
	dsp_id_t buf_id;
    enum buffer_property dir;   
    csi_dsp_buf_type_e type;
	uint8_t format;
	uint8_t plane_count;
	uint32_t width;
	uint32_t height;
	struct csi_dsp_plane planes[3];
};
 struct csi_dsp_algo_config_par{
	int16_t  algo_id;
    int       task_id;
    uint64_t  algo_ptr;
    uint64_t  sett_ptr;
    uint32_t  sett_length;
    uint64_t  bufs_ptr;
    uint32_t  buf_num;
    uint32_t  algo_size;
  
};
typedef enum csi_dsp_sw_req_status {
	CSI_DSP_SW_REQ_IDLE,
	CSI_DSP_SW_REQ_RUNNING,
    CSI_DSP_SW_REQ_FAIL,
    CSI_DSP_SW_REQ_DONE,
} csi_dsp_sw_req_status_e;

struct csi_sw_task_req{

    uint16_t  algo_id;
    uint16_t  request_id;
    uint16_t  buffer_num; 
    csi_dsp_sw_req_status_e  status;
    uint32_t  sett_length;
    uint64_t  sett_ptr;
    struct csi_dsp_buffer buffers[CSI_DSP_MAX_BUFFER];
    void *task;
    void* priv;
};


typedef enum csi_dsp_algo_lib_id{
	CSI_DSP_ALGO_LIB_COPY,
	CSI_DSP_ALGO_LIB_0,
	CSI_DSP_ALGO_LIB_1,

}csi_dsp_algo_lib_id_e;

#ifdef __cplusplus
}
#endif

#endif
