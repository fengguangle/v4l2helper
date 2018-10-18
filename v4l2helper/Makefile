all:v4l2core.o v4l2xu.o

v4l2core.o: v4l2core.c v4l2core.h
	cc -c v4l2core.c

v4l2xu.o: v4l2xu.c v4l2core.h v4l2xu.h
	cc -c v4l2xu.c

clean:
	-rm *.o

