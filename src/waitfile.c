/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)waitfile.c  0.12.6.4 2016/12/24
 */

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#define VERSION "0.1.0"

#define WAIT_SECS 60    /* default delay */
#define WAIT_MSEC 1000  /* interval for displaying warning */
#define WAIT_POLL 100   /* poll interval */

const char *progname;

#define ERR(fmt, ...) fprintf(stderr, "ERROR: %s: " fmt, progname, __VA_ARGS__)
#define WARN(fmt, ...) fprintf(stderr, "WARN: %s: " fmt, progname, __VA_ARGS__)

enum {
	FILE_EXIST = 0x01,
	FILE_MESG  = 0x02,
};

static const char *shortopts = "Ehmv";
static const struct option longopts[] = {
	{ "noexits",  0, NULL, 'E' },
	{ "message",  0, NULL, 'm' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Check file no-existance",
	"Print wait massage",
	"Print help massage",
	"Print version string",
	NULL
};

static void help_message(int status)
{
	int i = 0;

	printf("Usage: %s [-E] [-m] TIMEOUT FILENAME\n", progname);
	for ( ; longopts_help[i]; i++)
		printf("    -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);

	exit(status);
}

static int waitfile(const char *file, long unsigned int timeout, int flags)
{
	int i, j, r;
	int e = flags & FILE_EXIST, m = flags & FILE_MESG;
	long unsigned int msec = WAIT_MSEC, nsec, ssec = 10;

	if (timeout < ssec) {
		nsec = timeout;
		msec = 1000*timeout;
	}
	else
		nsec = timeout % ssec;
	nsec = nsec ? nsec : ssec;

	for (i = 0; i < timeout; ) {
		for (j = WAIT_POLL; j <= msec; j += WAIT_POLL) {
			r = access(file, F_OK);
			if ((r && e) || (!r && !e))
				return 0;
			/* use poll(3p) as a milliseconds timer (sleep(3) replacement) */
			if (poll(0, 0, WAIT_POLL) < 0)
				return 2;
		}
		if (!(++i % ssec) && m)
			WARN("waiting for %s (%d seconds)\n", file, i);
	}

	r = access(file, F_OK);
	if ((r && flags) || (!r && !e))
		return 0;
	else
		return 1;
}

int main(int argc, char *argv[])
{
	int flags = 0, opt;
	long unsigned int timeout;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		progname++;

	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (opt) {
		case 'E':
			flags |= FILE_EXIST;
			break;
		case 'm':
			flags |= FILE_MESG;
			break;
		case 'v':
			printf("%s version %s\n", progname, VERSION);
			exit(EXIT_SUCCESS);
		case 'h':
			help_message(EXIT_SUCCESS);
		default:
			help_message(EXIT_FAILURE);
		}
	}
	argc -= optind, argv += optind;

	if (argc < 2) {
		ERR("Insufficient number of arguments -- `%d' (two required)\n", argc);
		fprintf(stderr, "usage: %s [-E] TIMEOUT FILENAME\n", progname);
		exit(EXIT_FAILURE);
	}

	timeout = strtoul(*argv, NULL, 10);
	if (errno == ERANGE) {
		ERR("Invalid agument -- `%s'\n", *argv);
		exit(EXIT_FAILURE);
	}

	return waitfile(*++argv, timeout, flags);
}
