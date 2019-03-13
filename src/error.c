/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)error.c  0.15.0 2019/03/13
 */

#include "error.h"

int ERR_debug, ERR_syslog;

__attribute__((__unused__)) char *print_color(int col, int attr)
{
	static char *buf;
	static int pos, tty = -1;
	int old, val = -1;

	if (!buf) {
		buf = err_malloc(BUFSIZ);
		tty = isatty(STDERR_FILENO);
		if (!tty) tty = -1;
	}
	if (tty == 1) ;
	else {
		if (tty == -1) {
			free((void*)buf);
			tty = 0;
		}
		return "";
	}
	if (pos >= BUFSIZ || (BUFSIZ-pos) < 11) pos = 0U;

	switch (col) {
	case COLOR_BLK:
		if (attr == COLOR_RST)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%dm", COLOR_ES, COLOR_RST);
		else if (attr == COLOR_BG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_BG, COLOR_BLK);
		else if (attr == COLOR_FG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_FG, COLOR_BLK);
		else if (attr == COLOR_FNT)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_FNT, COLOR_FG, COLOR_BLK);
		break;
	case COLOR_RED:
		if (attr == COLOR_BLD)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%dm", COLOR_ES, COLOR_BLD);
		else if (attr == COLOR_BG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_BG, COLOR_RED);
		else if (attr == COLOR_FG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_FG, COLOR_RED);
		else if (attr == COLOR_FNT)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_FNT, COLOR_FG, COLOR_RED);
		break;
	case COLOR_GRN:
		if (attr == COLOR_FNT)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%dm", COLOR_ES, COLOR_FNT);
		else if (attr == COLOR_BG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_BG, COLOR_GRN);
		else if (attr == COLOR_FG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_FG, COLOR_GRN);
		else if (attr == COLOR_FNT)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_FNT, COLOR_FG, COLOR_GRN);
		break;
	case COLOR_YLW:
		if (attr == COLOR_ITA)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%dm", COLOR_ES, COLOR_ITA);
		else if (attr == COLOR_BG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_BG, COLOR_YLW);
		else if (attr == COLOR_FG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_FG, COLOR_YLW);
		else if (attr == COLOR_FNT)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_FNT, COLOR_FG, COLOR_YLW);
		break;
	case COLOR_BLU:
		if (attr == COLOR_UND)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%dm", COLOR_ES, COLOR_UND);
		else if (attr == COLOR_BG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_BG, COLOR_BLU);
		else if (attr == COLOR_FG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_FG, COLOR_BLU);
		else if (attr == COLOR_FNT)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_FNT, COLOR_FG, COLOR_BLU);
		break;
	case COLOR_MAG:
		if (attr == COLOR_BG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_BG, COLOR_MAG);
		else if (attr == COLOR_FG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_FG, COLOR_MAG);
		else if (attr == COLOR_FNT)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_FNT, COLOR_FG, COLOR_MAG);
		break;
	case COLOR_CYN:
		if (attr == COLOR_BG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_BG, COLOR_CYN);
		else if (attr == COLOR_FG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_FG, COLOR_CYN);
		else if (attr == COLOR_FNT)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_FNT, COLOR_FG, COLOR_CYN);
		break;
	case COLOR_WHT:
		if (attr == COLOR_BG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_BG, COLOR_WHT);
		else if (attr == COLOR_FG)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_BLD, COLOR_FG, COLOR_WHT);
		else if (attr == COLOR_FNT)
			val = snprintf(buf+pos, BUFSIZ-pos, "%s%d;%d%dm", COLOR_ES,
			               COLOR_FNT, COLOR_FG, COLOR_WHT);
		break;
	default:
		return "";
	}
	if (val > 0) {
		old = pos;
		pos += 1+val;
		return buf+old;
	}
	return "";
}

__attribute__((format(printf,2,3)))
__attribute__((__noreturn__)) void error(int err, const char *fmt, ...)
{
	char buf[BUFSIZ];
	va_list va;
	va_start(va, fmt);

	vsnprintf(buf, BUFSIZ-1, fmt, va);
	if (errno) {
		unsigned long int len = strlen(buf);
		snprintf(buf+len, BUFSIZ-len-1, ": %s", strerror(errno));
	}
	fprintf(stderr, "%s\n", buf);
	fflush(NULL);

	va_end(va);
	if (err >= 0)
		_exit(err);
	else
		_exit(EXIT_FAILURE);
}

__attribute__((format(printf,2,3))) void err_syslog(int priority, const char *fmt, ...)
{
	va_list ap;

	if (ERR_syslog) {
		if ((priority == LOG_DEBUG) && !ERR_debug) return;
		va_start(ap, fmt);
		vsyslog(priority, fmt, ap);
		va_end(ap);
		return;
	}

	switch (priority) {
	case LOG_EMERG:
	case LOG_ALERT:
	case LOG_CRIT:
	case LOG_ERR:
		fprintf(stderr, "%s: %serror%s: ", progname,
				print_color(COLOR_RED, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST));
		break;
	case LOG_WARNING:
		fprintf(stderr, "%s: %swarning%s: ", progname, 
				print_color(COLOR_YLW, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST));
		break;
	case LOG_NOTICE:
	case LOG_INFO:
		fprintf(stderr, "%s: %sinfo%s: ", progname, 
				print_color(COLOR_BLU, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST));
		break;
	case  LOG_DEBUG:
		if (!ERR_debug) return;
		fprintf(stderr, "%s: debug: ", progname);
		break;
	}
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
__attribute__((__unused__)) void *err_malloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr)
		return ptr;
	ERR_EXIT;
}

__attribute__((__unused__)) void *err_calloc(size_t num, size_t size)
{
	void *ptr = calloc(num, size);
	if (ptr)
		return ptr;
	ERR_EXIT;
}

__attribute__((__unused__)) void *err_realloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr || size == 0)
		return ptr;
	ERR_EXIT;
}

__attribute__((__unused__)) char *err_strdup(const char* str)
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
