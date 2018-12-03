#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include "v4l2core.h"
#include "v4l2xu.h"

v4l2_dev_t* vd = NULL;
static char* deviceName = "/dev/video0";
static unsigned int bSaveVideo = 0;

void* thread_task_capture(void)
{
    if(v4l2core_capture_init(vd)<0)
    {
        puts("Capture init error.");
        pthread_exit(NULL);
    }
    if(v4l2core_capture_start(vd)<0)
    {
        puts("Can not start capture");
        pthread_exit(NULL);
    }
    v4l2core_capture_loop(vd);
    v4l2core_capture_stop(vd);
    v4l2core_capture_uninit(vd);    
    pthread_exit(NULL);
}

/**
SIGINT interput handler
*/
void StopContCapture(int sig_id) {
	printf("stoping continuous capture\n");
	if(vd)
	{
        vd->bcapture = 0;
	}
}

void InstallSIGINTHandler() {
	struct sigaction sa;
	memset(&sa,0,sizeof(sa));

	sa.sa_handler = StopContCapture;
	if(sigaction(SIGINT, &sa, 0) != 0)
	{
		fprintf(stderr,"could not install SIGINT handler, continuous capture disabled");
			if(vd)
        vd->bcapture = 0;
	}
}

/**
	print usage information
*/
static void usage(FILE* fp, int argc, char** argv)
{
	fprintf(fp,
		"Usage: %s [options]\n\n"
		"Options:\n"
		"\t-d | --device name       Video device name [default:/dev/video0]\n"
		"\t-v | --save video file   Save video data to file,file name:out.avi\n"
        "\t-h | --help              Print this message\n"
		"",
		argv[0]);
}

static const char short_options [] = "d:vh";

static const struct option
long_options [] = {
	{ "device",             required_argument,      NULL,           'd' },
	{ "save video",         no_argument,            NULL,           'v' },
    { "help",               no_argument,            NULL,           'h' },
	{ 0, 0, 0, 0 }
};

int main(int argc,char **argv)
{
    while(1){
        int index,c = 0;
        c = getopt_long(argc, argv, short_options, long_options, &index);

		if (-1 == c)
		{
			break;
        }
        switch (c) {
			case 0: /* getopt_long() flag */
                exit(EXIT_SUCCESS);
                break;
            case 'd':
				deviceName = optarg;
				break;
            case 'v':
                bSaveVideo = 1;
                break;
            case 'h':
            default:
				usage(stderr, argc, argv);
				exit(EXIT_FAILURE);
		}
    }


    //open device
    vd = v4l2core_dev_open(deviceName);
    if(vd == NULL)
    {
        puts("Can't open video device.");
        return -1;
    }
    vd->bcapture = 1;
    //init device
    v4l2core_dev_init(vd);

    InstallSIGINTHandler();

    int ret = 0;
    pthread_t tid_capture;
    ret = pthread_create(&tid_capture,NULL,(void*)thread_task_capture(),NULL);
    if(ret)
    {
        puts("create capture thread error!");
        exit(EXIT_FAILURE);
    }

    pthread_join(tid_capture,NULL);
    v4l2core_dev_close(vd);

    return 0;
}