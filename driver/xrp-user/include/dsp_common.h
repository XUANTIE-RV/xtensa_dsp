/*
 * Copyright (c) 2022 Alibaba Group. All rights reserved.
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

#ifndef _DSP_COMMON_H_
#define _DSP_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>


#define DSP_MIN(a, b) ((a) < (b) ? (a) : (b))
#define DSP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define DSP_ALIGN_UP(num, bits) (((num) + (1 << (bits)) - 1) & ~((1 << (bits)) - 1))

#define DSP_ZERO_MEMORY(data) \
    { \
        memset(data, 0, sizeof(*(data))); \
    }

#define DSP_MEMCPY(dst, src, size) \
    { \
        memcpy(dst, src, size); \
    }

#define DSP_SAFE_FREE(buf) \
    { \
        if (buf != NULL) \
        { \
            free(buf); \
            buf = NULL; \
        } \
    }

#define DSP_PRINT(level, ...) \
    { \
        if (log_level >= CSI_DSP_LOG_##level) \
        { \
            printf("CSI_DSP[%d] %s,(%s,%d): ", pid, #level,__FUNCTION__,__LINE__);  \
            printf(__VA_ARGS__); \
        } \
    }

#define DSP_PRINT_RETURN(retcode, level, ...) \
    { \
        DSP_PRINT(level, __VA_ARGS__) \
        return retcode; \
    }

#define DSP_CHECK_CONDITION(cond, retcode, ...) \
    if (cond) \
    { \
        DSP_PRINT(ERROR, __VA_ARGS__) \
        return retcode; \
    }



typedef enum log_level
{
    CSI_DSP_LOG_QUIET = 0,
    CSI_DSP_LOG_ERROR,
    CSI_DSP_LOG_WARNING,
    CSI_DSP_LOG_INFO,
    CSI_DSP_LOG_DEBUG,
    CSI_DSP_LOG_TRACE,
    CSI_DSP_LOG_MAX
} csi_dsp_log_level;

extern int log_level;
extern int pid;

void dsp_InitEnv();

#endif 