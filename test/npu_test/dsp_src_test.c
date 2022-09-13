/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/* auto generate by HHB_VERSION "1.8.0" */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>
#include <unistd.h>
#include "io.h"
// #include "csi_ref.h"
#include "shm_common.h"
#include "test_bench_utils.h"
#include "../../driver/xrp-user/include/dsp_ps_ns.h"
#include "../../driver/xrp-user/dsp-ps/csi_dsp_core.h"
#include "csi_dsp_api.h"
#define MODULE_NAME "dsp_test"
#define FILE_LENGTH         1028

#define WIDTH 224
#define HEIGHT 224
int input_size[] = {1 * 3 * WIDTH * HEIGHT, };

#define BASE_MEMORY 0xD0000000
typedef struct _PictureBuffer {
    unsigned int bus_address;
    void *virtual_address;
    unsigned int size;
} PictureBuffer;


void AllocateBuffers(PictureBuffer picbuffers[NUM_OF_BUFFERS],unsigned int buffer_num, unsigned int size, int fd_mem) {
  unsigned int bus_address = BASE_MEMORY;
  unsigned int buffer_size = (size + 0xFFF) & ~0xFFF;
  for (int i = 0; i < buffer_num; i++) {
    picbuffers[i].virtual_address = mmap(0, buffer_size, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fd_mem,
                                        bus_address);
    printf("mmap %p from %x with size %d\n", picbuffers[i].virtual_address, bus_address, size);
    picbuffers[i].bus_address = bus_address;
    picbuffers[i].size = buffer_size;
    bus_address += buffer_size;
  }
}

void FreeBuffers(PictureBuffer picbuffers[NUM_OF_BUFFERS], unsigned int buffer_num, unsigned int size, int fd_mem) {
  for (int i = 0; i < buffer_num; i++) {
    munmap(picbuffers[i].virtual_address, picbuffers[i].size);
  }
}

int main(int argc, char **argv) {
    char **data_path = NULL;
    int input_num = 1;
    int output_num = 1;
    int i;
    int index = 0;

    if (argc < (1 + input_num)) {
        printf("Please set valide args: ./dsp_src_test image.rgb\n");
        return -1;
    } else {
            data_path = argv + 1;
    }

	struct csi_dsp_instance *instance = csi_dsp_create_instance(0);
	if(!instance)
	{
		printf("create fail\n");
		return -1;
	}
    struct data_move_msg cmd;

    int fd = shm_open("/ispnpu", O_RDWR, 0);
    if (fd == -1) {
        printf("%s: failed to shm_open", MODULE_NAME);
        return -1;
    }
    ShmBuffer *buffer = mmap(NULL, sizeof(ShmBuffer),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        printf("%s: failed to mmap", MODULE_NAME);
        return -1;
    }
    
    int fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd_mem < 0) {
        printf("%s: failed to open /dev/mem", MODULE_NAME);
        return -1;
    }
    
    int in_size = input_size[0];
    char filename[FILE_LENGTH] = {0};
    char filename_prefix[FILE_LENGTH] = {0};
    uint64_t start_time, end_time;
    PictureBuffer picbuffers[NUM_OF_BUFFERS*2];
    AllocateBuffers(picbuffers, NUM_OF_BUFFERS*2,in_size, fd_mem);


    do {
        if (buffer->exit == 0) {
            // Wait for available slot in buffer queue
            printf("sem_wait before\n");
            if (sem_wait(&buffer->sem_sink) == -1) {
                printf("%s: failed to open sem_wait", MODULE_NAME);
                return -1;
            }
            printf("sem_wait end\n");
            //ProcessOneFrame(picbuffers[index].virtual_address, fp, size);
            fill_buffer_from_file(data_path[0], picbuffers[index].virtual_address);
            printf("%s: prepared frame %d 0x%08x: %dx%d\n", 
                MODULE_NAME, index, picbuffers[index].bus_address, 
                WIDTH, HEIGHT);

            cmd.src_addr=  picbuffers[index].bus_address;
            cmd.dst_addr= picbuffers[NUM_OF_BUFFERS+index].bus_address;
            cmd.size = picbuffers[index].size;

            printf("%s: data move cmd frame %d from 0x%lx to 0x%lx ,size:%d\n", 
                MODULE_NAME, index, cmd.src_addr, cmd.dst_addr,cmd.size);
	    //snprintf(filename, FILE_LENGTH, "%s_src_data%u.txt", filename_prefix, i);
            //save_uint8_to_file(filename, (uint8_t*)picbuffers[index].virtual_address, in_size);
            if(csi_dsp_cmd_send(instance->comm_queue,PS_CMD_DATA_MOVE,&cmd,sizeof(struct data_move_msg),NULL,0,NULL))
            {
                printf("%s:cmd_to_dsp fail\n",MODULE_NAME);
                return -1;
            }


            PictureInfo *pic = &buffer->pics[index];
            pic->bus_address_rgb = picbuffers[NUM_OF_BUFFERS+index].bus_address;
            pic->pic_width = WIDTH;
            pic->pic_height = HEIGHT;
            printf("%s: Processed frame %d 0x%08x: %dx%d\n", 
                MODULE_NAME, index, pic->bus_address_rgb, 
                pic->pic_width, pic->pic_height);
            // Notify npu one picture is ready for inference
            if (sem_post(&buffer->sem_src) == -1) {
                printf("%s: failed to sem_post\n", MODULE_NAME);
                return -1;
            }
        }
        else {
            // If encoder set the 'exit' flag to 1, finish process
            printf("%s: Exit at loop %d\n", MODULE_NAME, index);
            // Notify encoder all the resource has been released
            if (sem_post(&buffer->sem_src) == -1) {
                printf("%s: failed to sem_post\n", MODULE_NAME);
                return -1;
            }
        }
        index = (index + 1) % NUM_OF_BUFFERS;
    } while (buffer->exit == 0);

cleanup:
    FreeBuffers(picbuffers, NUM_OF_BUFFERS*2,in_size, fd_mem);
    munmap(buffer, sizeof(ShmBuffer));
    shm_unlink("/ispnpu");

    return 0;
}

