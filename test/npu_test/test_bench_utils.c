
#include "io.h"
#include "shm_common.h"
#include "test_bench_utils.h"

struct ShmSrc {
  int fd;
  int fd_mem;
  int src_idx;
  int sink_idx;
  uint8_t *virtual_address;
  uint32_t size;
  ShmBuffer *buffer;
};

const void* ShmPicSrcOpen(const char *name) {
  struct ShmSrc* inst = calloc(1, sizeof(struct ShmSrc));
  uint32_t err = 0;
  if (inst == NULL) return NULL;
  inst->src_idx = 0;
  inst->sink_idx = 0;
  do {
    inst->fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR,
                        S_IRUSR | S_IWUSR);
    if (inst->fd == -1) {
      printf("ERROR: shm_open failed,%d\n",inst->fd);
      err = NULL;
      break;
    }
    if (ftruncate(inst->fd, sizeof(ShmBuffer)) == -1) {
      printf("ERROR: failed to ftruncate shared memory\n");
      err = NULL;
      break;
    }
    inst->buffer = mmap(NULL, sizeof(ShmBuffer),
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED, inst->fd, 0);
    if (inst->buffer == MAP_FAILED) {
      printf("ERROR: failed to map shared memory\n");
      err = NULL;
      break;
    }
    inst->buffer->exit = 0;
    if (sem_init(&inst->buffer->sem_sink, 1, NUM_OF_BUFFERS) == -1) {
      printf("ERROR: failed to initialize semaphore sem_sink\n");
      err = NULL;
      break;
    }
    if (sem_init(&inst->buffer->sem_src, 1, 0) == -1) {
      printf("ERROR: failed to initialize semaphore sem_src\n");
      err = NULL;
      break;
    }
    inst->fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
    if (inst->fd_mem == -1) {
      printf("ERROR: failed to open: %s\n", "/dev/mem");
      err = NULL;
      break;
    }
  } while (0);
  
  if (err != 0) {
    free((void*)inst);
    inst = NULL;
  }
  return inst;
}

void ShmSrcGetPicture(const void* inst, uint8_t **pRGBBuffer) {
  struct ShmSrc* src = (struct ShmSrc*)inst;
  if (src != NULL) {
    PictureInfo *pic = NULL;
    uint32_t size_rgb = 0;
    printf("wait for sem\n");
    sem_wait(&src->buffer->sem_src);
    pic = &src->buffer->pics[src->sink_idx];
    //printf("DEBUG: get one frame: %d, %dx%d, phyaddr = 0x%08x\n", 
      //src->npu_idx, pic->pic_width, pic->pic_height, pic->bus_address_rgb);
    size_rgb = 3 * pic->pic_height * pic->pic_width;
    src->size = size_rgb;
    if (src->fd_mem != -1) {
      src->virtual_address = (uint8_t *) mmap(0, size_rgb, PROT_READ | PROT_WRITE,
              MAP_SHARED, src->fd_mem, pic->bus_address_rgb);
      if (src->virtual_address == MAP_FAILED) {
          printf("ERROR: Failed to mmap busAddress: 0x%08x\n", pic->bus_address_rgb);
      }
    }
    printf("DEBUG: get one frame: %d, %dx%d, phyaddr = 0x%08x, viraddr = 0x%08x\n", 
      src->sink_idx, pic->pic_width, pic->pic_height, pic->bus_address_rgb, src->virtual_address);
    *pRGBBuffer = src->virtual_address;
    // dump one picture
    while (0) {
      static FILE *fp = NULL;
      if (fp == NULL) {
        fp = fopen("npudump.rgb", "wb");        
      }
      if (fp != NULL) {
        fwrite(src->virtual_address, src->size, 1, fp);
      }
    }
  }
}

void ShmSrcReleasePicture(const void* inst) {
  struct ShmSrc* src = (struct ShmSrc*)inst;
  if (src != NULL) {
    if (src->virtual_address != MAP_FAILED) {
        munmap(src->virtual_address, src->size);
    }
    sem_post(&src->buffer->sem_sink);
    printf("DEBUG: release one frame: %d\n", src->sink_idx);
    src->sink_idx = (src->sink_idx + 1) % NUM_OF_BUFFERS;
  }
}

void ShmPicSrcClose(const char *name, const void* inst) {
  struct ShmSrc* src = (struct ShmSrc*)inst;
  if (src != NULL) {
    if (src->fd != -1) {
      src->buffer->exit = 1;
      sem_post(&src->buffer->sem_sink);
      printf("DEBUG: post exit signal\n");
      sem_wait(&src->buffer->sem_src);
      printf("DEBUG: ISP exit\n");
      shm_unlink(name);
      if (src->buffer != MAP_FAILED) {
        munmap(src->buffer, sizeof(ShmBuffer));
      }
    }
    free((void*)inst);
  }
}

