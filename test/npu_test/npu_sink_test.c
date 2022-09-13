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

/* auto generate by HHB_VERSION "1.8.x" */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>
#include <unistd.h>
#include "io.h"
// #include "csi_ref.h"
#include "test_bench_utils.h"
#include "detect.h"
// #include "output_120_out0_nchw_1_2_7668_1.h"

#define MIN(x, y)           ((x) < (y) ? (x) : (y))
#define FILE_LENGTH         1028
#define SHAPE_LENGHT        128

void *csinn_(char *params);
void csinn_run(void *data0,  void *td);
void *csinn_nbg(const char *nbg_file_name);

int input_size[] = {1 * 3 * 300 * 300, };
int output_size[] = {1 * 7668, 1 * 2 * 7668, 1 * 1917 * 21, };
const char model_name[] = "network";

#define R_MEAN  127.5
#define G_MEAN  127.5
#define B_MEAN  127.5
#define SCALE   (1.0/127.5)

float mean[] = {B_MEAN, G_MEAN, R_MEAN};

const char class_name[][FILE_LENGTH] = {
    "background", "aeroplane", "bicycle", "bird", "boat",
    "bottle", "bus", "car", "cat", "chair", "cow", "diningtable",
    "dog", "horse", "motorbike", "person", "pottedplant", "sheep",
    "sofa", "train", "tvmonitor"
};

// static void print_tensor_info(struct csi_tensor *t) {
//     printf("\n=== tensor info ===\n");
//     printf("shape: ");
//     for (int j = 0; j < t->dim_count; j++) {
//         printf("%d ", t->dim[j]);
//     }
//     printf("\n");
//     if (t->dtype == CSINN_DTYPE_UINT8) {
//         printf("scale: %f\n", t->qinfo->scale);
//         printf("zero point: %d\n", t->qinfo->zero_point);
//     }
//     printf("data pointer: %p\n", t->data);
// }


/*
 * Postprocess function
 */
// static void postprocess(void *sess, const char *filename_prefix) {
//     int output_num, input_num;
//     struct csi_tensor *input = csi_alloc_tensor(NULL);
//     struct csi_tensor *output = csi_alloc_tensor(NULL);

//     input_num = csi_get_input_number(sess);
//     for (int i = 0; i < input_num; i++) {
//         input->data = NULL;
//         csi_get_input(i, input, sess);
//         //print_tensor_info(input);
        
//         struct csi_tensor *finput = csi_ref_tensor_transform_f32(input);
//         char filename[FILE_LENGTH] = {0};
//         char shape[SHAPE_LENGHT] = {0};
//         shape2string(input->dim, input->dim_count, shape, SHAPE_LENGHT);
//         snprintf(filename, FILE_LENGTH, "%s_input%u_%s.txt", filename_prefix, i, shape);
//         int input_size = csi_tensor_size(input);
//     	//printf("input_size: %d\n", input_size);
//         //save_data_to_file(filename, (float*)finput->data, input_size);
//     }

//     float *location;
//     float *confidence;

//     output_num = csi_get_output_number(sess);
//     for (int i = 0; i < output_num; i++) {
//         output->data = NULL;
//         csi_get_output(i, output, sess);
//         //print_tensor_info(output);

//         struct csi_tensor *foutput = csi_ref_tensor_transform_f32(output);
//         //csi_show_top5(foutput, sess);
//         char filename[FILE_LENGTH] = {0};
//         char shape[SHAPE_LENGHT] = {0};
//         shape2string(output->dim, output->dim_count, shape, SHAPE_LENGHT);
//         snprintf(filename, FILE_LENGTH, "%s_output%u_%s.txt", filename_prefix, i, shape);
//         int output_size = csi_tensor_size(foutput);
//         //save_data_to_file(filename, (float*)foutput->data, output_size);

//         if (i == 0) location = (float *)foutput->data;
//         if (i == 1) confidence = (float *)foutput->data;

//         //csi_ref_tensor_transform_free_f32(foutput);
//     }

//     BBoxOut out[100];
//     BBox gbboxes[num_prior];

//     int num = ssdforward(location, confidence, priorbox, gbboxes, out);

//     printf("%d\n", num);
//     for (int i = 0; i < num; i++) {
//         printf("%d, label=%s, score=%f, x1=%f, y1=%f, x2=%f, y2=%f\n", out[i].label, class_name[out[i].label],
//              out[i].score, out[i].xmin, out[i].ymin, out[i].xmax, out[i].ymax);
//     }
// }

int is_looping = 1;

int main(int argc, char **argv) {
    char **data_path = NULL;
    char *params_path = NULL;
    int input_num = 1;
    int output_num = 3;
    int input_group_num = 1;
    int i;
    int index = 0;

    // if (argc < (2 + input_num)) {
    //     printf("Please set valide args: ./model.elf model.params "
    //             "[tensor1/image1 ...] [tensor2/image2 ...]\n");
    //     return -1;
    // } else {
    //     if (argc == 3 && get_file_type(argv[2]) == FILE_TXT) {
    //         data_path = read_string_from_file(argv[2], &input_group_num);
    //         input_group_num /= input_num;
    //     } else {
    //         data_path = argv + 2;
    //         input_group_num = (argc - 2) / input_num;
    //     }
    // }
    // void *sess;
    // params_path = argv[1];
    // char *params = get_binary_from_file(params_path);
    // if (params == NULL) {
    //     return -1;
    // }
    // char *suffix = params_path + (strlen(params_path) - 8);
    // if (strcmp(suffix, ".mbs.bin") == 0) {
    //     // create binary graph
    //     sess = csinn_nbg(params_path);
    // } else {
    //     // create general graph
    //     sess = csinn_(params);
    // }

    uint8_t *input[input_num];
    float *finput[input_num];
    char filename_prefix[FILE_LENGTH] = {0};
    uint64_t start_time, end_time;
    uint64_t _start_time, _end_time;

    const void *shmSrc = ShmPicSrcOpen("dspnpu");
     printf("npu_sink_test:start\n");
    // while loop to receive shm picture
    while(is_looping) {
        for (i = 0; i < input_group_num; i++) {
            /* set input */
            for (int j = 0; j < input_num; j++) {
                char filename[FILE_LENGTH] = {0};
	        int in_size = input_size[0];
                // start_time = csi_get_timespec();
                //printf("npu_sink_test: wait frame%d\n", index);
	    	ShmSrcGetPicture(shmSrc, &input[j]);
                printf("npu_sink_test: get frame %d\n", index);
	        snprintf(filename, FILE_LENGTH, "%s_sink_data%u.txt", filename_prefix, i);
                //save_uint8_to_file(filename, (uint8_t*)input[j], in_size);
                
	    //     // end_time = csi_get_timespec();
        //     //     printf("wait frame time: %.5fmsf\n", ((float)(end_time-start_time))/1000000);
	 	
        //         // _start_time = csi_get_timespec();
		// finput[j] = (float *)malloc(in_size * sizeof(float));
        	
		// // pre-process with submean at each pixels
		// for (int k = 0; k < in_size; k++) {
        //             finput[j][k] = ((float)input[j][k] - mean[k/(in_size/3)]) * SCALE;
		// }
            
        //         // _end_time = csi_get_timespec();
        //         // printf("submean execution time: %.5fms\n", ((float)(_end_time-_start_time))/1000000);
                
		// _start_time = csi_get_timespec();
	    //     input[j] = csi_ref_f32_to_input_dtype(0, finput[j], sess);
        //         snprintf(filename, FILE_LENGTH, "%s_finput%u.txt", filename_prefix, i);
        //         //save_data_to_file(filename, (float*)finput[j], in_size);
            
	    //     snprintf(filename, FILE_LENGTH, "%s_input%u.txt", filename_prefix, i);
        //         //save_uint8_to_file(filename, (uint8_t*)input[j], in_size);
        //         _end_time = csi_get_timespec();
        //         printf("csi_f32_to_int8 execution time: %.5fms\n", ((float)(_end_time-_start_time))/1000000);
        //     }

        //     _start_time = csi_get_timespec();
        //     csinn_run(input[0],  sess);
        //     _end_time = csi_get_timespec();
        //     printf("Run graph execution time: %.5fms, FPS=%.2f\n", ((float)(_end_time-_start_time))/1000000,
        //             1000000000.0/((float)(_end_time-_start_time)));

        //     snprintf(filename_prefix, FILE_LENGTH, "%s", basename(data_path[i * input_num]));
        //     _start_time = csi_get_timespec();
        //     // postprocess(sess, filename_prefix);
        //     _end_time = csi_get_timespec();
        //     printf("postProcess execution time: %.5fms\n", ((float)(_end_time-_start_time))/1000000);

            for (int j = 0; j < input_num; j++) {
                // free(finput[j]);
                // free(input[j]);
                ShmSrcReleasePicture(shmSrc);
                printf("npu_sink_test: release frame%d\n", index);
	    }
            // printf("Run NPU frame inference time: %.5fms, FPS=%.2f\n", ((float)(_end_time-start_time))/1000000,
            //         1000000000.0/((float)(_end_time-start_time)));

        }
        index++;
    }
    }

    // free(params);
    ShmPicSrcClose("/dspnpu", shmSrc);
    // csi_session_deinit(sess);
    // csi_free_session(sess);

    return 0;
}

