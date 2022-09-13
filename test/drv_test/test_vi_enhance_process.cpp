
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

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
TEST_GROUP(DspViEnhanceProcessTest)
{
  void setup()
  {
	instance = csi_dsp_create_instance(0);
	if(!instance)
	{
		FAIL_TEST("create fail\n");

	}
    if( VMEM_create(&mem_allocor) <0)
    {
        FAIL_TEST("open mem_alloc_fd fail\n");
    }
  }
  void teardown()
  {
    // csi_dsp_destroy_task(task);
    csi_dsp_delete_instance(instance);
       VMEM_destroy(mem_allocor);
  }

  int reqDmaBuffer(VmemParams *params)
  {

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
    return 0;

  }  
  int releaseDmaBuffer(VmemParams *params)
  {
      VMEM_free(mem_allocor, params);
  }
  void *instance;
  void *mem_allocor;
//   void *task;
};

TEST(DspViEnhanceProcessTest,Isp2Dsp2RyProcessTestBasic)
{

	void *vi_task= csi_dsp_create_task(instance,CSI_DSP_TASK_HW_TO_HW);
	if(!vi_task)
	{
		FAIL_TEST("task create fail\n");
	}
	if(csi_dsp_create_reporter(instance))
	{
		FAIL_TEST("reporter create fail\n");
	}
	struct csi_dsp_task_fe_para config_params;
	
		config_params.frontend_type = CSI_DSP_FE_TYPE_ISP;
		config_params.task_id = -1;
		config_params.isp_param.id=0;
        config_params.isp_param.hor=640;
        config_params.isp_param.ver=480;
        config_params.isp_param.data_fmt=CSI_DSP_IMG_FMT_RAW12_ALGIN;
        config_params.isp_param.line_in_entry=640*2;
        config_params.isp_param.line_stride=640*2;
        config_params.isp_param.buffer_size=640*2*16*2;
        config_params.isp_param.buffer_addr=0xb0000000;

	if(csi_dsp_task_config_frontend(vi_task,&config_params))
	{
		FAIL_TEST("isp config fail\n");
	}

	struct csi_dsp_task_be_para post_config;

    post_config.backend_type = CSI_DSP_BE_TYPE_POST_ISP;
    post_config.task_id = -1;

    post_config.post_isp_param.id=0;
    post_config.post_isp_param.hor=640;
    post_config.post_isp_param.ver=480;
    post_config.post_isp_param.data_fmt=CSI_DSP_IMG_FMT_RAW12_ALGIN;
    post_config.post_isp_param.line_in_entry=640*2;
    post_config.post_isp_param.line_stride=640*2;
    post_config.post_isp_param.buffer_size=640*2*16*2;
    post_config.post_isp_param.buffer_addr=0xb0000000;


	if(csi_dsp_task_config_backend(vi_task,&post_config))
	{

		FAIL_TEST("post-isp config fail\n");
	}

    struct csi_dsp_algo_config_par alog_config={
        .algo_id=0,
        .sett_ptr =NULL,
        .sett_length =0,
	};

	if(csi_dsp_task_config_algo(vi_task,&alog_config))
	{
		FAIL_TEST("algo kernel config fail\n");
	}

	if(csi_dsp_task_register_cb(vi_task,NULL,NULL,32))
	{
		FAIL_TEST("algo kernel start fail\n");
	}

	if(csi_dsp_task_start(vi_task))
	{
		FAIL_TEST("task  start fail\n");
	}
    if(csi_dsp_task_stop(vi_task))
    {
        FAIL_TEST("task  stop fail\n");
    }

    if(csi_dsp_ps_task_unregister_cb(vi_task))
	{
		FAIL_TEST("unregister_cb fail\n");
	}

    if(csi_dsp_destroy_reporter(instance))
    {
        FAIL_TEST("reporter destroy fail\n");
    }
    csi_dsp_destroy_task(vi_task);

}

TEST(DspViEnhanceProcessTest,Isp2Dsp2RyProcessTestWithPorperty)
{

	void *vi_task= csi_dsp_create_task(instance,CSI_DSP_TASK_HW_TO_HW);
	if(!vi_task)
	{
		FAIL_TEST("task create fail\n");
	}
	if(csi_dsp_create_reporter(instance))
	{
		FAIL_TEST("reporter create fail\n");
	}
	struct csi_dsp_task_fe_para config_params;
	
		config_params.frontend_type = CSI_DSP_FE_TYPE_ISP;
		config_params.task_id = -1;
		config_params.isp_param.id=0;
        config_params.isp_param.hor=640;
        config_params.isp_param.ver=480;
        config_params.isp_param.data_fmt=CSI_DSP_IMG_FMT_RAW12_ALGIN;
        config_params.isp_param.line_in_entry=640*2;
        config_params.isp_param.line_stride=640*2;
        config_params.isp_param.buffer_size=640*2*16*2;
        config_params.isp_param.buffer_addr=0xb0000000;

	if(csi_dsp_task_config_frontend(vi_task,&config_params))
	{
		FAIL_TEST("isp config fail\n");
	}

	struct csi_dsp_task_be_para post_config;

    post_config.backend_type = CSI_DSP_BE_TYPE_POST_ISP;
    post_config.task_id = -1;

    post_config.post_isp_param.id=0;
    post_config.post_isp_param.hor=640;
    post_config.post_isp_param.ver=480;
    post_config.post_isp_param.data_fmt=CSI_DSP_IMG_FMT_RAW12_ALGIN;
    post_config.post_isp_param.line_in_entry=640*2;
    post_config.post_isp_param.line_stride=640*2;
    post_config.post_isp_param.buffer_size=640*2*16*2;
    post_config.post_isp_param.buffer_addr=0xb0000000;


	if(csi_dsp_task_config_backend(vi_task,&post_config))
	{

		FAIL_TEST("post-isp config fail\n");
	}

    struct algo_property{
        uint32_t width;
        uint32_t height;
    };

    struct algo_property sett {
        .width = 640,
        .height = 480,
    };

	struct csi_dsp_algo_config_par alog_config={
		.algo_id=0,
        .sett_ptr =(uint64_t)&sett,
        .sett_length =sizeof(struct algo_property),
	};

	if(csi_dsp_task_config_algo(vi_task,&alog_config))
	{
		FAIL_TEST("algo kernel config fail\n");
	}

	if(csi_dsp_task_register_cb(vi_task,NULL,NULL,32))
	{
		FAIL_TEST("register_cb fail\n");
	}

	if(csi_dsp_task_start(vi_task))
	{
		FAIL_TEST("task start fail\n");
	}

    if(csi_dsp_task_stop(vi_task))
    {
        FAIL_TEST("task  stop fail\n");
    }

    if(csi_dsp_ps_task_unregister_cb(vi_task))
	{
		FAIL_TEST("unregister_cb fail\n");
	}

    if(csi_dsp_destroy_reporter(instance))
    {
        FAIL_TEST("reporter destroy fail\n");
    }

    csi_dsp_destroy_task(vi_task);

}



TEST(DspViEnhanceProcessTest,Isp2Dsp2RyProcessMini)
{

	void *vi_task= csi_dsp_create_task(instance,CSI_DSP_TASK_HW_TO_HW);
	if(!vi_task)
	{
		FAIL_TEST("task create fail\n");
	}
	if(csi_dsp_create_reporter(instance))
	{
		FAIL_TEST("reporter create fail\n");
	}

    struct algo_property{
        uint32_t width;
        uint32_t height;
    };

    struct algo_property sett {
        .width = 1280,
        .height = 960,
    };

	struct csi_dsp_algo_config_par alog_config={
		.algo_id=0,
        .sett_ptr =(uint64_t)&sett,
        .sett_length =sizeof(struct algo_property),
	};

	if(csi_dsp_task_config_algo(vi_task,&alog_config))
	{
		FAIL_TEST("algo kernel config fail\n");
	}

	if(csi_dsp_task_register_cb(vi_task,NULL,NULL,32))
	{
		FAIL_TEST("register_cb fail\n");
	}


    if(csi_dsp_ps_task_unregister_cb(vi_task))
	{
		FAIL_TEST("unregister_cb fail\n");
	}

    if(csi_dsp_destroy_reporter(instance))
    {
        FAIL_TEST("reporter destroy fail\n");
    }

    csi_dsp_destroy_task(vi_task);

}


TEST(DspViEnhanceProcessTest,Vipre2Dsp2CpuProcessTestWithDmaBuf)
{

	void *vi_task= csi_dsp_create_task(instance,CSI_DSP_TASK_HW_TO_SW);
	if(!vi_task)
	{
		FAIL_TEST("task create fail\n");
	}
	if(csi_dsp_create_reporter(instance))
	{
		FAIL_TEST("reporter create fail\n");
	}
	struct csi_dsp_task_fe_para config_params;
	
		config_params.frontend_type = CSI_DSP_FE_TYPE_VIPRE;
		config_params.task_id = -1;
		config_params.vipre_param.act_channel= 1;
        config_params.vipre_param.hor=640;
        config_params.vipre_param.ver=480;
        config_params.vipre_param.data_fmt=CSI_DSP_IMG_FMT_RAW12_ALGIN;
        config_params.vipre_param.line_num=0;
        config_params.vipre_param.line_stride=640*2;
        config_params.vipre_param.buffer_size=640*2*16*2;
        config_params.vipre_param.buffer_num =1;
        config_params.vipre_param.buffer_addr[0]=0xb0000000;

	if(csi_dsp_task_config_frontend(vi_task,&config_params))
	{
		FAIL_TEST("vipre config fail\n");
	}
    int i,j;
	struct csi_dsp_task_be_para * host_config;
    VmemParams params;    

    host_config = (struct csi_dsp_task_be_para *)malloc(sizeof(struct csi_dsp_task_be_para)+3*sizeof(struct csi_dsp_buffer));
    int width = 640;
    int stride = 640;
    int height = 1280;
    host_config->backend_type = CSI_DSP_BE_TYPE_HOST;
    host_config->task_id = -1;
    host_config->sw_param.num_buf = 3;
    for(int i=0;i<host_config->sw_param.num_buf ;i++)
    {

    
        host_config->sw_param.bufs[i].buf_id = i;
        host_config->sw_param.bufs[i].dir = CSI_DSP_BUFFER_OUT;
        host_config->sw_param.bufs[i].type = CSI_DSP_BUF_TYPE_DMA_BUF_IMPORT;
        host_config->sw_param.bufs[i].plane_count = 1;
        host_config->sw_param.bufs[i].width =width;
        host_config->sw_param.bufs[i].height =height;
        for(j=0;j<host_config->sw_param.bufs[i].plane_count;j++)
        {
            params.size = stride*height;
            params.flags = VMEM_FLAG_CONTIGUOUS;
            if(reqDmaBuffer(&params))
            {
                FAIL_TEST("req dma buf fail\n");
            }
            

            host_config->sw_param.bufs[i].planes[j].stride= stride;
            host_config->sw_param.bufs[i].planes[j].size= params.size;
            host_config->sw_param.bufs[i].planes[j].fd = params.fd;
        }
        if(csi_dsp_task_create_buffer(vi_task,&host_config->sw_param.bufs[i]))
        {
              FAIL_TEST("create buf fail\n");
        }
    }


	if(csi_dsp_task_config_backend(vi_task,host_config))
	{
        free(host_config);
		FAIL_TEST("post-isp config fail\n");
	}


    struct csi_dsp_algo_config_par alog_config={
        .algo_id=0,
        .sett_ptr =NULL,
        .sett_length =0,
	};

	if(csi_dsp_task_config_algo(vi_task,&alog_config))
	{
		FAIL_TEST("algo kernel config fail\n");
	}

	if(csi_dsp_task_register_cb(vi_task,NULL,NULL,32))
	{
		FAIL_TEST("algo kernel start fail\n");
	}

	if(csi_dsp_task_start(vi_task))
	{
		FAIL_TEST("task  start fail\n");
	}
    if(csi_dsp_task_stop(vi_task))
    {
        FAIL_TEST("task  stop fail\n");
    }

    for(int i=0;i<host_config->sw_param.num_buf;i++)
    {
        if(csi_dsp_task_free_buffer(vi_task,&host_config->sw_param.bufs[i]))
        {
            FAIL_TEST("Dma buffer free fail\n");
        }
        params.phy_address = host_config->sw_param.bufs[i].planes[0].buf_phy;
        releaseDmaBuffer(&params);
    }
    free(host_config);
    if(csi_dsp_ps_task_unregister_cb(vi_task))
	{
		FAIL_TEST("unregister_cb fail\n");
	}

    if(csi_dsp_destroy_reporter(instance))
    {
        FAIL_TEST("reporter destroy fail\n");
    }
    csi_dsp_destroy_task(vi_task);

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
