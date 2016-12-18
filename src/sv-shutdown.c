/*
 * Utility providing an interface {halt,reboot,shutdown,poweroff}
 * per supervision backend.
 *
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-shutdown.c
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "error.h"
#include "helper.h"
#include <grp.h>
#include <pwd.h>
#include <utmpx.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/reboot.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define VERSION "0.12.6"

#ifndef LIBDIR
# define LIBDIR "/lib"
#endif
#define SV_SVC_BACKEND LIBDIR "/sv/opt/SVC_BACKEND"

#define SV_ACTION_SHUTDOWN 0
#define SV_ACTION_SINGLE   1
#define SV_ACTION_REBOOT   6
#define SV_ACTION_MESSAGE  8

const char *prgname;
static int reboot_action;
static int reboot_force;
static int reboot_sync = 1;
static int shutdown_action = -1;

static const char *shortopts = "06crshpfFEHPntkuv";
static const struct option longopts[] = {
	{ "reboot",   0, NULL, 'r' },
	{ "shutdown", 0, NULL, 's' },
	{ "halt",     0, NULL, 'h' },
	{ "poweroff", 0, NULL, 'p' },
	{ "fast",     0, NULL, 'f' },
	{ "fsck",     0, NULL, 'F' },
	{ "force",    0, NULL, 'E' },
	{ "nosync",   0, NULL, 'n' },
	{ "cancel",   0, NULL, 'c' },
	{ "time",     1, NULL, 't' },
	{ "message",  0, NULL, 'k' },
	{ "usage",    0, NULL, 'u' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"System reboot",
	"System shutdown",
	"System halt     (-0 alias)",
	"System poweroff (-0 alias)",
	"Skip  fsck(8) on reboot",
	"Force fsck(8) on reboot",
	"Force halt/reboot/shutdown",
	"Disable filesystem synchronizations",
	"Cancel a waiting shutdown process",
	"Send signal after waiting",
	"Broadcast message only",
	"Print help massage",
	"Print version string",
	NULL
};

__NORETURN__ static void help_message(int status)
{
	int i = 0;

	printf("Usage: %s [OPTIONS] [ACTION] (TIME) [MESSAGE]\n", prgname);
	printf("    -6, -%c, --%-9s         %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	printf("    -0, -%c, --%-9s         %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	printf("    -H, -%c, --%-9s         %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	printf("    -P, -%c, --%-9s         %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	for ( ; longopts_help[i]; i++) {
		printf("        -%c, --%-9s", longopts[i].val, longopts[i].name);
		if (longopts[i].has_arg)
			printf("<ARG>    ");
		else
			printf("         ");
		puts(longopts_help[i]);
	}

	exit(status);
}

static void sighandler(int sig, siginfo_t *info, void *context)
{
	switch (sig) {
	case SIGINT:
	case SIGUSR1:
	case SIGUSR2:
		/*printf("%s: Caught signal %s ...\n", prgname, strsignal(sig));*/
		if (!info->si_uid)
			exit(EXIT_SUCCESS);
		break;
	}
}
static void sigsetup(void)
{
	struct sigaction act;
	int sig[] = { SIGINT, SIGUSR1, SIGUSR2, 0 };
	int i;

	act.sa_sigaction = sighandler;
	act.sa_flags = SA_SIGINFO;
	sigemptyset(&act.sa_mask);
	sigaddset  (&act.sa_mask, SIGQUIT);
	for (i = 0; sig[i]; i++)
		if (sigaction(sig[i], &act, NULL) < 0)
			ERROR("%s: sigaction(%s, ...)", __func__, strsignal(sig[i]));
}

#ifndef UT_LINESIZE
# ifdef __UT_LINESIZE
#  define UT_LINESIZE __UT_LINESIZE
# else
#  define UT_LINESIZE 32
# endif
#endif

static int sv_wall(char *message)
{
	struct utmpx *utxent;
	char dev[UT_LINESIZE+8];
	int fd, n;
	size_t len;

	if (message == NULL) {
		errno = ENOENT;
		return 1;
	}
	len = strlen(message);

	setutxent();
	while ((utxent = getutxent())) {
		if (utxent->ut_type != USER_PROCESS)
			continue;
		snprintf(dev, UT_LINESIZE, "/dev/%s", utxent->ut_line);
		if ((fd = open(dev, O_WRONLY | O_CLOEXEC | O_NOCTTY)) < 0)
			ERROR("Failed to open `%s'", dev);
		if ((n = write(fd, message, len)) != len)
			ERROR("Failed to write to `%s'", dev);
		close(fd);
	}
	endutxent();

	return 0;
}

__NORETURN__ static int sv_shutdown(char *message)
{
	FILE *fp;
	size_t len = 0;
	char *line = NULL, *ptr = NULL;
	char *argv[8], opt[8];
	static const char ent[] = "__SV_NAM__";
	static size_t siz = sizeof(ent)-1;

	if (shutdown_action == SV_ACTION_MESSAGE)
		exit(sv_wall(message));
	if (message)
		sv_wall(message);
	if (reboot_sync)
		sync();
	if (reboot_force)
		exit(reboot(reboot_action));

	argv[0] = "rs", argv[2] = NULL;
	if (shutdown_action == SV_ACTION_REBOOT)
		argv[1] = "reboot";
	else if (shutdown_action == SV_ACTION_SHUTDOWN)
		argv[1] = "shutdown";
	else if (shutdown_action == SV_ACTION_SINGLE) {
		argv[1] = "single";
		goto shutdown;
	}
	else {
		ERR("-0|-6 is required to proceed; see `%s -u'\n", prgname);
		exit(EXIT_FAILURE);
	}

	if ((fp = fopen(SV_SVC_BACKEND, "r")))
		while (rs_getline(fp, &line, &len) > 0)
			if (strncmp(line, ent, siz) == 0) {
				ptr = shell_string_value(line+siz+1);
				ptr = err_strdup(ptr);
				free(line);
				fclose(fp);
				break;
			}
	else
		ERR("Failed to open `%s': %s\n", SV_SVC_BACKEND, strerror(errno));

	if (ptr) {
		if (strcmp(ptr, "runit") == 0) {
			snprintf(opt, sizeof(opt), "%d", shutdown_action);
			argv[0] = "runit-init";
			argv[1] = opt;
		}
		else if (strcmp(ptr, "s6") == 0) {
			snprintf(opt, sizeof(opt), "-%d", shutdown_action);
			argv[0] = "s6-svscanctl";
			argv[1] = opt;
		}
		else if (strncmp(ptr, "daemontools", 11) == 0)
			;
		else
			ERR("Invalid supervision backend: %s\n", ptr);
		free(ptr);
	}
	else
		ERR("%s: Failed to get supervision backend\n", __func__);

	goto shutdown;

shutdown:
	execvp(argv[0], argv);
	ERROR("Failed to execlp(%s, %s)", *argv, argv[1]);
}

int main(int argc, char *argv[])
{
	int opt;
	int fd;
	long h, m, sec;
	time_t t;
	struct timespec tms;
	struct tm *now;
	char *ptr;
	const char *options;
	int open_flags = O_CREAT|O_WRONLY|O_NOFOLLOW|O_TRUNC;
	mode_t open_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;

	prgname = strrchr(argv[0], '/');
	if (prgname == NULL)
		prgname = argv[0];
	else
		prgname++;

	if (geteuid()) {
		fprintf(stderr, "%s: must be the superuser to proceed\n", prgname);
		exit(EXIT_FAILURE);
	}

	if (strcmp(prgname, "reboot") == 0) {
		reboot_action = RB_AUTOBOOT;
		shutdown_action = SV_ACTION_REBOOT;
		options = "dfiknw";
		goto reboot;
	}
	else if (strcmp(prgname, "halt") == 0) {
		reboot_action = RB_HALT_SYSTEM;
		shutdown_action = SV_ACTION_SHUTDOWN;
		options = "dfhinpw";
		goto reboot;
	}
	else if (strcmp(prgname, "poweroff") == 0) {
		reboot_action = RB_POWER_OFF;
		shutdown_action = SV_ACTION_SHUTDOWN;
		options = "dfhinw";
		goto reboot;
	}

	/* Parse options */
	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (opt) {
		case '0':
		case 'P':
		case 'p':
		case 's':
			shutdown_action = SV_ACTION_SHUTDOWN;
			reboot_action = RB_POWER_OFF;
			break;
		case 'H':
		case 'h':
			shutdown_action = SV_ACTION_SHUTDOWN;
			reboot_action = RB_HALT_SYSTEM;
			break;
		case '6':
		case 'r':
			shutdown_action = SV_ACTION_REBOOT;
			reboot_action = RB_AUTOBOOT;
			break;
		case 'E':
			reboot_force = 1;
			break;
		case 'n':
			reboot_sync = 0;
			break;
		case 'c':
			execlp("killall", "-USR2", prgname, NULL);
			break;
		case 'f':
			if ((fd = open("/fastboot", open_flags, open_mode)) < 1)
				ERROR("Failed to create /fastboot", NULL);
			close(fd);
			break;
		case 'F':
			if ((fd = open("/forcefsck", open_flags, open_mode)) < 1)
				ERROR("Failed to create /forcefsck", NULL);
			close(fd);
			break;
		case 't': /* ignored */
			break;
		case 'k':
			shutdown_action = SV_ACTION_MESSAGE;
			break;
		case 'v':
			printf("%s version %s\n", prgname, VERSION);
			exit(EXIT_SUCCESS);
		case '?':
		case 'u':
			help_message(EXIT_SUCCESS);
		default:
			help_message(EXIT_FAILURE);
		}
	}

	if (strcmp(prgname, "shutdown") == 0) {
		if (shutdown_action < 0)
			shutdown_action = SV_ACTION_SINGLE;
		if (!argv[optind]) {
			fprintf(stderr, "Usage: %s [OPTIONS] TIME [MESSAGE] "
					"(TIME argument required)\n", prgname);
			exit(EXIT_FAILURE);
		}

		if (strncmp(argv[optind], "now", 3) == 0)
			sec = 0L;
		else if (argv[optind][0] == '+') {
			sec = 60L*strtol(argv[optind], NULL, 0);
			if (errno == ERANGE)
				goto shutdown;
		}
		else if (argv[optind][1] == ':' || argv[optind][2] == ':') {
			h = strtol(argv[optind], &ptr, 0);
			if (errno == ERANGE)
				goto shutdown;
			ptr++, m += strtol(ptr, NULL, 0);
			if (errno == ERANGE)
				goto shutdown;

			t = time(NULL);
			if (!(now = localtime(&t)))
				goto shutdown;
			sec = 3600L*(h - now->tm_hour) + 60L*(m - now->tm_min) - now->tm_sec;
			if (sec < 0)
				goto shutdown;
		}

		/* setup signal for shutdown cancellation */
		sigsetup();

		if (sec) {
			if (fork() > 0)
				exit(EXIT_SUCCESS);

			tms.tv_sec = sec, tms.tv_nsec = 0;
			while (nanosleep(&tms, &tms))
				if (errno == EINTR)
					continue;
				else
					ERROR("Failed to nanosleep(%d...)", sec);
		}
	}

	return sv_shutdown(argv[optind]);

shutdown:
	ERR("invalid TIME argument -- `%s'\n", argv[optind]);
	exit(EXIT_FAILURE);
reboot:
	while ((opt = getopt(argc, argv, options)) != -1) {
		switch (opt) {
		case 'p':
			shutdown_action = SV_ACTION_SHUTDOWN;
			reboot_action = RB_POWER_OFF;
			break;
		case 'f':
			reboot_force = 1;
			break;
		case 'n':
			reboot_sync = 0;
			break;
		case 'w':
			return EXIT_SUCCESS;
		case 'd':
		case 'i':
		case 'k':
			/* ignored */
			break;
		default:
			printf("Usage: %s [-n] [-w] [-d] [-f] [-i] [-p] [-h]\n", prgname);
			puts(  "    -f    Force halt or reboot, don't call shutdown");
			puts(  "    -n    Don't sync before system halt or reboot");
			puts(  "    -p    Switch off the power when halting the system");
			return EXIT_FAILURE;
		}
	}

	return sv_shutdown(argv[optind]);
}
