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

#include "dsp_common.h"

int log_level = CSI_DSP_LOG_INFO;
int pid = 0;


static int getLogLevel()
{
    pid = getpid();
    char *env = getenv("LIGHT_DSP_LOG_LEVEL");
   
    if (env == NULL)
        return CSI_DSP_LOG_ERROR;
    else
    {
        int level = atoi(env);
        if (level >= CSI_DSP_LOG_MAX || level < CSI_DSP_LOG_QUIET)
            return CSI_DSP_LOG_ERROR;
        else
            return level;
    }
}

void dsp_InitEnv()
{
    log_level = getLogLevel();

}
