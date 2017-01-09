-include ../config.mak
CC      ?= cc

dist_SRCS := error.c helper.c
dist_OBJS := $(dist_SRCS:%.c=%.o)
dist_HDRS := error.h helper.h

.SECONDEXPANSION:
checkpath_SRCS := checkpath.c $(dist_SRCS)
sv-shutdown_SRCS  := sv-shutdown.c $(dist_SRCS)
fstabinfo_SRCS := fstabinfo.c error.c
mountinfo_SRCS := mountinfo.c error.c sv-list.c
waitfile_SRCS  := waitfile.c

rs_SRCS   := sv-conf.c sv-deps.c rs.c sv-list.c sv-stage.c
rs_HDRS   := sv.h
rs_CFLAGS  = $(PTHREAD_FLAGS)
rs_LDFLAGS = $(PTHREAD_FLAGS)

HDRRS      = $(dist_HDRS) $(rs_HDRS)
simple_BINS := checkpath fstabinfo mountinfo sv-shutdown waitfile
dist_BINS  = $(simple_BINS) rs

ifeq (SYSVINIT,yes)
.SECONDEXPANSION:
simple_BINS  += initctl
initctl_SRCS := initctl.c $(dist_SRCS)
endif

all: $(dist_BINS)

clean:
	rm -f $(dist_BINS) $(dist_OBJS) $(rs_SRCS:%.c=%.o) $(simple_BINS:%=%.o) \
		$(simple_BINS)

rs: $(rs_SRCS:%.c=%.o) $(dist_OBJS) $(dist_HDRS) $(rs_HDRS)
	$(CC) $(CFLAGS) $($@_CFLAGS) -o $@ $(rs_SRCS:%.c=%.o) $(dist_OBJS) $(LDFLAGS) $($@_LDFLAGS) $($@_LIBS)

mountinfo.o: $(mountinfo_SRCS:%.c=%.o) sv-list.o
$(simple_BINS): $$@.o $(dist_OBJS) $(dist_HDRS) $($$@_SRCS:%.c=%.o)
	$(CC) $(CFLAGS) $($@_CFLAGS) -o $@ $($@_SRCS:%.c=%.o) $(LDFLAGS) $($@_LDFLAGS) $($@_LIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) $(@_CFLAGS) $(CPPFLAGS) $(@_CPPFLAGS) $< -c -o $@

