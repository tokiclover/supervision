/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)error.c  0.13.0 2016/12/28
 */

#include "error.h"

__attribute__((format(printf,2,3)))
__attribute__((__noreturn__)) void error(int err, const char *fmt, ...)
{
	char buf[BUFSIZ];
	va_list va;
	va_start(va, fmt);

	vsnprintf(buf, BUFSIZ-1, fmt, va);
	if (errno) {
		int len = strlen(buf);
		snprintf(buf+len, BUFSIZ-len-1, ": %s", strerror(errno));
	}
	fprintf(stderr, "%s\n", buf);
	fflush(NULL);

	va_end(va);
	if (err >= 0)
		exit(err);
	else
		abort();
}

void *err_malloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr)
		return ptr;
	ERR_EXIT;
}

void *err_calloc(size_t num, size_t size)
{
	void *ptr = calloc(num, size);
	if (ptr)
		return ptr;
	ERR_EXIT;
}

void *err_realloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr || size == 0)
		return ptr;
	ERR_EXIT;
}

char *err_strdup(const char* str)
{
	char *ptr = strdup(str);
	if (!str)
		return NULL;
	if (ptr)
		return ptr;
	ERR_EXIT;
}

__attribute__((__unused__)) inline ssize_t err_write(int fd, const char *what, const char *path)
{
	ssize_t l, r;
	off_t o = 0;
	l = strlen(what);
	do {
		r = write(fd, what+o, l);
		if (r < 0) {
			if (errno == EINTR) continue;
			ERR("Failed to write fo `%s': %s\n", path, strerror(errno));
			return r;
		}
		o += (off_t)r;
		l -= r;
	} while(l);
	return 0;
}
