#ifndef TEST_BENCH_UTILS_H
#define TEST_BENCH_UTILS_H

//#include "base_type.h"

const void* ShmPicSrcOpen(const char *name);
void ShmSrcGetPicture(const void* inst, uint8_t **pRGBBuffer);
void ShmSrcReleasePicture(const void* inst);
void ShmPicSrcClose(const char *name, const void* inst);

#endif // TEST_BENCH_UTILS_H
