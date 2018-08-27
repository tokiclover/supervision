/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)waitfile.c  0.2.0 2016/12/24
 */

#include "config.h"
#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "error.h"

#define VERSION "0.4.0"

#define WAIT_SECS 60    /* default delay */
#define WAIT_MSEC 1000  /* interval for displaying warning */
#define WAIT_POLL 100   /* poll interval */

const char *progname;

static const char *signame[] = { "SIGHUP", "SIGINT", "SIGQUIT", "SIGTERM", "SIGKILL" };
static void sighandler(int sig);
static void sigsetup(void);

static char *NM, *FP;
static int EF, MF, PF;
static unsigned long int TM;

static const char *shortopts = "Ef:hmn:p:t:v";
static const struct option longopts[] = {
	{ "file"   ,  0, NULL, 'f' },
	{ "name"   ,  0, NULL, 'n' },
	{ "pid"    ,  0, NULL, 'p' },
	{ "timeout",  0, NULL, 't' },
	{ "noexits",  0, NULL, 'E' },
	{ "message",  0, NULL, 'm' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"File path to check",
	"Name [tail] to display",
	"Check PID existance",
	"Timeout in second(s)",
	"Check file no-existance",
	"Print wait massage",
	"Print help massage",
	"Print version string",
	NULL
};

__attribute__((__noreturn__)) static void help_message(int status)
{
	int i = 0;

	printf("Usage: %s [OPTIONS] [-t] TIMEOUT [-f] FILENAME\n", progname);
	for ( ; longopts_help[i]; i++)
		printf("    -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);

	exit(status);
}

static void sighandler(int sig)
{
	int i = -1, serrno = errno;

	switch (sig) {
	case SIGHUP:
		i = 0;
	case SIGINT:
		if (i < 0) i = 1;
	case SIGTERM:
		if (i < 0) i = 3;
	case SIGQUIT:
		if (i < 0) i = 2;
	case SIGKILL:
		if (i < 0) i = 4;
		ERR("caught %s, aborting\n", signame[i]);
		exit(EXIT_FAILURE);
	default:
		ERR("caught unknown signal %d\n", sig);
	}

	/* restore errno */
	errno = serrno;
}

static void sigsetup(void)
{
	int *sig = (int []){ SIGHUP, SIGINT, SIGQUIT, SIGTERM, 0 };
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sighandler;
	for ( ; *sig; sig++)
		if (sigaction(*sig, &sa, NULL)) {
			ERROR("%s: sigaction(%d,..)", __func__, *sig);
		}
}

static int waitfile(void)
{
	int i, j;
	int fd;
	int FF = O_RDONLY;
	int wf = 0;
	long unsigned int msec = WAIT_MSEC, nsec, ssec = 1;
	char WF[512] = RUNDIR "/sv/.tmp/wait/";
	size_t len = strlen(WF);
	FILE * fp;
	pid_t pid = 0;

	if (TM < ssec) {
		nsec = TM;
		msec = 1000LU*TM;
	}
	else
		nsec = TM % ssec;
	nsec = nsec ? nsec : ssec;

	if (!NM) NM = strrchr(FP, '/')+1LU;
	snprintf(WF+len, sizeof(WF)-len, "%s", NM);
	if (!FP) FP = WF;
	if (FP == WF || !strcmp(WF, FP)) {
		FF = O_WRONLY | O_EXCL | O_CREAT;
		PF++;
		wf++;
getpid:
		if ((fd = open(FP, O_RDONLY, 0644)) < 0)
			return 0;
		if ((fp = fdopen(fd, "r"))) {
			if (!fscanf(fp, "pid=%d:", &pid)) pid = 0;
			(void)close(fd);
		}
		if ((pid == getpid())) {
			(void)unlink(FP);
			return 0;
		}
		if (wf && !pid) return 0;
	}
	else if (PF) goto getpid;

	for (i = 0; i < TM; ) {
		for (j = WAIT_POLL; j <= msec; j += WAIT_POLL) {
			fd = open(FP, FF, 0644);
			if (fd < 0 &&  EF) return 0;
			if (fd > 0 && !EF) {
				if (wf) {
					if ((fp = fdopen(fd, "w"))) {
						fprintf(fp, "pid=%d:command=wait", getpid());
					}
				}
				return 0;
			}
			if (pid && kill(pid, 0)) {
				(void)unlink(FP);
				return 0;
			}

			/* use poll(3p) as a milliseconds timer (sleep(3) replacement) */
			if (poll(0, 0, WAIT_POLL) < 0)
				return EXIT_FAILURE;
		}
		if (!(++i % ssec) && MF) {
			if (wf)
				printf("%s%s%s: %sinfo%s: waiting for `%s' (%d seconds)\n",
						print_color(COLOR_YLW, COLOR_FG), NM,
						print_color(COLOR_RST, COLOR_RST),
						print_color(COLOR_BLU, COLOR_FG),
						print_color(COLOR_RST, COLOR_RST), FP, i);
			else
				INFO("waiting for `%s' (%d seconds)\n", FP, i);
		}
	}

	fd = open(FP, FF, 0644);
	if (fd < 0 &&  EF) return 0;
	if (fd > 0 && !EF) {
		(void)close(fd);
		(void)unlink(FP);
		return 0;
	}
	return 2;
}

int main(int argc, char *argv[])
{
	int opt;

	sigsetup();
	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		progname++;

	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (opt) {
		case 'E':
			EF++;
			break;
		case 'm':
			MF++;
			break;
		case 'n':
			NM = optarg;
			break;
		case 'p':
			PF++;
			break;
		case 't':
			TM = strtoul(optarg, NULL, 10);
			if (errno == ERANGE) {
				ERR("Invalid agument -- `%s'\n", *argv);
				exit(EXIT_FAILURE);
			}
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

	if (!TM) {
		TM = strtoul(*argv, NULL, 10);
		if (errno == ERANGE) {
			ERR("TIMOUT argument is required!\n", NULL);
			goto reterr;
		}
	}
	argc--; argv++;
	if (!FP) FP = *argv;
	if (!FP && !NM) {
		ERR("`[-f] FILENAME' or `-n NAME' argument is required!\n", NULL);
		goto reterr;
	}

	return waitfile();
reterr:
	ERR("invalid agument -- `%s'\n", *argv);
	fprintf(stderr, "usage: %s [OPTIONS] [-t] TIMEOUT [-f] FILENAME\n", progname);
	exit(EXIT_FAILURE);
}
