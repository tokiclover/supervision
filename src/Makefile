-include ../config.mak
CC      ?= cc

dist_SRCS := error.c helper.c
dist_OBJS := $(dist_SRCS:%.c=%.o)
dist_HDRS := error.h helper.h

.SECONDEXPANSION:
checkpath_SRCS := checkpath.c $(dist_SRCS)
sv-shutdown_SRCS  := sv-shutdown.c $(dist_SRCS)
fstabinfo_SRCS := fstabinfo.c error.c
mountinfo_SRCS := mountinfo.c error.c

rs_SRCS   := rs-conf.c rs-deps.c rs-list.c rs.c
rs_HDRS   := rs.h

HDRRS      = $(dist_HDRS) $(rs_HDRS)
simple_BINS := checkpath fstabinfo mountinfo sv-shutdown
dist_BINS  = $(simple_BINS) rs

ifeq (SYSVINIT,yes)
.SECONDEXPANSION:
simple_BINS  += initctl
initctl_SRCS := initctl.c $(dist_SRCS)
endif

all: $(dist_BINS)

clean:
	rm -f $(dist_BINS) $(dist_OBJS) $(rs_SRCS:%.c=%.o) $(simple_BINS:%=%.o) \
		$(simple_BINS:%=../sv/.lib/bin/%)

rs: $(rs_SRCS:%.c=%.o) $(dist_OBJS) $(dist_HDRS) $(rs_HDRS)
	$(CC) $(CFLAGS) -o $@ $(rs_SRCS:%.c=%.o) $(dist_OBJS) $(LDFLAGS)

$(simple_BINS): $$@.o $(dist_OBJS) $(dist_HDRS)
	$(CC) $(CFLAGS) -o ../sv/.lib/bin/$@ $($@_SRCS:%.c=%.o) $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@

