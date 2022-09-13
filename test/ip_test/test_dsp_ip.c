
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include "../../driver/xrp-user/dsp-ps/csi_dsp_core.h"

#define PAYLOAD_SIZE   32

struct message{
	int cmd;
	char message[PAYLOAD_SIZE];
};

// void vi_callback( void *context,void * data)
// {}
int main(int argc, char *argv[])
{

	void *instance = csi_dsp_create_instance(0);

	if(!instance)
	{
		printf("create fail\n");
		return -1;
	}
    void * buf = malloc(1024);
    if(buf==NULL)
    {
        printf("malloc fail\n");
        return 0;
    }

    struct csi_dsp_ip_test_par config_para={
            .case_goup="VectorizationTest",
            .result_buf_size=1024,
    };
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    printf("Start testsuite %s .......\n",config_para.case_goup);
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    if(csi_dsp_test_config(instance,&config_para,buf))
    {
        printf("Suite Fail NO RESP\n");
    }
    printf("%s",buf);


	return 0;
}


// int test(int argc, char *argv[])
// {

// 	unsigned char comm_task[] = XRP_PS_NSID_INITIALIZER;
// 	struct dsp_instnace *instance = create_dsp_instance(0,comm_task);
// 	if(!instance)
// 	{
// 		printf("create fail\n");
// 		return -1;
// 	}
// 	if(dsp_ps_create_reporter(instance))
// 	{
// 		printf("reporter create fail\n");
// 		return -1;
// 	}
// 	unsigned char isp_task[] = XRP_PS_NSID_ISP_ALGO;
// 	struct csi_dsp_ps_task_handler *vi_task= csi_dsp_create_task(instance,isp_task,1);
// 	if(!vi_task)
// 	{
// 		printf("task create fail\n");
// 		return -1;
// 	}

// 	sisp_config_par isp_config =
// 	{
// 		.id=0,
// 		.hor=640,
// 		.ver=480,
// 		.data_fmt=IMG_RAW12_FORMAT_ALGIN1,
// 		.line_in_entry=640*2,
// 		.line_stride=640*2,
// 		.buffer_size=640*2*16*2,
// 		.buffer_addr=0xb0000000,

// 	};
//     struct csi_dsp_task_fe_para *fe_cfg=malloc(sizeof(struct csi_dsp_task_fe_para)+sizeof(sisp_config_par)-1);
	
// 	fe_cfg->frontend_type=CSI_DSP_FE_TYPE_ISP;
// 	memcpy(fe_cfg->para_data,&isp_config,siezof(sisp_config_par));
// 	if(csi_dsp_task_config_frontend(vi_task,fe_cfg))
// 	{

// 		printf("isp config fail\n");
// 		return -1;
// 	}
// 	spost_isp_config_par post_config =
// 	{
// 		.id=0,
// 		.hor=640,
// 		.ver=480,
// 		.data_fmt=IMG_RAW12_FORMAT_ALGIN1,
// 		.line_in_entry=640*2,
// 		.line_stride=640*2,
// 		.buffer_size=640*2*16*2,
// 		.buffer_addr=0xb0000000,

// 	};
//  	struct csi_dsp_task_be_para *be_cfg=malloc(sizeof(struct csi_dsp_task_be_para)+sizeof(spost_isp_config_par)-1);
// 	be_cfg->CSI_DSP_BE_TYPE_POST_ISP;
// 	memcpy(fe_cfg->para_data,&post_config,siezof(sisp_config_par));
// 	if(csi_dsp_task_config_backend(vi_task,&be_cfg))
// 	{

// 		printf("post-isp config fail\n");
// 		return -1;
// 	}

// 	salgo_config_par alog_config={
// 		.algo_id=0,
// 	};

// 	if(csi_dsp_task_config_algo(vi_task,&alog_config))
// 	{
// 		printf("algo kernel config fail\n");
// 		return -1;
// 	}
// 	vi_task->report_id=1;
// 	if(csi_dsp_task_start(vi_task,isp_algo_result_handler,NULL,32))
// 	{
// 		printf("algo kernel start fail\n");
// 		return -1;
// 	}

// 	while(1)
// 	{
// 		;
// 	}
// 	csi_dsp_task_stop(vi_task);
// 	csi_dsp_destroy_task(vi_task);
// 	csi_dsp_delete_instance(vi_task);
// 	return 0;
// }