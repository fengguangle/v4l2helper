TOPTARGETS := all v4l2helper sample clean

all:v4l2helper sample

v4l2helper:
	$(MAKE) -C v4l2helper/.

sample:v4l2helper
	$(MAKE) -C sample/.

v4l2helper-clean:
	$(MAKE) -C v4l2helper/. clean

sample-clean:
	$(MAKE) -C sample/. clean

clean:v4l2helper-clean sample-clean

.PHONY: $(TOPTARGETS)
