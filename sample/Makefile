V4L2PATH =  ../v4l2helper/

all:sample1

sample1.o: sample1.c
	cc -I../v4l2helper -c sample1.c

sample1: sample1.o
	cc -o sample1 sample1.o $(V4L2PATH)v4l2core.o $(V4L2PATH)v4l2xu.o -lpthread

clean:
	-rm *.o sample1
