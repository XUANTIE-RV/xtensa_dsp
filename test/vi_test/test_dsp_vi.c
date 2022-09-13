
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

	void *vi_task= csi_dsp_create_task(instance,CSI_DSP_TASK_HW_TO_HW);
	if(!vi_task)
	{
		printf("task create fail\n");
		return -1;
	}
	if(csi_dsp_create_reporter(instance))
	{
		printf("reporter create fail\n");
		return -1;
	}
	struct csi_dsp_task_fe_para config_params = 
	{
		.frontend_type = CSI_DSP_FE_TYPE_ISP,
		.task_id = -1,
		{
			.isp_param.id=0,
			.isp_param.hor=640,
			.isp_param.ver=480,
			.isp_param.data_fmt=CSI_DSP_IMG_FMT_RAW12_ALGIN,
			.isp_param.line_in_entry=640*2,
			.isp_param.line_stride=640*2,
			.isp_param.buffer_size=640*2*16*2,
			.isp_param.buffer_addr=0xb0000000,
		}
	};


	if(csi_dsp_task_config_frontend(vi_task,&config_params))
	{

		printf("isp config fail\n");
		return -1;
	}

	struct csi_dsp_task_be_para post_config =
	{
		.backend_type = CSI_DSP_BE_TYPE_POST_ISP,
		.task_id = -1,
		{
			.post_isp_param.id=0,
			.post_isp_param.hor=640,
			.post_isp_param.ver=480,
			.post_isp_param.data_fmt=CSI_DSP_IMG_FMT_RAW12_ALGIN,
			.post_isp_param.line_in_entry=640*2,
			.post_isp_param.line_stride=640*2,
			.post_isp_param.buffer_size=640*2*16*2,
			.post_isp_param.buffer_addr=0xb0000000,
		}
	};

	if(csi_dsp_task_config_backend(vi_task,&post_config))
	{

		printf("post-isp config fail\n");
		return -1;
	}

	struct csi_dsp_algo_config_par alog_config={
		.algo_id=0,
	};

	if(csi_dsp_task_config_algo(vi_task,&alog_config))
	{
		printf("algo kernel config fail\n");
		return -1;
	}

	if(csi_dsp_task_register_cb(vi_task,isp_algo_result_handler,NULL,32))
	{
		printf("algo kernel start fail\n");
		return -1;
	}

	if(csi_dsp_task_start(vi_task))
	{
		printf("task  start fail\n");
		return -1;
	}

	
	while(1)
	{
		;
	}

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