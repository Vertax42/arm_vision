
.PHONY:: all 
MAKE := make

SUBDIRS = HawkColor HawkDepth HawkLightIr HawkMixColorDepth HawkMixHDColorDepth HawkMixColorDepthMatting HawkNetDeviceInfo HawkAutoSetNetInfo

all::$(SUBDIRS)
	@for f in $^; do (cd $$f&&$(MAKE) --file=Makefile&&cd -)||exit; done

clean::$(SUBDIRS)
	@for f in $^; do (cd $$f&&$(MAKE) --file=Makefile clean&&cd -)||exit; done
