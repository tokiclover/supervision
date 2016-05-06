override CFLAGS  += -D_DEFAULT_SOURCE -std=c99 -fPIC -DVERSION=$(VERSION)
CC      ?= cc

SRCS = error.c helper.c rs-conf.c rs-list.c rs.c
OBJS = $(SRCS:%.c=%.o)
HDRS = error.h helper.h rs.h
BINS = rs

ifdef STATIC
override CFLAGS  += -DSTATIC_SERVICE
endif
ifdef SYSCONFDIR
override CFLAGS  += -DSYSCONFDIR=\"$(SYSCONFDIR)\"
endif

all: $(BINS)

clean:
	rm -f $(BINS) $(OBJS)

$(BINS): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@

