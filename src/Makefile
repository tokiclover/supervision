-include ../config.mak
CC      ?= cc

dist_SRCS := error.c helper.c
dist_HDRS := error.h helper.h

.SECONDEXPANSION:
checkpath_SRCS := checkpath.c error.c
sv-config_SRCS := $(dist_SRCS) sv-config.c sv-conf.c
sv-shutdown_SRCS  := sv-shutdown.c $(dist_SRCS)
sv-shutdown_LIBS   = $(LIBRT_LIBS)
supervision_SRCS := supervision.c error.c err_syslog.c
fstabinfo_SRCS := fstabinfo.c error.c
mountinfo_SRCS := mountinfo.c error.c sv-list.c
waitfile_SRCS  := error.c waitfile.c

sv-init_SRCS  := sv-init.c error.c
sv-run_SRCS   := sv-conf.c sv-deps.c sv-run.c sv-list.c sv-rc.c $(dist_SRCS)
sv-run_HDRS   := sv.h sv-conf.h sv-copyright.h
sv-run_CFLAGS  = $(PTHREAD_FLAGS)
sv-run_LDFLAGS = $(PTHREAD_FLAGS)

svp_SRCS       := svp.c error.c

HDRRS      = $(dist_HDRS) $(sv-run_HDRS)
.SECONDEXPANSION:
dist_BINS := \
	checkpath \
	fstabinfo \
	mountinfo \
	supervision \
	sv-config \
	sv-init \
	svd \
	svp \
	svr \
	sv-shutdown \
	waitfile

ifeq (SYSVINIT,yes)
dist_BINS  += initctl
initctl_SRCS := initctl.c $(dist_SRCS)
endif

all: $(dist_BINS) sv-run

clean:
	rm -f $(dist_BINS) $(dist_SRCS:%.c=%.o) $(sv-run_SRCS:%.c=%.o) $(dist_BINS:%.c=%.o%=%.o) \
		$(dist_BINS)

sv-run: $(sv-run_SRCS:%.c=%.o) $(dist_HDRS) $(sv-run_HDRS)
	$(CC) $(CFLAGS) $($@_CFLAGS) -o $@ $(sv-run_SRCS:%.c=%.o) $(LDFLAGS) $($@_LDFLAGS) $($@_LIBS)

supervision_HDRS := error.h sv-copyright.h
svp_HDRS       := config.h error.h sv-copyright.h

mountinfo_HDRS := sv-list.h
sv-config_HDRS := $(dist_HDRS) sv-conf.h
sv-init_HDRS   := config.h sv-copyright.h
sv-shutdown_HDRS := error.h sv-copyright.h

checkpath: $(ceckpath_SRCS:%.c=%.o) $(checkpath_HDRS)
sv-config: $(sv-config_SRCS:%.c=%.o) $(sv-config_HDRS)
sv-shutdown: $(sv-shutdown_SRCS:%.c=%.o) $(sv-shutdown_HDRS)
supervision: $(supervision_SRCS:%.c=%.o) $(supervision_HDRS)
fstabinfo: $(fstabinfo_SRCS:%.c=%.o) $(fstabinfo_HDRS)
mountinfo: $(mountinfo_SRCS:%.c=%.o) $(mountinfo_HDRS)
waitfile: $(waitfile_SRCS:%.c=%.o) $(waitfile_HDRS)
sv-init: $(sv-init_SRCS:%.c=%.o) $(sv-init_HDRS)
svp: $(svp_SRCS:%.c=%.o) $(svp_HDRS)
initctl: $(initctl_SRCS:%.c=%.o) $(initctl_HDRS)

$(dist_BINS): $@.o $($$@_SRCS:%.c=%.o) $($$@_HDRS)
	$(CC) $(CFLAGS) $($@_CFLAGS) -o $@ $($@_SRCS:%.c=%.o) $(LDFLAGS) $($@_LDFLAGS) $($@_LIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) $(@_CFLAGS) $(CPPFLAGS) $(@_CPPFLAGS) $< -c -o $@

