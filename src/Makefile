-include ../config.mak
CC      ?= cc

dist_SRCS := error.c helper.c
dist_OBJS := $(dist_SRCS:%.c=%.o)
dist_HDRS := error.h helper.h

.SECONDEXPANSION:
checkpath_SRCS := checkpath.c error.c
sv-config_SRCS := $(dist_SRCS) sv-config.c sv-conf.c
sv-shutdown_SRCS  := sv-shutdown.c $(dist_SRCS)
sv-shutdown_LIBS   = $(LIBRT_LIBS)
fstabinfo_SRCS := fstabinfo.c error.c
mountinfo_SRCS := mountinfo.c error.c sv-list.c
waitfile_SRCS  := error.c waitfile.c

sv-run_SRCS   := sv-conf.c sv-deps.c sv-run.c sv-list.c sv-rc.c
sv-run_HDRS   := sv.h sv-conf.h sv-copyright.h
sv-run_CFLAGS  = $(PTHREAD_FLAGS)
sv-run_LDFLAGS = $(PTHREAD_FLAGS)

HDRRS      = $(dist_HDRS) $(sv-run_HDRS)
simple_BINS := checkpath fstabinfo mountinfo sv-config sv-shutdown waitfile
dist_BINS  = $(simple_BINS) sv-run

ifeq (SYSVINIT,yes)
.SECONDEXPANSION:
simple_BINS  += initctl
initctl_SRCS := initctl.c $(dist_SRCS)
endif

all: $(dist_BINS)

clean:
	rm -f $(dist_BINS) $(dist_OBJS) $(sv-run_SRCS:%.c=%.o) $(simple_BINS:%=%.o) \
		$(simple_BINS)

sv-run: $(sv-run_SRCS:%.c=%.o) $(dist_OBJS) $(dist_HDRS) $(sv-run_HDRS)
	$(CC) $(CFLAGS) $($@_CFLAGS) -o $@ $(sv-run_SRCS:%.c=%.o) $(dist_OBJS) $(LDFLAGS) $($@_LDFLAGS) $($@_LIBS)

checkpath: $(checkpath_SRCS:%.c=%.o) error.h
fstabinfo: $(fstabinfo_SRCS:%.c=%.o) error.h
mountinfo: $(mountinfo_SRCS:%.c=%.o) sv-list.h
sv-config: $(sv-config_SRCS:%.c=%.o) $(dist_HDRS) sv-conf.h
sv-shutdown: $(sv-shutdown_SRCS:%.c=%.o) error.h
$(simple_BINS): $$@.o $($$@_SRCS:%.c=%.o)
	$(CC) $(CFLAGS) $($@_CFLAGS) -o $@ $($@_SRCS:%.c=%.o) $(LDFLAGS) $($@_LDFLAGS) $($@_LIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) $(@_CFLAGS) $(CPPFLAGS) $(@_CPPFLAGS) $< -c -o $@

