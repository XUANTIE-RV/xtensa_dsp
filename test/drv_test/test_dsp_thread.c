
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

#include "csi_dsp_api.h"
#include "csi_dsp_task_defs.h"
#include "csi_dsp_post_process_defs.h"

struct Param {
    int dsp_instance_id;
    int dsp_plane_cnt;
    int dsp_algo_id;

    char *algo_name;
    int dsp_buf_width;
    int dsp_buf_height;
};

struct timespec start_time = {0, 0};
struct timespec end_time   = {0, 0};

int create_dsp_task(void *arg) {
    struct Param param = *(struct Param *)arg;
    int i;

    void *instance = csi_dsp_create_instance(param.dsp_instance_id);
    if (!instance) {
        printf("[dsp test] create fail\n");
        pthread_exit(-1);
    }

    void *task = csi_dsp_create_task(instance, CSI_DSP_TASK_SW_TO_SW);
    if (!task) {
        printf("[dsp test] task create fail.\n");
        pthread_exit(-1);
    } else {
        printf("[dsp test] task create success.\n");
    }

    if(csi_dsp_task_acquire_algo(task,param.algo_name))
    {
        csi_dsp_algo_load_req_t algo_config = {
            .algo_id = param.dsp_algo_id,
        };

        if (csi_dsp_task_load_algo(task, &algo_config)) {
            printf("[dsp test] algo kernel config fail.\n");
            pthread_exit(-1);
        } else {
            printf("[dsp test] algo kernel config success.\n");
        }
    }


    struct csi_sw_task_req* req = NULL;
    req = csi_dsp_task_create_request(task);
    if (req == NULL) {
        printf("[dsp test] req create fail.\n");
        pthread_exit(-1);
    } else {
        printf("[dsp test] req create success.\n");
    }

    struct csi_dsp_buffer buf0 = {
        .buf_id           = 0,
        .dir              = CSI_DSP_BUFFER_IN,
        .type             = CSI_DSP_BUF_ALLOC_DRV,
        .plane_count      = param.dsp_plane_cnt,
        .width            = param.dsp_buf_width,
        .height           = param.dsp_buf_height,
        .planes[0].stride = param.dsp_buf_width,
        .planes[0].size   = param.dsp_buf_width*param.dsp_buf_height,
        .planes[1].stride = param.dsp_buf_width,
        .planes[1].size   = param.dsp_buf_width*param.dsp_buf_height,
        .planes[2].stride = param.dsp_buf_width,
        .planes[2].size   = param.dsp_buf_width*param.dsp_buf_height,
    };

    if (csi_dsp_request_add_buffer(req, &buf0)) {
        printf("[dsp test] %s,add buffer:%d\n", __FUNCTION__, buf0.buf_id);
        csi_dsp_task_release_request(req);
        pthread_exit(-1);
    } else {
        printf("[dsp test] req request add buffer success, buf0.\n");
    }

    if (param.dsp_plane_cnt == 1 || param.dsp_plane_cnt == 2 || param.dsp_plane_cnt == 3) {
        for (i = 0; i < buf0.planes[0].size/4; i++) {
            ((int *)(buf0.planes[0].buf_vir))[i] = rand();
        }
    }
    if (param.dsp_plane_cnt == 2 || param.dsp_plane_cnt == 3) {
        for (i = 0; i < buf0.planes[1].size/4; i++) {
            ((int *)(buf0.planes[1].buf_vir))[i] = rand();
        }
    }
    if (param.dsp_plane_cnt == 3) {
        for (i = 0 ; i < buf0.planes[2].size/4; i++) {
            ((int *)(buf0.planes[2].buf_vir))[i] = rand();
        }
    }
    
    struct csi_dsp_buffer buf1 = {
        .buf_id           = 1,
        .dir              = CSI_DSP_BUFFER_OUT,
        .type             = CSI_DSP_BUF_ALLOC_DRV,
        .plane_count      = param.dsp_plane_cnt,
        .width            = param.dsp_buf_width,
        .height           = param.dsp_buf_height,
        .planes[0].stride = param.dsp_buf_width,
        .planes[0].size   = param.dsp_buf_width*param.dsp_buf_height,
        .planes[1].stride = param.dsp_buf_width,
        .planes[1].size   = param.dsp_buf_width*param.dsp_buf_height,
        .planes[2].stride = param.dsp_buf_width,
        .planes[2].size   = param.dsp_buf_width*param.dsp_buf_height,
    };

    if (csi_dsp_request_add_buffer(req, &buf1)) {
        printf("[dsp test] %s,add buffer:%d\n", __FUNCTION__, buf1.buf_id);
        csi_dsp_task_release_request(req);
        pthread_exit(-1);
    } else {
        printf("[dsp test] req request add buffer success, buf1.\n");
    }

    if (csi_dsp_request_enqueue(req)) {
        printf("[dsp test] %s,req enqueu fail:%d\n", __FUNCTION__);
        csi_dsp_task_release_request(req);
        pthread_exit(-1);
    } else {
        printf("[dsp test] req request enqueque success.\n");
    }

    req = csi_dsp_request_dequeue(task);
    if( req == NULL && req->status != CSI_DSP_SW_REQ_DONE ) {
        printf("[dsp test] %s,req dequeue fail:%d\n", __FUNCTION__);
        pthread_exit(-1);
    } else {
        printf("[dsp test] req request dequeque success.\n");
    }

    if( param.dsp_plane_cnt == 1 || param.dsp_plane_cnt == 2 || param.dsp_plane_cnt == 3 ) {
        if( memcmp((void*)buf0.planes[0].buf_vir, (void *)buf1.planes[0].buf_vir, buf0.planes[0].size) ) {
            printf("[dsp test] cmp fail, line num %d\n",__LINE__);
            csi_dsp_task_release_request(req);
            csi_dsp_destroy_task(task);
            csi_dsp_delete_instance(instance);
            pthread_exit(-1);
        }
    }
    if ( param.dsp_plane_cnt == 2 || param.dsp_plane_cnt == 3 ) {
        if( memcmp((void*)buf0.planes[1].buf_vir, (void *)buf1.planes[1].buf_vir, buf0.planes[1].size) ) {
            printf("[dsp test] cmp fail, line num %d\n",__LINE__);
            csi_dsp_task_release_request(req);
            csi_dsp_destroy_task(task);
            csi_dsp_delete_instance(instance);
            pthread_exit(-1);;
        }
    }
    if ( param.dsp_plane_cnt == 3 ) {
        if( memcmp((void*)buf0.planes[2].buf_vir, (void *)buf1.planes[2].buf_vir, buf0.planes[2].size )) {
            printf("[dsp test] cmp fail, line num %d\n",__LINE__);
            csi_dsp_task_release_request(req);
            csi_dsp_destroy_task(task);
            csi_dsp_delete_instance(instance);
            pthread_exit(-1);;
        }
    }
    csi_dsp_task_release_request(req);
    csi_dsp_destroy_task(task);
    csi_dsp_delete_instance(instance);
    pthread_exit(0);
}

int main(int argc, char *argv[]) {
    printf("********************************\n");
    printf("[dsp test] kick off test\n");
    printf("********************************\n");
    
    pthread_t thread1,   thread2;
    int       ret_thrd1, ret_thrd2;

    if (argc < 6) {
        printf("[dsp test] please provide parameter in following format:\n");
        printf("  ./dsp_test dsp_id plane_cnt algo_id buf_width buf_height thread_num.");
        exit(-1);
    }

    int dsp_instance_id = atoi(argv[1]);
    int dsp_plane_cnt   = atoi(argv[2]);
    char *algo_name     = (argv[3]);
    int dsp_algo_id     = atoi(argv[3]);
    int dsp_buf_width   = atoi(argv[4]);
    int dsp_buf_height  = atoi(argv[5]);
    int test_thd_num    = atoi(argv[6]);





    printf("==========  TEST CONFIGURATIONS  ==========\n");
    printf(" dsp_id         : %d\n", dsp_instance_id);
    printf(" plane_num      : %d\n", dsp_plane_cnt);
    printf(" algo_name       : %s\n", algo_name);
    printf(" buf_width      : %d\n", dsp_buf_width);
    printf(" buf_height     : %d\n", dsp_buf_height);
    printf(" test_thd_num   : %d\n", test_thd_num);
    printf("==========================================\n");

    struct Param param = {
        .dsp_instance_id = dsp_instance_id,
        .dsp_plane_cnt   = dsp_plane_cnt,
        .algo_name       = algo_name,
        .dsp_algo_id     = dsp_algo_id,
        .dsp_buf_width   = dsp_buf_width,
        .dsp_buf_height  = dsp_buf_height
    };

    printf("[dsp test] Dsp Post Process Test Start !\n");
    void *thread_res;
    int res;
    int ret;
    
    clock_gettime(CLOCK_REALTIME, &start_time);
    pthread_t * thread = malloc(sizeof(pthread_t)*test_thd_num);
    for(int i = 0; i < test_thd_num; i++) {
        ret_thrd1 = pthread_create(&thread[i], NULL, (void *)&create_dsp_task, (void*)&param);
    }
    for(int i = 0; i < test_thd_num; i++) {
        res = pthread_join(thread[i],(void*)&thread_res);
        clock_gettime(CLOCK_REALTIME, &end_time);
        ret = (int *)thread_res;
        printf("==========================================\n");
        if(ret == 0) {
            printf("[dsp test] thread %d,Test Pass!\n", i);
        } else {
            printf("[dsp test] thread %d,Test Fail!\n", i);
        }
        printf("==========================================\n");
    }
    printf("[dsp test] total time cost   : %d ms\n",
                (end_time.tv_sec - start_time.tv_sec) * 1000 +
                (end_time.tv_nsec - start_time.tv_nsec) / 1000000);
    
    return 0;
}
