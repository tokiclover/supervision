/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)error.h
 */

#ifndef _ERROR_H
#define _ERROR_H

#if __GNUC__ > 2 || defined(__INTEL_COMPILER)
# define __NORETURN__ __attribute__((__noreturn__))
# define __UNUSED__ __attribute__((__unused__))
#else
# define __NORETURN__
# define __UNUSED__
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define _POSIX_C_SOURCE 200809L
#if defined SOLARIS
#  define _XOPEN_SOURCE 600
#else
#  define _XOPEN_SOURCE 700
#endif

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

#define PRGNAME prgname
#define ERR(fmt, ...) fprintf(stderr, "ERROR: %s: " fmt, PRGNAME, __VA_ARGS__)
#define WARN(fmt, ...) fprintf(stderr, "WARN: %s: " fmt, PRGNAME, __VA_ARGS__)
#define ERROR(fmt, ...)  error(errno, "ERROR: %s: " fmt, PRGNAME, __VA_ARGS__)
#define ERR_EXIT ERROR("", NULL)
#if defined DEBUG
#  define DBG(fmt, ...) fprintf(stderr, "%s:%s:%d: " fmt, PRGNAME, __FILE__, __LINE__, __VA_ARGS__)
#else
#  define DBG(fmt, ...)
#endif

__NORETURN__ void error(int err, const char *fmt, ...);
extern const char *prgname;

__UNUSED__ void *err_malloc(size_t size);
__UNUSED__ void *err_calloc(size_t num, size_t size);
__UNUSED__ void *err_realloc(void *ptr, size_t size);
__UNUSED__ char *err_strdup(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* _ERROR_H */
