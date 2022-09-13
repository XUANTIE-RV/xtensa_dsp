
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include "csi_dsp_api.h"
#include "csi_dsp_task_defs.h"
#include "csi_dsp_post_process_defs.h"

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

struct buf_param{
    int with;
    int height;
    int stride;
    int plane_num;
};

TEST_GROUP(DspPostProcessTestBasic)
{
  void setup()
  {
	instance = csi_dsp_create_instance(0);
	if(!instance)
	{
		FAIL_TEST("create fail\n");

	}
    task= csi_dsp_create_task(instance,CSI_DSP_TASK_SW_TO_SW);
	if(!task)
	{
		FAIL_TEST("task create fail\n");
	}
  }
  void teardown()
  {
    csi_dsp_destroy_task(task);
    csi_dsp_delete_instance(instance);
  }
  void *instance;
  void *task;

  int oneRequsetHelper(int with,int height,int stride,int plane_num)
  {
        int i=0;
        int j=0;
        int ret =0 ;
        struct timeval time_enqueue;
        struct timeval time_dequeue;
        csi_dsp_algo_load_req_t alog_config={
            .algo_id=0,
        };

        if(csi_dsp_task_load_algo(task,&alog_config))
        {
            FAIL_TEST("algo kernel load fail\n");

        }

        struct csi_sw_task_req* req=NULL;
        req =csi_dsp_task_create_request(task);
        if(req==NULL)
        {
            FAIL_TEST("req create fail\n");
        }
        struct csi_dsp_buffer buf1;

    
        buf1.buf_id = 0;
        buf1.dir = CSI_DSP_BUFFER_IN;
        buf1.type = CSI_DSP_BUF_ALLOC_DRV;
        buf1.plane_count = plane_num;
        buf1.width =with;
        buf1.height =height;
        for(i=0;i<buf1.plane_count;i++)
        {
            buf1.planes[i].stride= stride;
            buf1.planes[i].size= stride*buf1.height;
        }

    

        if(csi_dsp_request_add_buffer(req,&buf1))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }

        int *buf;

        for(j=0;j<buf1.plane_count;j++)
        {
            buf = (int *)buf1.planes[j].buf_vir;
            for(i=0;i<buf1.planes[j].size/4;i++)
            {
                buf[i]=rand();
            }
            printf("plane：%d, buf1 data:%d\n",j,buf[0]);
        }


        struct csi_dsp_buffer buf2;
        buf2.buf_id = 1;
        buf2.dir = CSI_DSP_BUFFER_OUT;
        buf2.type = CSI_DSP_BUF_ALLOC_DRV;
        buf2.plane_count = plane_num;
        buf2.width =with;
        buf2.height =height;

        for(i=0;i<buf2.plane_count;i++)
        {
            buf2.planes[i].stride= stride;
            buf2.planes[i].size= stride*buf2.height;
        }
    

        if(csi_dsp_request_add_buffer(req,&buf2))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }

        for(j=0;j<buf2.plane_count;j++)
        {
            buf = (int *)buf2.planes[j].buf_vir;
            for(i=0;i<buf2.planes[0].size/4;i++)
            {
                buf[i]=rand();
            }
            printf("plane：%d, buf2 data:%d\n",j,buf[0]);
        }

        if(csi_dsp_request_enqueue(req))
        {
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }
        gettimeofday(&time_enqueue, 0);
        req = csi_dsp_request_dequeue(task);
        if(req==NULL && req->status != CSI_DSP_SW_REQ_DONE)
        {   
            FAIL_TEST("req dequeue fail\n");
        }
        gettimeofday(&time_dequeue, 0);

        printf("req:%d,start:%d,%d,end:%d,%d\n",req->request_id,time_enqueue.tv_sec,time_enqueue.tv_usec,time_dequeue.tv_sec,time_dequeue.tv_usec);
        // MEMCMP_EQUAL((void*)buf1.planes[0].buf_vir,(void *)buf2.planes[0].buf_vir,buf1.planes[0].size);
        for(i=0;i<buf2.plane_count;i++)
        {
            ret = memcmp((void*)buf1.planes[i].buf_vir,(void *)buf2.planes[i].buf_vir,buf2.planes[i].size);
            printf("compare plane:%d, buf1:%d,buf2:%d,size:%d\n",i,((int*)buf1.planes[i].buf_vir)[0],((int*)buf2.planes[i].buf_vir)[0],buf2.planes[i].size);
            CHECK_EQUAL_ZERO(ret);
        }

        // CHECK_EQUAL_ZERO(ret);
        csi_dsp_task_release_request(req);
        return 0;
    }

    // int task_thread_process(struct buf_param * param)
    // {

    // }

};

TEST(DspPostProcessTestBasic,oneProcessReq_640_480)
{
    CHECK_EQUAL_ZERO(oneRequsetHelper(640,480,640,1));

}


TEST(DspPostProcessTestBasic,oneProcessReq_1920_1080)
{

    CHECK_EQUAL_ZERO(oneRequsetHelper(1920,1080,1920,1));

}

TEST(DspPostProcessTestBasic,oneProcessReq_4096_2160)
{

    CHECK_EQUAL_ZERO(oneRequsetHelper(4096,2160,4096,1));

}

TEST(DspPostProcessTestBasic,oneProcessReq_multi_planes)
{

    CHECK_EQUAL_ZERO(oneRequsetHelper(640,480,640,3));

}


TEST(DspPostProcessTestBasic,oneProcessReq_multi_planes_1280_960)
{

   CHECK_EQUAL_ZERO(oneRequsetHelper(1280,960,1280,3));

}

TEST(DspPostProcessTestBasic,oneProcessReq_multi_planes_1920_1080)
{

    CHECK_EQUAL_ZERO(oneRequsetHelper(1920,1080,1920,3));

}

TEST(DspPostProcessTestBasic,MultiProcessReq)
{
    
    struct csi_sw_task_req* req_list[6];
    int loop=0;
    int req_num = sizeof(req_list)/sizeof(struct csi_sw_task_req*);
    struct csi_sw_task_req* req=NULL;

    csi_dsp_algo_load_req_t alog_config={
		.algo_id=0,
	};

	if(csi_dsp_task_load_algo(task,&alog_config))
	{
		FAIL_TEST("algo kernel load fail\n");

	}
    for(loop =0 ;loop < req_num;loop++)
    {

        req =csi_dsp_task_create_request(task);
        if(req==NULL)
        {
            FAIL_TEST("req create fail\n");
        }
        req_list[loop]=req;
        struct csi_dsp_buffer buf1;

    
        buf1.buf_id = 0;
        buf1.dir = CSI_DSP_BUFFER_IN;
        buf1.type = CSI_DSP_BUF_ALLOC_DRV;
        buf1.plane_count = 1;
        buf1.width =640;
        buf1.height =480;
        buf1.planes[0].stride= 640;
        buf1.planes[0].size= 640*480;
    

        if(csi_dsp_request_add_buffer(req,&buf1))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }
        int i=0;
        for(i=0;i<buf1.planes[0].size/4;i++)
        {
            ((int *)(buf1.planes[0].buf_vir))[i]=rand();
        }
        struct csi_dsp_buffer buf2;
        buf2.buf_id = 1;
        buf2.dir = CSI_DSP_BUFFER_OUT;
        buf2.type = CSI_DSP_BUF_ALLOC_DRV;
        buf2.plane_count = 1;
        buf2.width =640;
        buf2.height =480;
        buf2.planes[0].stride= 640;
        buf2.planes[0].size= 640*480;
    

        if(csi_dsp_request_add_buffer(req,&buf2))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }

        if(csi_dsp_request_enqueue(req))
        {
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }

    }

    for(;loop>0;loop--)
    {
        req = csi_dsp_request_dequeue(task);
        if(req==NULL && req->status != CSI_DSP_SW_REQ_DONE)
        {   
            FAIL_TEST("req dequeue fail\n");
        }
        // MEMCMP_EQUAL((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        int ret = memcmp((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        CHECK_EQUAL_ZERO(ret);
        csi_dsp_task_release_request(req);
    }

}


TEST(DspPostProcessTestBasic,MultiProcessReqEnqueueConcentration)
{
    struct csi_sw_task_req* req_list[6];
    int loop=0;
    struct csi_sw_task_req* req=NULL;
    int req_num = sizeof(req_list)/sizeof(struct csi_sw_task_req*);

    csi_dsp_algo_load_req_t alog_config={
		.algo_id=0,
	};

	if(csi_dsp_task_load_algo(task,&alog_config))
	{
		FAIL_TEST("algo kernel load fail\n");

	}
    for(loop =0 ;loop < req_num;loop++)
    {

        req =csi_dsp_task_create_request(task);
        if(req==NULL)
        {
            FAIL_TEST("req create fail\n");
        }
        req_list[loop]=req;
        struct csi_dsp_buffer buf1;

    
        buf1.buf_id = 0;
        buf1.dir = CSI_DSP_BUFFER_IN;
        buf1.type = CSI_DSP_BUF_ALLOC_DRV;
        buf1.plane_count = 1;
        buf1.width =640;
        buf1.height =480;
        buf1.planes[0].stride= 640;
        buf1.planes[0].size= 640*480;
    

        if(csi_dsp_request_add_buffer(req,&buf1))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }
        int i=0;
        for(i=0;i<buf1.planes[0].size/4;i++)
        {
            ((int *)(buf1.planes[0].buf_vir))[i]=rand();
        }
        struct csi_dsp_buffer buf2;
        buf2.buf_id = 1;
        buf2.dir = CSI_DSP_BUFFER_OUT;
        buf2.type = CSI_DSP_BUF_ALLOC_DRV;
        buf2.plane_count = 1;
        buf2.width =640;
        buf2.height =480;
        buf2.planes[0].stride= 640;
        buf2.planes[0].size= 640*480;
    

        if(csi_dsp_request_add_buffer(req,&buf2))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }


    }

    for(loop =0 ;loop < req_num;loop++)
    {
        if(csi_dsp_request_enqueue(req_list[loop]))
        {
            csi_dsp_task_release_request(req_list[loop]);
            FAIL_TEST("Add buffer:%d\n");
        }
    }

    for(;loop>0;loop--)
    {
        req = csi_dsp_request_dequeue(task);
        if(req==NULL && req->status != CSI_DSP_SW_REQ_DONE)
        {   
            FAIL_TEST("req dequeue fail\n");
        }
        // MEMCMP_EQUAL((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        int ret = memcmp((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        CHECK_EQUAL_ZERO(ret);
        csi_dsp_task_release_request(req);
    }

}


TEST(DspPostProcessTestBasic,MultiProcessReqEnqueueConcentration_1)
{
    struct csi_sw_task_req* req_list[6];
    int loop=0;
    struct csi_sw_task_req* req=NULL;
    int req_num = sizeof(req_list)/sizeof(struct csi_sw_task_req*);

    csi_dsp_algo_load_req_t alog_config={
		.algo_id=0,
	};

	if(csi_dsp_task_load_algo(task,&alog_config))
	{
		FAIL_TEST("algo kernel load fail\n");

	}
    for(loop =0 ;loop < req_num;loop++)
    {

        req =csi_dsp_task_create_request(task);
        if(req==NULL)
        {
            FAIL_TEST("req create fail\n");
        }
        req_list[loop]=req;
        struct csi_dsp_buffer buf1;

    
        buf1.buf_id = 0;
        buf1.dir = CSI_DSP_BUFFER_IN;
        buf1.type = CSI_DSP_BUF_ALLOC_DRV;
        buf1.plane_count = 1;
        buf1.width =640;
        buf1.height =480;
        buf1.planes[0].stride= 640;
        buf1.planes[0].size= 640*480;
    

        if(csi_dsp_request_add_buffer(req,&buf1))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }
        int i=0;
        for(i=0;i<buf1.planes[0].size/4;i++)
        {
            ((int *)(buf1.planes[0].buf_vir))[i]=rand();
        }
        struct csi_dsp_buffer buf2;
        buf2.buf_id = 1;
        buf2.dir = CSI_DSP_BUFFER_OUT;
        buf2.type = CSI_DSP_BUF_ALLOC_DRV;
        buf2.plane_count = 1;
        buf2.width =640;
        buf2.height =480;
        buf2.planes[0].stride= 640;
        buf2.planes[0].size= 640*480;
    

        if(csi_dsp_request_add_buffer(req,&buf2))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }


    }

    for(loop =0 ;loop < req_num;loop++)
    {
        if(csi_dsp_request_enqueue(req_list[loop]))
        {
            csi_dsp_task_release_request(req_list[loop]);
            FAIL_TEST("Add buffer:%d\n");
        }
    }

    for(;loop>0;loop--)
    {
        req = csi_dsp_request_dequeue(task);
        if(req==NULL && req->status != CSI_DSP_SW_REQ_DONE)
        {   
            FAIL_TEST("req dequeue fail\n");
        }
    }
    for(loop =0 ;loop < req_num;loop++)
    {
        req = req_list[loop];
        // MEMCMP_EQUAL((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        int ret = memcmp((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        CHECK_EQUAL_ZERO(ret);
        csi_dsp_task_release_request(req);
    }



}



TEST(DspPostProcessTestBasic,MultiProcessReqEnqueueConcentration_profile_640)
{
    struct csi_sw_task_req* req_list[6];
    struct timeval cur_time_enqueue[6];
    struct timeval cur_time_dequeue[6];
    int loop=0;
    struct csi_sw_task_req* req=NULL;
    int req_num = sizeof(req_list)/sizeof(struct csi_sw_task_req*);

    csi_dsp_algo_load_req_t alog_config={
		.algo_id=0,
	};

	if(csi_dsp_task_load_algo(task,&alog_config))
	{
		FAIL_TEST("algo kernel load fail\n");

	}
    for(loop =0 ;loop < req_num;loop++)
    {

        req =csi_dsp_task_create_request(task);
        if(req==NULL)
        {
            FAIL_TEST("req create fail\n");
        }
        req_list[loop]=req;
        struct csi_dsp_buffer buf1;

    
        buf1.buf_id = 0;
        buf1.dir = CSI_DSP_BUFFER_IN;
        buf1.type = CSI_DSP_BUF_ALLOC_DRV;
        buf1.plane_count = 1;
        buf1.width =640;
        buf1.height =480;
        buf1.planes[0].stride= 640;
        buf1.planes[0].size= 640*480;
    

        if(csi_dsp_request_add_buffer(req,&buf1))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }
        int i=0;
        for(i=0;i<buf1.planes[0].size/4;i++)
        {
            ((int *)(buf1.planes[0].buf_vir))[i]=rand();
        }
        struct csi_dsp_buffer buf2;
        buf2.buf_id = 1;
        buf2.dir = CSI_DSP_BUFFER_OUT;
        buf2.type = CSI_DSP_BUF_ALLOC_DRV;
        buf2.plane_count = 1;
        buf2.width =640;
        buf2.height =480;
        buf2.planes[0].stride= 640;
        buf2.planes[0].size= 640*480;
    

        if(csi_dsp_request_add_buffer(req,&buf2))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }

    }
 
    for(loop =0 ;loop < req_num;loop++)
    {
        if(csi_dsp_request_enqueue(req_list[loop]))
        {
            csi_dsp_task_release_request(req_list[loop]);
            FAIL_TEST("Add buffer:%d\n");
        }
        	
	    gettimeofday(&cur_time_enqueue[loop], 0);
    }

    for(;loop>0;loop--)
    {
        req = csi_dsp_request_dequeue(task);
        if(req==NULL && req->status != CSI_DSP_SW_REQ_DONE)
        {   
            FAIL_TEST("req dequeue fail\n");
        }
        gettimeofday(&cur_time_dequeue[loop], 0);
    }
    for(loop =0 ;loop < req_num;loop++)
    {
        printf("req:%d,start:%d,%d,end:%d,%d\n",req_list[loop]->request_id,cur_time_enqueue[loop].tv_sec,cur_time_enqueue[loop].tv_usec,
                                                                    cur_time_dequeue[loop].tv_sec,cur_time_dequeue[loop].tv_usec);
    }
    for(loop =0 ;loop < req_num;loop++)
    {
        req = req_list[loop];
        // MEMCMP_EQUAL((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        int ret = memcmp((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        CHECK_EQUAL_ZERO(ret);
        csi_dsp_task_release_request(req);
    }


}


TEST(DspPostProcessTestBasic,MultiProcessReqEnqueueConcentration_profile_1920)
{
    struct csi_sw_task_req* req_list[6];
    struct timeval cur_time_enqueue[6];
    struct timeval cur_time_dequeue[6];
    int loop=0;
    struct csi_sw_task_req* req=NULL;
    int req_num = sizeof(req_list)/sizeof(struct csi_sw_task_req*);

    csi_dsp_algo_load_req_t alog_config={
		.algo_id=0,
	};

	if(csi_dsp_task_load_algo(task,&alog_config))
	{
		FAIL_TEST("algo kernel load fail\n");

	}
    for(loop =0 ;loop < req_num;loop++)
    {

        req =csi_dsp_task_create_request(task);
        if(req==NULL)
        {
            FAIL_TEST("req create fail\n");
        }
        req_list[loop]=req;
        struct csi_dsp_buffer buf1;

    
        buf1.buf_id = 0;
        buf1.dir = CSI_DSP_BUFFER_IN;
        buf1.type = CSI_DSP_BUF_ALLOC_DRV;
        buf1.plane_count = 1;
        buf1.width =1920;
        buf1.height =1080;
        buf1.planes[0].stride= 1920;
        buf1.planes[0].size= 1920*1080;
    

        if(csi_dsp_request_add_buffer(req,&buf1))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }
        int i=0;
        for(i=0;i<buf1.planes[0].size/4;i++)
        {
            ((int *)(buf1.planes[0].buf_vir))[i]=rand();
        }
        struct csi_dsp_buffer buf2;
        buf2.buf_id = 1;
        buf2.dir = CSI_DSP_BUFFER_OUT;
        buf2.type = CSI_DSP_BUF_ALLOC_DRV;
        buf2.plane_count = 1;
        buf2.width =1920;
        buf2.height =1080;
        buf2.planes[0].stride= 1920;
        buf2.planes[0].size= 1920*1080;
    

        if(csi_dsp_request_add_buffer(req,&buf2))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }

    }
 
    for(loop =0 ;loop < req_num;loop++)
    {
        if(csi_dsp_request_enqueue(req_list[loop]))
        {
            csi_dsp_task_release_request(req_list[loop]);
            FAIL_TEST("Add buffer:%d\n");
        }
        	
	    gettimeofday(&cur_time_enqueue[loop], 0);
    }

    for(;loop>0;loop--)
    {
        req = csi_dsp_request_dequeue(task);
        if(req==NULL && req->status != CSI_DSP_SW_REQ_DONE)
        {   
            FAIL_TEST("req dequeue fail\n");
        }
        gettimeofday(&cur_time_dequeue[loop], 0);
    }
    uint32_t counter;
    for(loop =0 ;loop < req_num;loop++)
    {
        printf("req:%d,start:%d,%d,end:%d,%d\n",req_list[loop]->request_id,cur_time_enqueue[loop].tv_sec,cur_time_enqueue[loop].tv_usec,
                                                                    cur_time_dequeue[loop].tv_sec,cur_time_dequeue[loop].tv_usec);
    }
    for(loop =0 ;loop < req_num;loop++)
    {
        req = req_list[loop];
        // MEMCMP_EQUAL((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        int ret = memcmp((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        CHECK_EQUAL_ZERO(ret);
        csi_dsp_task_release_request(req);
    }

}

TEST(DspPostProcessTestBasic,MultiProcessReqEnqueueConcentration_profile_4096)
{
    struct csi_sw_task_req* req_list[6];
    struct timeval cur_time_enqueue[6];
    struct timeval cur_time_dequeue[6];
    int loop=0;
    struct csi_sw_task_req* req=NULL;
    int req_num = sizeof(req_list)/sizeof(struct csi_sw_task_req*);

    csi_dsp_algo_load_req_t alog_config={
		.algo_id=0,
	};

	if(csi_dsp_task_load_algo(task,&alog_config))
	{
		FAIL_TEST("algo kernel load fail\n");

	}
    for(loop =0 ;loop < req_num;loop++)
    {

        req =csi_dsp_task_create_request(task);
        if(req==NULL)
        {
            FAIL_TEST("req create fail\n");
        }
        req_list[loop]=req;
        struct csi_dsp_buffer buf1;

    
        buf1.buf_id = 0;
        buf1.dir = CSI_DSP_BUFFER_IN;
        buf1.type = CSI_DSP_BUF_ALLOC_DRV;
        buf1.plane_count = 1;
        buf1.width =4096;
        buf1.height =2160;
        buf1.planes[0].stride= 4096;
        buf1.planes[0].size= 4096*2160;
    

        if(csi_dsp_request_add_buffer(req,&buf1))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }
        int i=0;
        for(i=0;i<buf1.planes[0].size/4;i++)
        {
            ((int *)(buf1.planes[0].buf_vir))[i]=rand();
        }
        struct csi_dsp_buffer buf2;
        buf2.buf_id = 1;
        buf2.dir = CSI_DSP_BUFFER_OUT;
        buf2.type = CSI_DSP_BUF_ALLOC_DRV;
        buf2.plane_count = 1;
        buf2.width =4096;
        buf2.height =2160;
        buf2.planes[0].stride= 640;
        buf2.planes[0].size= 4096*2160;
    

        if(csi_dsp_request_add_buffer(req,&buf2))
        {
            
            csi_dsp_task_release_request(req);
            FAIL_TEST("Add buffer:%d\n");
        }

    }
 
    for(loop =0 ;loop < req_num;loop++)
    {
        if(csi_dsp_request_enqueue(req_list[loop]))
        {
            csi_dsp_task_release_request(req_list[loop]);
            FAIL_TEST("Add buffer:%d\n");
        }
        	
	    gettimeofday(&cur_time_enqueue[loop], 0);
    }

    for(;loop>0;loop--)
    {
        req = csi_dsp_request_dequeue(task);
        if(req==NULL && req->status != CSI_DSP_SW_REQ_DONE)
        {   
            FAIL_TEST("req dequeue fail\n");
        }
        gettimeofday(&cur_time_dequeue[loop], 0);
    }

    uint32_t counter;
    for(loop =0 ;loop < req_num;loop++)
    {
        printf("req:%d,start:%d,%d,end:%d,%d\n",req_list[loop]->request_id,cur_time_enqueue[loop].tv_sec,cur_time_enqueue[loop].tv_usec,
                                                                    cur_time_dequeue[loop].tv_sec,cur_time_dequeue[loop].tv_usec);
    }

    for(loop =0 ;loop < req_num;loop++)
    {
        req = req_list[loop];
        // MEMCMP_EQUAL((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        int ret = memcmp((void*)req->buffers[0].planes[0].buf_vir,(void *)req->buffers[1].planes[0].buf_vir,req->buffers[0].planes[0].size);
        CHECK_EQUAL_ZERO(ret);
        csi_dsp_task_release_request(req);
    }

}

int main( int argc, char **argv )
{

	return RUN_ALL_TESTS(argc, argv);
}

// int main(int argc, char *argv[])
// {
//     printf("Dsp Post Process Test Start !\n");
// 	void *instance = csi_dsp_create_instance(0);
// 	if(!instance)
// 	{
// 		printf("create fail\n");
// 		return -1;
// 	}

// 	void *task= csi_dsp_create_task(instance,CSI_DSP_TASK_SW_TO_SW);
// 	if(!task)
// 	{
// 		printf("task create fail\n");
// 		return -1;
// 	}

//     struct csi_dsp_algo_config_par alog_config={
// 		.algo_id=0,
// 	};

// 	if(csi_dsp_task_config_algo(task,&alog_config))
// 	{
// 		printf("algo kernel config fail\n");
// 		return -1;
// 	}

//     struct csi_sw_task_req* req=NULL;
//     req =csi_dsp_task_create_request(task);
//     if(req==NULL)
//     {
//         printf("req create fail\n");
// 		return -1;
//     }
//     struct csi_dsp_buffer buf1 = 
//     {
//         .buf_id = 0,
//         .dir = CSI_DSP_BUFFER_IN,
//         .type = CSI_DSP_BUF_ALLOC_DRV,
//         .plane_count = 1,
//         .width =640,
//         .height =480,
//         .planes[0].stride= 640,
//         .planes[0].size= 604*480,
//     };

//     if(csi_dsp_request_add_buffer(req,&buf1))
//     {
//         printf("%s,add buffer:%d\n",__FUNCTION__,buf1.buf_id);
//         csi_dsp_task_release_request(req);
//         return -1;
//     }
//     int i=0;
//     for(i=0;i<buf1.planes[0].size/4;i++)
//     {
//         ((int *)(buf1.planes[0].buf_vir))[i]=rand();
//     }
//     struct csi_dsp_buffer buf2 = 
//     {
//         .buf_id = 1,
//         .dir = CSI_DSP_BUFFER_OUT,
//         .type = CSI_DSP_BUF_ALLOC_DRV,
//         .plane_count = 1,
//         .width =640,
//         .height =480,
//         .planes[0].stride= 640,
//         .planes[0].size= 604*480,
//     };

//     if(csi_dsp_request_add_buffer(req,&buf2))
//     {
//         printf("%s,add buffer:%d\n",__FUNCTION__,buf1.buf_id);
//         csi_dsp_task_release_request(req);
//         return -1;
//     }

//     if(csi_dsp_request_enqueue(req))
//     {
//         printf("%s,req enqueu fail:%d\n",__FUNCTION__);
//         csi_dsp_task_release_request(req);
//         return -1;
//     }

//     req = csi_dsp_request_dequeue(task);
//     if(req==NULL && req->status != CSI_DSP_SW_REQ_DONE)
//     {
//         printf("%s,req dequeue fail:%d\n",__FUNCTION__);
//         return -1;
//     }
//     if(memcmp((void*)buf1.planes[0].buf_vir,(void *)buf2.planes[0].buf_vir,buf1.planes[0].size))
//     {
//         printf("%s,cmp fail\n",__FUNCTION__);
//         csi_dsp_task_release_request(req);
//         csi_dsp_destroy_task(task);
//         csi_dsp_delete_instance(instance);
//         return -1;
//     }

//     csi_dsp_task_release_request(req);
//     csi_dsp_destroy_task(task);
//     csi_dsp_delete_instance(instance);
//     printf("%s,Test Pass!\n",__FUNCTION__);
// 	return 0;
// }
