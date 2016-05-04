CFLAGS  += -D_DEFAULT_SOURCE -std=c99 -fPIC #-I./include
LDFLAGS +=
CC       = cc

SRCS = error.c helper.c rs-list.c rs.c
OBJS = $(SRCS:%.c=%.o)
HDRS = error.h helper.h rs.h
BINS = rs

all: $(BINS)

clean:
	rm -f $(BINS) $(OBJS)

$(BINS): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@

