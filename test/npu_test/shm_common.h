#ifndef __SHM_COMMON_H__
#define __SHM_COMMON_H__
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_OF_BUFFERS 6

typedef struct PictureInfo {
  unsigned int bus_address_rgb;
  unsigned int pic_width;
  unsigned int pic_height;
} PictureInfo;

typedef struct Box {
  float x1;
  float y1;
  float x2;
  float y2;
} Box;

typedef struct Landmark {
  float x[5];
  float y[5];
} Landmark;

typedef struct FaceDetect {
  float score;
  Box box;
  Landmark landmark;
} FaceDetect;

typedef struct FaceInfo {
  unsigned int bus_address_feature;
  unsigned int face_cnt;
} FaceInfo;

typedef struct ShmBuffer {
  sem_t  sem_src;           /* ISP posts to inform NPU that a frame buffer is ready for inference */
  sem_t  sem_sink;           /* NPU posts to inform ISP that a frame buffer is released */
  PictureInfo pics[NUM_OF_BUFFERS];
  int exit;
} ShmBuffer;

typedef struct ShmFeatureBuffer {
  sem_t  sem_src;           /* NPU posts to inform G2D that a frame buffer is ready for drawing */
  sem_t  sem_sink;           /* ISP posts to inform NPU that a frame buffer is released */
  FaceInfo feature[NUM_OF_BUFFERS];
  int exit;
} ShmFeatureBuffer;

#endif /* __SHM_COMMON_H__ */
