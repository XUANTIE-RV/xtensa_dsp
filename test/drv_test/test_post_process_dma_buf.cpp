
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include "csi_dsp_api.h"
#include "csi_dsp_task_defs.h"
#include "csi_dsp_post_process_defs.h"

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "video_mem.h"
#ifdef __cplusplus
}
#endif
struct buf_param{
    int with;
    int height;
    int stride;
    int plane_num;
};

TEST_GROUP(DspPostProcessTestDmaBuf)
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
    if( VMEM_create(&mem_allocor) <0)
    {
        FAIL_TEST("open mem_alloc_fd fail\n");
    }
  }
  void teardown()
  {
    csi_dsp_destroy_task(task);
    csi_dsp_delete_instance(instance);
    VMEM_destroy(mem_allocor);

  }
  void *instance;
  void *mem_allocor;
  void *task;

  int reqDmaBuffer(VmemParams *params)
  {

    int pgsize = getpagesize();
    params->size = ((params->size+pgsize-1)/pgsize)*pgsize;
    if(VMEM_allocate(mem_allocor, params))
    {
        return -1;
    }

    printf("%s,alloct dma buf @ phy:%lx\n",__FUNCTION__,params->phy_address);
    if(VMEM_export(mem_allocor,params))
    {
        return -1;
    }
    printf("%s,export dma buf @fd:%x\n",__FUNCTION__,params->fd);
    // if(VMEM_mmap(mem_allocor,params))
    // {
    //     return -1;
    // }
    // printf("%s,mmap dma buf addr:%lx\n",__FUNCTION__,params->vir_address);
    return 0;

  }  
  int releaseDmaBuffer(VmemParams *params)
  {
      VMEM_free(mem_allocor, params);
  }
  int oneRequsetHelper(int with,int height,int stride,int plane_num,char *name)
  {
        int i=0;
        int j=0;
        int ret =0 ;
        struct timeval time_enqueue;
        struct timeval time_dequeue;
        VmemParams params_in[3];
        VmemParams params_out[3];

        csi_dsp_algo_load_req_t alog_config={
            .algo_id=0,
        };

        if(name != NULL)
        {
            if(csi_dsp_task_acquire_algo(task,name))
            {
                FAIL_TEST("algo kernel load fail\n");
            }
        }
        else 
        {
            if(csi_dsp_task_load_algo(task,&alog_config))
            {
                FAIL_TEST("algo kernel load fail\n");
            }
        }

        struct csi_sw_task_req* req=NULL;
        req =csi_dsp_task_create_request(task);
        if(req==NULL)
        {
            FAIL_TEST("req create fail\n");
        }
       for(i=0;i<plane_num;i++)
        {
            params_in[i].size = stride*height;
            params_in[i].flags = VMEM_FLAG_CONTIGUOUS;
            if(reqDmaBuffer(&params_in[i]))
            {
                FAIL_TEST("req dma buf fail\n");
            }
        }

        struct csi_dsp_buffer buf1;

    
        buf1.buf_id = 0;
        buf1.dir = CSI_DSP_BUFFER_IN;
        buf1.type = CSI_DSP_BUF_TYPE_DMA_BUF_IMPORT;
        buf1.plane_count = plane_num;
        buf1.width =with;
        buf1.height =height;
        for(i=0;i<buf1.plane_count;i++)
        {
            buf1.planes[i].stride= stride;
            buf1.planes[i].size= params_in[i].size;
            buf1.planes[i].fd = params_in[i].fd;
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

       for(i=0;i<plane_num;i++)
        {
            params_out[i].size = stride*height;
            params_out[i].flags = VMEM_FLAG_CONTIGUOUS;
            if(reqDmaBuffer(&params_out[i]))
            {
                FAIL_TEST("req dma buf fail\n");
            }
        }
        struct csi_dsp_buffer buf2;
        buf2.buf_id = 1;
        buf2.dir = CSI_DSP_BUFFER_OUT;
        buf2.type = CSI_DSP_BUF_TYPE_DMA_BUF_IMPORT;
        buf2.plane_count = plane_num;
        buf2.width =with;
        buf2.height =height;

        for(i=0;i<buf2.plane_count;i++)
        {
            buf2.planes[i].stride= stride;
            buf2.planes[i].size=  params_out[i].size;
            buf2.planes[i].fd = params_out[i].fd;
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
        // memset((void *)buf2.planes[0].buf_vir,0xff,16);
        for(i=0;i<buf2.plane_count;i++)
        {
            ret |= memcmp((void*)buf1.planes[i].buf_vir,(void *)buf2.planes[i].buf_vir,buf2.planes[i].size);
            printf("compare plane:%d, buf1:%d,buf2:%d,size:%d\n",i,((int*)buf1.planes[i].buf_vir)[0],((int*)buf2.planes[i].buf_vir)[0],buf2.planes[i].size);
        }

        CHECK_EQUAL_ZERO(ret);
        csi_dsp_task_release_request(req);
        for(i=0;i<plane_num;i++)
        {
            releaseDmaBuffer(&params_in[i]);
            releaseDmaBuffer(&params_out[i]);
        }

        return 0;
    }


};

TEST(DspPostProcessTestDmaBuf,oneProcessReq_640_480_fl_lib)
{
    CHECK_EQUAL_ZERO(oneRequsetHelper(640,480,640,1,"dsp_dummy_algo_flo"));

}

TEST(DspPostProcessTestDmaBuf,oneProcessReq_640_480_pi_lib)
{
    CHECK_EQUAL_ZERO(oneRequsetHelper(640,480,640,1,"dsp_dummy_algo_pisl"));

}


TEST(DspPostProcessTestDmaBuf,oneProcessReq_1920_1080_fl_lib)
{

    CHECK_EQUAL_ZERO(oneRequsetHelper(1920,1080,1920,1,"dsp_dummy_algo_flo"));

}

TEST(DspPostProcessTestDmaBuf,oneProcessReq_4096_2160)
{

    CHECK_EQUAL_ZERO(oneRequsetHelper(4096,2160,4096,1,NULL));

}

TEST(DspPostProcessTestDmaBuf,oneProcessReq_640_480)
{
    CHECK_EQUAL_ZERO(oneRequsetHelper(640,480,640,1,NULL));
}

TEST(DspPostProcessTestDmaBuf,oneProcessReq_multi_planes)
{

    CHECK_EQUAL_ZERO(oneRequsetHelper(640,480,640,3,NULL));

}


