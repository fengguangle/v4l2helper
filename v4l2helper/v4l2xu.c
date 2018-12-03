#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <assert.h>
#include "v4l2xu.h"

#define DELAYQUERY      1            //is delay
#define DELAYQUERY_IV   10000        //delay time us

uint8_t u8unit=5;
uint8_t u8ctrl=4;

static void errno_show(const char* s)
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
}

/*static void   Delay(int   time)//time*1000为秒数
{
    clock_t   now   =   clock();
    while(clock() - now <  time);
}*/

uint16_t get_length_xu_control(v4l2_dev_t *vd, uint8_t unit, uint8_t selector)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->fd > 0);

	uint16_t length = 0;

	struct uvc_xu_control_query xu_ctrl_query =
	{
		.unit     = unit,
		.selector = selector,
		.query    = UVC_GET_LEN,
		.size     = sizeof(length),
		.data     = (uint8_t *) &length
	};

	if (xioctl(vd->fd, UVCIOC_CTRL_QUERY, &xu_ctrl_query) < 0)
	{
		fprintf(stderr, "V4L2_CORE: UVCIOC_CTRL_QUERY (GET_LEN) - Error: %s\n", strerror(errno));
		return 0;
	}

	return length;
}

int query_xu_control(v4l2_dev_t *vd, uint8_t unit, uint8_t selector, uint8_t query, void *data)
{
	int err = 0;
	uint16_t len = get_length_xu_control(vd, unit, selector);

	struct uvc_xu_control_query xu_ctrl_query =
	{
		.unit     = unit,
		.selector = selector,
		.query    = query,
		.size     = len,
		.data     = (uint8_t *) data
	};

	/*get query data*/
	if ((err=xioctl(vd->fd, UVCIOC_CTRL_QUERY, &xu_ctrl_query)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: UVCIOC_CTRL_QUERY (%i) - Error: %s\n", query, strerror(errno));
	}

	return err;
}
