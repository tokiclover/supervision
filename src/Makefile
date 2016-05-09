override CFLAGS  += -D_DEFAULT_SOURCE -std=c99 -fPIC -DVERSION=$(VERSION)
CC      ?= cc

.SECONDEXPANSION:
rs_SRCS   = rs-conf.c rs-list.c rs.c
rs_HDRS   = rs.h

dist_SRCS = error.c helper.c
dist_OBJS = $(dist_SRCS:%.c=%.o)
dist_HDRS = error.h helper.h
HDRRS     = $(dist_HDRS) $(rs_HDRS)
dist_BINS = rs

ifdef STATIC
override CFLAGS  += -DSTATIC_SERVICE
endif
ifdef SYSCONFDIR
override CFLAGS  += -DSYSCONFDIR=\"$(SYSCONFDIR)\"
endif

ifdef SYSVINIT
.SECONDEXPANSION:
dist_BINS   += initctl
initctl_SRCS = initctl.c
endif

all: $(dist_BINS)

clean:
	rm -f $(dist_BINS) $(dist_OBJS) $(rs_SRCS:%.c=%.o) $(initctl_SRCS:%.c=%.o)

rs: $(rs_SRCS:%.c=%.o) $(dist_OBJS) $(dist_HDRS) $(rs_HDRS)
	$(CC) $(CFLAGS) -o $@ $(rs_SRCS:%.c=%.o) $(dist_OBJS) $(LDFLAGS)
initctl: $(initctl_SRCS:%.c=%.o) $(dist_OBJS) $(dist_HDRS)
	$(CC) $(CFLAGS) -o $@ $(initctl_SRCS:%.c=%.o) $(dist_OBJS) $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@

