#ifndef V4L2CORE_H_INCLUDED
#define V4L2CORE_H_INCLUDED

#include <linux/videodev2.h>
#include "utlist.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLEAR(x) memset (&(x), 0, sizeof (x))
#define VIDIOC_REQBUFS_COUNT 2

typedef void (*ProcessVBuff)(char* buff,int size);

typedef enum {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
} io_method;

typedef enum {
        FMT_H264,
        FMT_YUV,
        FMT_MJPEG,
} fmt_type;

typedef struct buffer {
        void *                  start;
        unsigned int            length;
} buffer;

typedef struct DeviceCap{
    uint8_t isSupportCapture;
    uint8_t isSupportStreaming;
    uint8_t isSupportRead;
}DeviceCap;

typedef struct FrameSize{
    struct FrameSize *prev, *next;
    struct v4l2_frmsizeenum frmsizeenum;
}FrameSize;

typedef struct FrameDesc{
    struct FrameDesc *prev, *next;
    uint8_t         index;
    char            description[32];
    unsigned int    pixformat;
    FrameSize*      pframeSize;
}FrameDesc;

typedef struct v4l2_dev_t{
    //device
    int fd;
    char *deviceName;

    //UVC
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    struct v4l2_format fmtack;
    struct v4l2_streamparm frameint;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum frmsozeenum;

    //video
    unsigned int width;
    unsigned int height;
    unsigned int fps;

    //capture
    io_method    io;
    buffer*      buffers;
    unsigned int n_buffers;
    unsigned int buffer_size;

    ProcessVBuff VBuffCallback;
    unsigned int bcapture;

    FrameDesc*  p_frameDesc;
    DeviceCap   deviceCap;

} v4l2_dev_t;

int xioctl(int fd, int request, void* argp);

v4l2_dev_t* v4l2core_dev_open(const char* name);

void v4l2core_dev_close(v4l2_dev_t *vd);

int v4l2core_dev_init(v4l2_dev_t* vd);

int v4l2core_dev_set_fmt(v4l2_dev_t* vd,uint32_t pixfmt,uint32_t width,uint32_t height);

int v4l2core_dev_set_fps(v4l2_dev_t* vd,uint32_t numerator,uint32_t denominator);

int v4l2core_capture_init(v4l2_dev_t *vd);

void v4l2core_capture_uninit(v4l2_dev_t *vd);

int v4l2core_capture_start(v4l2_dev_t* vd);

void v4l2core_capture_loop(v4l2_dev_t* vd);

void v4l2core_capture_stop(v4l2_dev_t* vd);

#ifdef __cplusplus
}
#endif

#endif // V4L2CORE_H_INCLUDED
