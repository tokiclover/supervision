/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)err_syslog.c  0.15.0 2019/03/13
 */

#include <syslog.h>
#include "error.h"

int debug, log;

__attribute__((format(printf,2,3))) void err_syslog(int priority, const char *fmt, ...)
{
	va_list ap;

	if (log) {
		if ((priority == LOG_DEBUG) && !debug) return;
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
		if (!debug) return;
		fprintf(stderr, "%s: debug: ", progname);
		break;
	}
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

