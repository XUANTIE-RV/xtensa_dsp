
#include <string.h>
#include <stdio.h>>
#include <stdlib.h>
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

int
compareBuffers(int expected, char* src, char* dst, int size)
{
  int i;
  for(i = 0 ; i < size; i++) {
    if (dst[i] != src[i]) {
      if (expected) {
        printf("MISMATCH EXPECTED @ byte %d (dst:%x, src:%x)!!\n", i, dst[i], src[i]);
      }
      else {
        printf("COPY FAILED @ byte %d (dst:%x, src:%x)!!\n", i, dst[i], src[i]);
	return  -1;
      }
    }
  }
  printf("Compare OK (src:%p, dst:%p, len:%d)\n", src, dst, size);
  return 0;
}
int load_data_from_file(char *fileInName,void* buf,size_t data_size)
{
    // char *fileInName = NULL;
    // char *fileReferenceName = NULL;
    char fileOutName[512];
    int rt;
    FILE *file;


    // fileInName = "./test/obj_bits_1280.dat";
    // //fileOutName = argv[2];
    // fileReferenceName = "./test/ref_bits_1280.dat";

    // file = fopen(fileReferenceName, "rb");
    // if (file != NULL) {
    //     fread(ref_binary, sizeof(char), 1280*1087/8, file);
    //     fclose(file);
    // } else {
    //     printf("open ref file fail: %s", fileReferenceName);
    //     return -1;
    // }
    file = fopen(fileInName, "rb");
    if(file != NULL){
        fread(buf, sizeof(char), data_size, file);
        fclose(file);
    } else
    {
        printf("open obj file fail: %s", fileInName);
        return -2;
    }
    return 0;


}


int main(int argc, char *argv[])
{
    printf("********************************\n");
    printf("[dsp max power]  test\n");
    printf("********************************\n");
    

    if (argc < 2) {
        printf("[dsp test] please provide parameter in following format:\n");
        printf("  ./dsp_max_power case_id, repeat.");
        exit(-1);
    }

    int case_id = atoi(argv[1]);
    int repeat   = atoi(argv[2]);

    csi_dsp_algo_load_req_t alog_config;
    switch(case_id)
    {
        case 0: 
            alog_config.algo_id = 1;
            printf("Select Ant Algo !\n");
            break;
        case 1:
            alog_config.algo_id = 2;
            printf("Select Vendor Max Power Algo !\n");
            break;
        default:
            printf("unsupport case id:%d !\n",case_id);
            return -1;
    }
    printf("Case repeat:%d !\n",repeat);
	void *instance = csi_dsp_create_instance(1);
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

    if(case_id == 0)
    {
        if(csi_dsp_task_acquire_algo(task,"x_test_flo"))
        {
            printf("algo kernel load fail\n");
            return -1;

        }
    }
    else if(case_id == 1)
    { 
        if(csi_dsp_task_acquire_algo(task,"max_power_diags_flo"))
        {
            printf("algo kernel load fail\n");
            return -1;

        }
    }
    else
    {
        printf("algo kernel no invalid for case :%d\n",case_id);
        return -1;
    }

    struct csi_sw_task_req* req=NULL;
    req =csi_dsp_task_create_request(task);
    if(req==NULL)
    {
        printf("req create fail\n");
		return -1;
    }
    struct csi_dsp_buffer buf2;
    if(case_id == 0)
    {
        
  
  /*****************************input buffer****************************************/
            struct csi_dsp_buffer buf1 = 
            {
                .buf_id = 0,
                .dir = CSI_DSP_BUFFER_IN,
                .type = CSI_DSP_BUF_ALLOC_DRV,
                .plane_count = 1,
                .width =1280,
                .height =960,
                .planes[0].stride= 1280/8,
                .planes[0].size=1280*1180/8,
            };

            if(csi_dsp_request_add_buffer(req,&buf1))
            {
                printf("%s,add buffer:%d\n",__FUNCTION__,buf1.buf_id);
                csi_dsp_task_release_request(req);
                return -1;
            }
            if(load_data_from_file("obj_bits_1280.dat",buf1.planes[0].buf_vir,1280*960/8))
            {
                return -1;
            }

  /*****************************output buffer****************************************/

            buf2.buf_id = 1;
            buf2.dir = CSI_DSP_BUFFER_OUT;
            buf2.type = CSI_DSP_BUF_ALLOC_DRV;
            buf2.plane_count = 1;
            buf2.width =320;
            buf2.height =240;
            buf2.planes[0].stride= 320*2;
            buf2.planes[0].size=640*480*2;


            if(csi_dsp_request_add_buffer(req,&buf2))
            {
                printf("%s,add buffer:%d\n",__FUNCTION__,buf2.buf_id);
                csi_dsp_task_release_request(req);
                return -1;
            }
            memset(buf2.planes[0].buf_vir,0x00,buf2.planes[0].size);

  /*****************************user defineed buffer****************************************/
          struct csi_dsp_buffer buf3 = 
            {
                .buf_id = 2,
                .dir = CSI_DSP_BUFFER_IN_OUT,
                .type = CSI_DSP_BUF_ALLOC_DRV,
                .plane_count = 1,
                .width =1280,
                .height =1080,
                .planes[0].stride= 1280/8,
                .planes[0].size= 1280*1180/8,
            };

            if(csi_dsp_request_add_buffer(req,&buf3))
            {
                printf("%s,add buffer:%d\n",__FUNCTION__,buf3.buf_id);
                csi_dsp_task_release_request(req);
                return -1;
            }

            if(load_data_from_file("ref_bits_1280.dat",buf3.planes[0].buf_vir,1280*1087/8))
            {
                return -1;
            }



    }

    if(csi_dsp_request_set_property(req,&repeat,sizeof(repeat)))
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
    if(case_id == 0)
    {
        short result[320*240];
        load_data_from_file("result.raw",result,320*240*2);
        if(compareBuffers(1,(void*)buf2.planes[0].buf_vir,result,320*240*2))
        {
            printf("%s,ERR cmp fail\n",__FUNCTION__);
            csi_dsp_task_release_request(req);
            csi_dsp_destroy_task(task);
            csi_dsp_delete_instance(instance);
            printf("%s,Test FAIL!\n",__FUNCTION__);
            return -1;
        }
    }


    csi_dsp_task_release_request(req);
    csi_dsp_destroy_task(task);
    csi_dsp_delete_instance(instance);
    printf("%s,Test Pass!\n",__FUNCTION__);
	return 0;
}
