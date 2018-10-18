#ifndef V4L2XU_H_INCLUDED
#define V4L2XU_H_INCLUDED
#include "v4l2core.h"
#include <inttypes.h>
#include <linux/uvcvideo.h>
#include <linux/usb/video.h>
//#include <ctime.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t get_length_xu_control(v4l2_dev_t *vd, uint8_t unit, uint8_t selector);

int query_xu_control(v4l2_dev_t *vd, uint8_t unit, uint8_t selector, uint8_t query, void *data);

#ifdef __cplusplus
}
#endif

#endif // V4L2XU_H_INCLUDED
