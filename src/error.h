/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)error.h  0.14.0 2018/08/06
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

/* define color macros */
#define COLOR_ES "\033["
#define COLOR_BG 4
#define COLOR_FG 3
enum {
	COLOR_RST,
#define COLOR_RST COLOR_RST
	COLOR_BLD,
#define COLOR_BLD COLOR_BLD
	COLOR_FNT,
#define COLOR_FNT COLOR_FNT
	COLOR_ITA,
#define COLOR_ITA COLOR_ITA
	COLOR_UND,
#define COLOR_UND COLOR_UND
};
enum {
	COLOR_BLK,
#define COLOR_BLK COLOR_BLK
	COLOR_RED,
#define COLOR_RED COLOR_RED
	COLOR_GRN,
#define COLOR_GRN COLOR_GRN
	COLOR_YLW,
#define COLOR_YLW COLOR_YLW
	COLOR_BLU,
#define COLOR_BLU COLOR_BLU
	COLOR_MAG,
#define COLOR_MAG COLOR_MAG
	COLOR_CYN,
#define COLOR_CYN COLOR_CYN
	COLOR_WHT,
#define COLOR_WHT COLOR_WHT
};

#define ERR(fmt, ...) fprintf(stderr, "%s: %serror%s: "    fmt, progname, \
		print_color(COLOR_RED, COLOR_FG), \
		print_color(COLOR_RST, COLOR_RST), __VA_ARGS__)
#define WARN(fmt, ...) fprintf(stderr, "%s: %swarning%s: " fmt, progname, \
		print_color(COLOR_YLW, COLOR_FG), \
		print_color(COLOR_RST, COLOR_RST), __VA_ARGS__)
#define INFO(fmt, ...) fprintf(stdout, "%s: %sinfo%s: " fmt, progname, \
		print_color(COLOR_BLU, COLOR_FG), \
		print_color(COLOR_RST, COLOR_RST), __VA_ARGS__)
#define ERROR(fmt, ...)  error(-errno, "%s: %serror%s: "   fmt, progname, \
		print_color(COLOR_RED, COLOR_FG), \
		print_color(COLOR_RST, COLOR_RST), __VA_ARGS__)
#define ERR_EXIT ERROR("", NULL)
#if defined DEBUG
#  define DBG(fmt, ...) fprintf(stderr, "%s: debug: %s:%d: " fmt, progname, __FILE__, __LINE__, __VA_ARGS__)
#else
#  define DBG(fmt, ...)
#endif

__attribute__((__unused__)) char *print_color(int col, int attr);
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
