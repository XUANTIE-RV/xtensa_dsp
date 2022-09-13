
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "csi_dsp_api.h"
#include "csi_dsp_task_defs.h"
#include "csi_dsp_post_process_defs.h"

#define PAYLOAD_SIZE   32

struct message{
	int cmd;
	char message[PAYLOAD_SIZE];
};

int main(int argc, char *argv[])
{
    printf("Dsp Post Process Test Start !\n");
	void *instance = csi_dsp_create_instance(0);
	if(!instance)
	{
		printf("create fail\n");
		return -1;
	}

	void *task= csi_dsp_create_task(instance,CSI_DSP_TASK_SW_TO_SW);
	if(!task)
	{
		printf("task create fail\n");
		return -1;
	}
#if 0
    csi_dsp_algo_load_req_t alog_config={
		.algo_id=0,
	};

	if(csi_dsp_task_load_algo(task,&alog_config))
	{
		printf("algo kernel load fail\n");
        return -1;

	}
#else 
    if(csi_dsp_task_acquire_algo(task,"dsp_dummy_algo_pisl"))
    {
        printf("algo kernel load fail\n");
        return -1;

    }

#endif
    struct csi_sw_task_req* req=NULL;
    req =csi_dsp_task_create_request(task);
    if(req==NULL)
    {
        printf("req create fail\n");
		return -1;
    }
    struct csi_dsp_buffer buf1 = 
    {
        .buf_id = 0,
        .dir = CSI_DSP_BUFFER_IN,
        .type = CSI_DSP_BUF_ALLOC_DRV,
        .plane_count = 1,
        .width =640,
        .height =480,
        .planes[0].stride= 640,
        .planes[0].size= 640*480,
    };

    if(csi_dsp_request_add_buffer(req,&buf1))
    {
        printf("%s,add buffer:%d\n",__FUNCTION__,buf1.buf_id);
        csi_dsp_task_release_request(req);
        return -1;
    }
    int i=0;
    for(i=0;i<buf1.planes[0].size/4;i++)
    {
        ((int *)(buf1.planes[0].buf_vir))[i]=rand();
    }
    struct csi_dsp_buffer buf2 = 
    {
        .buf_id = 1,
        .dir = CSI_DSP_BUFFER_OUT,
        .type = CSI_DSP_BUF_ALLOC_DRV,
        .plane_count = 1,
        .width =640,
        .height =480,
        .planes[0].stride= 640,
        .planes[0].size= 640*480,
    };

    if(csi_dsp_request_add_buffer(req,&buf2))
    {
        printf("%s,add buffer:%d\n",__FUNCTION__,buf1.buf_id);
        csi_dsp_task_release_request(req);
        return -1;
    }

    struct setting{
        int with;
        int height;
    }setting_config;
    setting_config.height = 480;
    setting_config.with =640;
    if(csi_dsp_request_set_property(req,&setting_config,sizeof(setting_config)))
    {
        printf("%s,seting req fail:%d\n",__FUNCTION__);
        csi_dsp_task_release_request(req);
        return -1;
    }

    if(csi_dsp_request_enqueue(req))
    {
        printf("%s,req enqueu fail:%d\n",__FUNCTION__);
        csi_dsp_task_release_request(req);
        return -1;
    }

    req = csi_dsp_request_dequeue(task);
    if(req==NULL && req->status != CSI_DSP_SW_REQ_DONE)
    {
        printf("%s,req dequeue fail:%d\n",__FUNCTION__);
        return -1;
    }
    if(memcmp((void*)buf1.planes[0].buf_vir,(void *)buf2.planes[0].buf_vir,buf1.planes[0].size))
    {
        printf("%s,cmp fail\n",__FUNCTION__);
        csi_dsp_task_release_request(req);
        csi_dsp_destroy_task(task);
        csi_dsp_delete_instance(instance);
        return -1;
    }

    csi_dsp_task_release_request(req);
    csi_dsp_destroy_task(task);
    csi_dsp_delete_instance(instance);
    printf("%s,Test Pass!\n",__FUNCTION__);
	return 0;
}
