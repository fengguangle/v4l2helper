#include "v4l2core.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>

int xioctl(int fd, int request, void* argp)
{
	int r;

	do r = ioctl(fd, request, argp);
	while (-1 == r && EINTR == errno);

	return r;
}

static void errno_show(const char* s)
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
}

static int readInit(v4l2_dev_t *vd)
{
        vd->buffers = calloc(1, sizeof(*vd->buffers));

        if (!vd->buffers) {
                fprintf(stderr, "Out of memory\\n");
                return -1;
        }

        vd->buffers[0].length = vd->buffer_size;
        vd->buffers[0].start = malloc(vd->buffer_size);

        if (!vd->buffers[0].start) {
                fprintf(stderr, "Out of memory\\n");
                return -1;
        }
}

static int mmapInit(v4l2_dev_t *vd)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = VIDIOC_REQBUFS_COUNT;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(vd->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support memory mapping\n", vd->deviceName);
			return -1;
		} else {
			errno_show("VIDIOC_REQBUFS");
			return -1;
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n", vd->deviceName);
		return -1;
	}

	vd->buffers = calloc(req.count, sizeof(*vd->buffers));

	if (!vd->buffers) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}

	for (vd->n_buffers = 0; vd->n_buffers < req.count; ++vd->n_buffers) {
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = vd->n_buffers;

		if (-1 == xioctl(vd->fd, VIDIOC_QUERYBUF, &buf)){
			errno_show("VIDIOC_QUERYBUF");
			return -1;
        }

		vd->buffers[vd->n_buffers].length = buf.length;
		vd->buffers[vd->n_buffers].start = mmap(NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */, MAP_SHARED /* recommended */, vd->fd, buf.m.offset);

		if (MAP_FAILED == vd->buffers[vd->n_buffers].start){
			errno_show("mmap");
			return -1;
        }
	}
	return 0;
}

static int userptrInit(v4l2_dev_t *vd)
{
	struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count  = VIDIOC_REQBUFS_COUNT;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

        if (-1 == xioctl(vd->fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "user pointer i/on", vd->deviceName);
                        return -1;
                } else {
                        errno_show("VIDIOC_REQBUFS");
						return -1;
                }
        }

        vd->buffers = calloc(req.count, sizeof(*vd->buffers));

        if (!vd->buffers) {
                fprintf(stderr, "Out of memory\\n");
                return -1;
        }

        for (vd->n_buffers = 0; vd->n_buffers < req.count; ++vd->n_buffers) {
                vd->buffers[vd->n_buffers].length = vd->buffer_size;
                vd->buffers[vd->n_buffers].start = malloc(vd->buffer_size);

                if (!vd->buffers[vd->n_buffers].start) {
                        fprintf(stderr, "Out of memory\\n");
                        return -1;
                }
        }
}

v4l2_dev_t* v4l2core_dev_open(const char* deviceName)
{
    v4l2_dev_t* vd = calloc(1,sizeof(v4l2_dev_t));
    assert(vd != NULL);
    memset(vd,0,sizeof(v4l2_dev_t));
    vd->io = IO_METHOD_MMAP;
    vd->fps = 15;
    vd->deviceName = strdup(deviceName);
    vd->width = 0;
    vd->height = 0;
	vd->p_frameDesc = NULL;

    struct stat st;
	// stat file
	if (-1 == stat(deviceName, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n", vd->deviceName, errno, strerror(errno));
		return NULL;
	}

	// check if its device
	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", vd->deviceName);
		return NULL;
	}

	// open device
	vd->fd = open(vd->deviceName, O_RDWR | O_NONBLOCK,0);

	// check if opening was successfull
	if (-1 == vd->fd) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n", vd->deviceName, errno, strerror(errno));
		return NULL;
	}

	return vd;
}

void v4l2core_dev_close(v4l2_dev_t *vd)
{
    assert(vd!=NULL);
    if(vd->deviceName)
        free(vd->deviceName);
    vd->deviceName = NULL;

    if(vd->fd>0)
    {
        close(vd->fd);
		vd = NULL;
    }
}

void enum_frame_sizes(v4l2_dev_t *vd,uint32_t pixfmt,FrameDesc* pframeDesc)
{
    vd->frmsozeenum.index=0;
    vd->frmsozeenum.pixel_format = pixfmt;
    while(ioctl(vd->fd,VIDIOC_ENUM_FRAMESIZES,&vd->frmsozeenum)!=-1)
    {
		FrameSize* frm = (FrameSize*)malloc(sizeof(FrameSize));
		memset(frm,0,sizeof(FrameSize));
		if(frm)
		{
			memcpy(&frm->frmsizeenum,&vd->frmsozeenum,sizeof(vd->frmsozeenum));
			DL_APPEND(pframeDesc->pframeSize,frm);
		}
        vd->frmsozeenum.index++;
    }
}

int v4l2core_dev_init(v4l2_dev_t *vd)
{
    puts("************UVC Device Information************");
    if (-1 == xioctl(vd->fd, VIDIOC_QUERYCAP, &vd->cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n",vd->deviceName);
		} else {
			errno_show("query capabilities");
		}
		return 0;
	}
	else
	{
        printf("driver:\t\t%s\n",vd->cap.driver);
        printf("card:\t\t%s\n",vd->cap.card);
        printf("bus_info:\t%s\n",vd->cap.bus_info);
        printf("version:\t%d\n",vd->cap.version);
        printf("capabilities:\t%x\n",vd->cap.capabilities);

        if ((vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE)
        {
            printf("Device %s: supports capture.\n",vd->deviceName);
        }

        if ((vd->cap.capabilities & V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING)
        {
            printf("Device %s: supports streaming.\n",vd->deviceName);
        }

        switch (vd->io) {
		case IO_METHOD_READ:
			if (!(vd->cap.capabilities & V4L2_CAP_READWRITE)) {
				fprintf(stderr, "%s does not support read i/o\n",vd->deviceName);
			}
			break;
		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
      			if (!(vd->cap.capabilities & V4L2_CAP_STREAMING)) {
				fprintf(stderr, "%s does not support streaming i/o\n",vd->deviceName);
			}
			break;
        }
	}

	/* Select video input, video standard and tune here. */
	CLEAR(vd->cropcap);

	vd->cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(vd->fd, VIDIOC_CROPCAP, &vd->cropcap)) {
		vd->crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vd->crop.c = vd->cropcap.defrect; /* reset to default */

		if (-1 == xioctl(vd->fd, VIDIOC_S_CROP, &vd->crop)) {
			switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	} else {
		/* Errors ignored. */
	}

    puts("************Support Image Formats Information************");
    //emu all support fmt
    vd->fmtdesc.index=0;
    vd->fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    

    while(ioctl(vd->fd,VIDIOC_ENUM_FMT,&vd->fmtdesc)!=-1)
    {
		FrameDesc* fdesc = (FrameDesc*)malloc(sizeof(FrameDesc));
		if(!fdesc)
			continue;		
		fdesc->index = vd->fmtdesc.index;
		memcpy(fdesc->description,&vd->fmtdesc.description,sizeof(fdesc->description));
		fdesc->index = vd->fmtdesc.index;
		fdesc->pixformat = vd->fmtdesc.pixelformat;
		fdesc->pframeSize = NULL;
        enum_frame_sizes(vd,vd->fmtdesc.pixelformat,fdesc);
		DL_APPEND(vd->p_frameDesc,fdesc);
        vd->fmtdesc.index++;
    }

	FrameDesc* elt;
	FrameSize* elt2;
	int count;
	DL_COUNT(vd->p_frameDesc,elt,count);
	printf("Support format (%d):\n",count);
	DL_FOREACH(vd->p_frameDesc,elt) {
		printf("\t%d.%s\n",elt->index, elt->description);
		DL_COUNT(elt->pframeSize,elt2,count);
		printf("\t\tSupport frame size (%d):\n",count);
		DL_FOREACH(elt->pframeSize,elt2)
		{
			if (elt2->frmsizeenum.type == V4L2_FRMSIZE_TYPE_DISCRETE)
			{
				printf("\t\t{ discrete: width = %u, height = %u }\n",
					   elt2->frmsizeenum.discrete.width, elt2->frmsizeenum.discrete.height);
			}
			else if (elt2->frmsizeenum.type == V4L2_FRMSIZE_TYPE_CONTINUOUS || elt2->frmsizeenum.type == V4L2_FRMSIZE_TYPE_STEPWISE)
			{
				if (elt2->frmsizeenum.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
					printf("\t\t{ continuous: min { width = %u, height = %u } .. "
						   "max { width = %u, height = %u } }\n",
						   elt2->frmsizeenum.stepwise.min_width, elt2->frmsizeenum.stepwise.min_height,
						   elt2->frmsizeenum.stepwise.max_width, elt2->frmsizeenum.stepwise.max_height);
				else
					printf("\t\t{ stepwise: min { width = %u, height = %u } .. "
						   "max { width = %u, height = %u } / "
						   "stepsize { width = %u, height = %u } }\n",
						   elt2->frmsizeenum.stepwise.min_width, elt2->frmsizeenum.stepwise.min_height,
						   elt2->frmsizeenum.stepwise.max_width, elt2->frmsizeenum.stepwise.max_height,
						   elt2->frmsizeenum.stepwise.step_width, elt2->frmsizeenum.stepwise.step_height);
			}
			else
			{
				fprintf(stderr, "V4L2_CORE: fsize.type not supported: %d\n", elt2->frmsizeenum.type);
				fprintf(stderr, "    (Discrete: %d   Continuous: %d  Stepwise: %d)\n",
						V4L2_FRMSIZE_TYPE_DISCRETE,
						V4L2_FRMSIZE_TYPE_CONTINUOUS,
						V4L2_FRMSIZE_TYPE_STEPWISE);
			}
		}
	}
	

    puts("************Current Image Formats Information************");
    //get fmt
    vd->fmtack.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(vd->fd, VIDIOC_G_FMT, &vd->fmtack) == -1)
    {
        errno_show("VIDIOC_G_FMT");
        printf("Unable to get format\n");
        return;
    }
    else
    {
         printf("fmt.type:\t\t%d\n",vd->fmtack.type);
         printf("pix.pixelformat:\t%c%c%c%c\n",vd->fmtack.fmt.pix.pixelformat & 0xFF, (vd->fmtack.fmt.pix.pixelformat >> 8) & 0xFF,(vd->fmtack.fmt.pix.pixelformat >> 16) & 0xFF, (vd->fmtack.fmt.pix.pixelformat >> 24) & 0xFF);
         printf("pix.width:\t\t%d\n",vd->fmtack.fmt.pix.width);
         printf("pix.height:\t\t%d\n",vd->fmtack.fmt.pix.height);
         printf("pix.field:\t\t%d\n",vd->fmtack.fmt.pix.field);

         vd->width = vd->fmtack.fmt.pix.width;
         vd->height = vd->fmtack.fmt.pix.height;

    }

    //get fps
    CLEAR(vd->frameint);
    vd->frameint.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(vd->fd, VIDIOC_G_PARM, &vd->frameint)){
        fprintf(stderr,"Unable to get frame interval.\n");
    }
    else
    {
        printf("capture fps:\t\t%d/%d\n",vd->frameint.parm.capture.timeperframe.numerator,
               vd->frameint.parm.capture.timeperframe.denominator);
        vd->fps = vd->frameint.parm.capture.timeperframe.denominator/vd->frameint.parm.capture.timeperframe.numerator;
    }

	return 1;
}

int v4l2core_dev_set_fmt(v4l2_dev_t* vd,uint32_t pixfmt,uint32_t width,uint32_t height)
{
	CLEAR(vd->fmt);
	vd->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->fmt.fmt.pix.pixelformat = pixfmt;
	vd->fmt.fmt.pix.width = width;
    vd->fmt.fmt.pix.height = height;    
    if(xioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt) == -1)
    {
        printf("Unable to set format\n");
        return -1;
    }
    printf("Set format success.\n");
	return 0;
}

int v4l2core_dev_set_fps(v4l2_dev_t* vd,uint32_t numerator,uint32_t denominator)
{
	CLEAR(vd->frameint);
	vd->frameint.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->frameint.parm.capture.timeperframe.numerator = 1;
	vd->frameint.parm.capture.timeperframe.denominator = vd->fps;
	if (-1 == xioctl(vd->fd, VIDIOC_S_PARM, &vd->frameint))
	{
		fprintf(stderr, "Unable to set frame interval.\n");
		return -1;
	}
	return 0;
}

int v4l2core_capture_init(v4l2_dev_t *vd)
{
	switch (vd->io)
	{
	case IO_METHOD_READ:
		vd->buffer_size = vd->fmt.fmt.pix.sizeimage;
		return readInit(vd);
		break;
	case IO_METHOD_MMAP:
		return mmapInit(vd);
		break;
	case IO_METHOD_USERPTR:
		vd->buffer_size = vd->fmt.fmt.pix.sizeimage;
		return userptrInit(vd);
		break;
	default:
		break;
	}
	return -1;
}

void v4l2core_capture_uninit(v4l2_dev_t *vd)
{
	unsigned int i;
	switch (vd->io) {
		case IO_METHOD_READ:
			free(vd->buffers[0].start);
			break;
		case IO_METHOD_MMAP:
			for (i = 0; i < vd->n_buffers; ++i)
				if (-1 == munmap(vd->buffers[i].start, vd->buffers[i].length))
					errno_show("munmap");
			break;
		case IO_METHOD_USERPTR:
			for (i = 0; i < vd->n_buffers; ++i)
				free(vd->buffers[i].start);
			break;
	}
	free(vd->buffers);
}

int v4l2core_capture_start(v4l2_dev_t* vd)
{
    printf("v4l2core_capture_start:\tn_buffers:%d\n",vd->n_buffers);
	unsigned int i;
	enum v4l2_buf_type type;

	switch (vd->io) {
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;
		case IO_METHOD_MMAP:
			for (i = 0; i < vd->n_buffers; ++i) {
                printf("v4l2core_capture_start:\tn_buffers:%d\n",i);
				struct v4l2_buffer buf;

				CLEAR(buf);

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = i;

				if (-1 == xioctl(vd->fd, VIDIOC_QBUF, &buf)){
                    errno_show("VIDIOC_QBUF");
					return -1;
                }
            }

			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			if (-1 == xioctl(vd->fd, VIDIOC_STREAMON, &type)){
                errno_show("VIDIOC_STREAMON");
				return -1;
            }

			break;
		case IO_METHOD_USERPTR:
			for (i = 0; i < vd->n_buffers; ++i) {
				struct v4l2_buffer buf;

			CLEAR (buf);

			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;
			buf.index = i;
			buf.m.userptr = (unsigned long) vd->buffers[i].start;
			buf.length = vd->buffers[i].length;

			if (-1 == xioctl(vd->fd, VIDIOC_QBUF, &buf))
				return -1;
			}

			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			if (-1 == ioctl(vd->fd, VIDIOC_STREAMON, &type))
				return -1;
			break;
	}
	return 0;
}

/**
	process orignal raw data
*/
static void dataProcess(v4l2_dev_t* vd,const void* p,int len ,struct timeval timestamp)
{
    printf("video recive:\t%d\n",len);
}

/**
	read single frame
*/
static int frameRead(v4l2_dev_t* vd)
{
	struct v4l2_buffer buf;
	unsigned int i;

	switch (vd->io) {
		case IO_METHOD_READ:
			if (-1 == read(vd->fd, vd->buffers[0].start, vd->buffers[0].length)) {
				switch (errno) {
					case EAGAIN:
						return 0;

					case EIO:
						// Could ignore EIO, see spec.
						// fall through

					default:
						return -1;
				}
			}

			struct timespec ts;
			struct timeval timestamp;
			clock_gettime(CLOCK_MONOTONIC,&ts);
			timestamp.tv_sec = ts.tv_sec;
			timestamp.tv_usec = ts.tv_nsec/1000;

			dataProcess(vd,vd->buffers[0].start,vd->buffers[0].length,timestamp);
			if(vd->VBuffCallback){
                vd->VBuffCallback((char*)vd->buffers[0].start,vd->buffers[0].length);
            }
			break;
		case IO_METHOD_MMAP:
			CLEAR(buf);

			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
            //puts("VIDIOC_DQBUF");
			if (-1 == xioctl(vd->fd, VIDIOC_DQBUF, &buf)) {
				switch (errno) {
					case EAGAIN:
						return 0;

					case EIO:
						// Could ignore EIO, see spec
						// fall through

					default:
                        errno_show("VIDIOC_DQBUF");
						return -1;
				}
			}

			assert(buf.index < vd->n_buffers);
			dataProcess(vd,vd->buffers[buf.index].start,buf.bytesused,buf.timestamp);
			if(vd->VBuffCallback){
                vd->VBuffCallback((char*)vd->buffers[buf.index].start,buf.bytesused);
            }
			if (-1 == xioctl(vd->fd, VIDIOC_QBUF, &buf))
                errno_show("VIDIOC_QBUF");

			break;
        case IO_METHOD_USERPTR:
				CLEAR (buf);

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_USERPTR;

				if (-1 == ioctl(vd->fd, VIDIOC_DQBUF, &buf)) {
					switch (errno) {
						case EAGAIN:
							return 0;

						case EIO:
							// Could ignore EIO, see spec.
							// fall through

						default:
							errno_show("VIDIOC_DQBUF");
							return -1;
					}
				}

				for (i = 0; i < vd->n_buffers; ++i)
					if (buf.m.userptr == (unsigned long)vd->buffers[i].start 
					&& buf.length == vd->buffers[i].length)
						break;

				assert (i < vd->n_buffers);

				//imageProcess((void *)buf.m.userptr,buf.timestamp);
				dataProcess(vd,vd->buffers[buf.index].start,buf.bytesused,buf.timestamp);

				if (-1 == ioctl(vd->fd, VIDIOC_QBUF, &buf))
				{
					errno_show("VIDIOC_QBUF");
					return -1;
				}
				break;
	}
	return 1;
}

void v4l2core_capture_loop(v4l2_dev_t* vd)
{
    fd_set fds;
    struct timeval tv;
    int r;

    while (vd->bcapture) {

        FD_ZERO(&fds);
        FD_SET(vd->fd, &fds);

        /* Timeout. */
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        r = select(vd->fd + 1, &fds, NULL, NULL, &tv);
        if(r<0)
        {
            fprintf(stderr, "Could not grab image (select error): %s\n", strerror(errno));
        }
        else if(r==0)
        {
            fprintf(stderr, "Could not grab image (select timeout): %s\n", strerror(errno));
        }
        else if(r>0)
        {
            frameRead(vd);
        }
	}
}

void v4l2core_capture_stop(v4l2_dev_t* vd)
{
    enum v4l2_buf_type type;

	switch (vd->io) {
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;
		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			if (-1 == ioctl(vd->fd, VIDIOC_STREAMOFF, &type))
			puts("VIDIOC_STREAMOFF error.");
			break;
	}
}
