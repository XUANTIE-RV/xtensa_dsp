#ifndef DETECT_H
#define DETECT_H

#ifdef __cplusplus
extern "C" {
#endif
//#include "cnndecoder.h"
typedef struct BBoxOut
{
    int label;
    float score;
    float xmin;
    float ymin;
    float xmax;
    float ymax;
}BBoxOut;
#define PIX3218 0
#define PIX3030 1
#if PIX3218
#define  num_prior 1224
#elif PIX3030
#define  num_prior 1917
#endif
typedef struct BBox
{
    float xmin;
    float ymin;
    float xmax;
    float ymax;
}BBox;

typedef struct BBoxRect
{
    float xmin;
    float ymin;
    float xmax;
    float ymax;
    int label;
}BBoxRect;


int ssdforward(float *location,float * confidence,float * priorbox,BBox *bboxes,BBoxOut *out);
int readbintomem(float *dst,char *path);

#ifdef __cplusplus
}
#endif

#endif
