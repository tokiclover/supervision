/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)error.h  0.13.0 2016/12/28
 */

#ifndef ERROR_H
#define ERROR_H

#if (defined(__GNUC__) && (__GNUC__ > 2)) || defined(__INTEL_COMPILER) || defined(__arm__)
# define HAVE_GNU_STYLE_ATTRIBUTE 1
#else
# define __attribute__(x)
#endif


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ERR(fmt, ...) fprintf(stderr, "ERROR: %s: " fmt, progname, __VA_ARGS__)
#define WARN(fmt, ...) fprintf(stderr, "WARN: %s: " fmt, progname, __VA_ARGS__)
#define ERROR(fmt, ...)  error(errno, "ERROR: %s: " fmt, progname, __VA_ARGS__)
#define ERR_EXIT ERROR("", NULL)
#if defined DEBUG
#  define DBG(fmt, ...) fprintf(stderr, "%s:%s:%d: " fmt, PROGNAME, __FILE__, __LINE__, __VA_ARGS__)
#else
#  define DBG(fmt, ...)
#endif

__attribute__((format(printf,2,3)))
__attribute__((__noreturn__)) void error(int err, const char *fmt, ...);
extern const char *progname;

__attribute__((__unused__)) void *err_malloc(size_t size);
__attribute__((__unused__)) void *err_calloc(size_t num, size_t size);
__attribute__((__unused__)) void *err_realloc(void *ptr, size_t size);
__attribute__((__unused__)) char *err_strdup(const char *str);
__attribute__((__unused__)) ssize_t err_write(int fd, const char *what, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* ERROR_H */
