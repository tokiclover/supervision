/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 */

#include "error.h"

__NORETURN__ void error(int err, const char *fmt, ...)
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

