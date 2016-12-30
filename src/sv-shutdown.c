/*
 * Utility providing an interface {halt,reboot,shutdown,poweroff}
 * per supervision backend.
 *
 * Copyright (c) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-shutdown.c  0.12.6.4 2016/12/28
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
#include <paths.h>
#include <sys/reboot.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>

#define VERSION "0.12.6.4"

#ifndef LIBDIR
# define LIBDIR "/lib"
#endif
#define SV_SVC_BACKEND LIBDIR "/sv/opt/SVC_BACKEND"

#define FASTBOOT  "/fastboot"
#define FORCEFSCK "/forcefsck"

#define SV_ACTION_SHUTDOWN 0
#define SV_ACTION_SINGLE   1
#define SV_ACTION_REBOOT   6
#define SV_ACTION_MESSAGE  8

const char *progname;
static int reboot_action;
static int reboot_force;
static int reboot_sync = 1;
static int shutdown_action = -1;
static uid_t uid;

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

_noreturn_ static void help_message(int status)
{
	int i = 0;

	printf("Usage: %s [OPTIONS] [ACTION] (TIME) [MESSAGE]\n", progname);
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

static void sighandler(int sig)
{
	const char *action[] = { "shutdown", "singlge", "reboot" };
	if (shutdown_action == SV_ACTION_REBOOT)
		shutdown_action = 2;
	/*printf("%s: Caught signal %s ...\n", progname, strsignal(sig));*/
	if (!access(FASTBOOT , F_OK)) unlink(FASTBOOT);
	if (!access(FORCEFSCK, F_OK)) unlink(FORCEFSCK);
	if (!access(_PATH_NOLOGIN  , F_OK)) unlink(_PATH_NOLOGIN);
	if (sig > 0) {
		fprintf(stderr, "%s: cancelling system %s\n", progname,
				action[shutdown_action]);
		exit(EXIT_SUCCESS);
	}
}
static int sigsetup(void)
{
	struct sigaction act;
	int sig[] = { SIGINT, SIGTERM, SIGUSR1, 0 };
	int i;

	act.sa_handler = sighandler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset  (&act.sa_mask, SIGQUIT);
	for (i = 0; sig[i]; i++)
		if (sigaction(sig[i], &act, NULL) < 0) {
			ERR("sigaction(%s, ...): %s\n", strsignal(sig[i]), strerror(errno));
			return 1;
		}
	return 0;
}

#ifndef UT_LINESIZE
# ifdef __UT_LINESIZE
#  define UT_LINESIZE __UT_LINESIZE
# else
#  define UT_LINESIZE 32
# endif
#endif

static int sv_wall(char **argv)
{
	struct utmpx *utxent;
	char dev[UT_LINESIZE+8];
	int i;
	FILE *fp;

	if (!*argv || !argv[0][0]) {
		errno = ENOENT;
		return 1;
	}

	setutxent();
	while ((utxent = getutxent())) {
		if (utxent->ut_type != USER_PROCESS)
			continue;
		snprintf(dev, UT_LINESIZE, "%s%s", _PATH_DEV, utxent->ut_line);
		if ((fp = fopen(dev, "w")) == NULL) {
			ERR("Failed to open `%s': %s\n", dev, strerror(errno));
			sighandler(-1);
			exit(EXIT_FAILURE);
		}
		for (i = 0; argv[i]; i++)
			fprintf(fp, "%s\n", argv[i]);
		fclose(fp);
	}
	endutxent();

	return 0;
}

_noreturn_ static int sv_shutdown(char **message)
{
	FILE *fp;
	size_t len = 0;
	char *line = NULL, *ptr = NULL;
	char *argv[8], arg[8];
	char *whom;
	struct passwd *pw;
	static const char ent[] = "__SV_NAM__";
	static size_t siz = sizeof(ent)-1;

	if (shutdown_action == SV_ACTION_MESSAGE)
		exit(sv_wall(message));
	if (*message)
		sv_wall(message);
	if (reboot_sync)
		sync();
	if (reboot_force)
		exit(reboot(reboot_action));

	argv[0] = "sv-stage", argv[2] = NULL;
	if (shutdown_action == SV_ACTION_REBOOT)
		argv[1] = "reboot";
	else if (shutdown_action == SV_ACTION_SHUTDOWN)
		argv[1] = "shutdown";
	else if (shutdown_action == SV_ACTION_SINGLE) {
		argv[1] = "single";
		goto shutdown;
	}
	else {
		ERR("-0|-6 is required to proceed; see `%s -u'\n", progname);
		exit(EXIT_FAILURE);
	}

	if ((fp = fopen(SV_SVC_BACKEND, "r")))
		while (sv_getline(fp, &line, &len) > 0)
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
			snprintf(arg, sizeof(arg), "%d", shutdown_action);
			argv[0] = "runit-init";
			argv[1] = arg;
		}
		else if (strcmp(ptr, "s6") == 0) {
			snprintf(arg, sizeof(arg), "-%d", shutdown_action);
			argv[0] = "s6-svscanctl";
			argv[1] = arg;
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
	if (!access(_PATH_NOLOGIN, F_OK))
		unlink(_PATH_NOLOGIN);

	if (!(whom = getlogin()))
		whom = (pw = getpwuid(uid)) ? pw->pw_name : "???";
	openlog(progname, LOG_PID | LOG_CONS, LOG_AUTH);
	if (shutdown_action == SV_ACTION_REBOOT)
		syslog(LOG_CRIT, "system rebooted (by USER=%s)", whom);
	else if (shutdown_action == SV_ACTION_SINGLE)
		syslog(LOG_CRIT, "single user mode runlevel (by USER=%s)", whom);
	else
		syslog(LOG_CRIT, "system halted (by USER=%s)", whom);
	closelog();

	execvp(argv[0], argv);
	ERR("Failed to execlp(%s, %s): %s\n", *argv, argv[1], strerror(errno));
	sighandler(-1);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int i;
	int nologin = 0;
	FILE *fp;
	long h, m;
	time_t t;
	struct timespec ts;
	struct tm *now;
	char *ptr;
	const char *options;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		progname++;

	uid = getuid();
#ifndef DEBUG
	if (geteuid()) {
		errno = EPERM;
		ERROR("must be the superuser to proceed", NULL);
	}
#endif
	/* support setuid to a special group */
	setuid(geteuid());

	if (strcmp(progname, "reboot") == 0) {
		reboot_action = RB_AUTOBOOT;
		shutdown_action = SV_ACTION_REBOOT;
		options = "dfiknw";
		goto reboot;
	}
	else if (strcmp(progname, "halt") == 0) {
		reboot_action = RB_HALT_SYSTEM;
		shutdown_action = SV_ACTION_SHUTDOWN;
		options = "dfhinpw";
		goto reboot;
	}
	else if (strcmp(progname, "poweroff") == 0) {
		reboot_action = RB_POWER_OFF;
		shutdown_action = SV_ACTION_SHUTDOWN;
		options = "dfhinw";
		goto reboot;
	}

	/* Parse options */
	while ((i = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (i) {
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
			execlp("pkill",   "pkill",   "-INT", "-x", progname, NULL);
			execlp("killall", "killall", "-INT", "-e", progname, NULL);
			ERROR("Failed to execute neither pkill nor killall", NULL);
			break;
		case 'f':
			if (fclose(fopen(FASTBOOT, "w"))) {
				ERR("Failed to create %s\n", FASTBOOT);
				goto setup_error;
			}
			break;
		case 'F':
			if (fclose(fopen(FORCEFSCK, "w"))) {
				ERR("Failed to create %s\n", FORCEFSCK);
				goto setup_error;
			}
			break;
		case 't': /* ignored */
			break;
		case 'k':
			shutdown_action = SV_ACTION_MESSAGE;
			break;
		case 'v':
			printf("%s version %s\n", progname, VERSION);
			exit(EXIT_SUCCESS);
		case 'u':
			help_message(EXIT_SUCCESS);
		default:
			sighandler(-1);
			help_message(EXIT_FAILURE);
		}
	}
	argc -= optind, argv += optind;

	if (strcmp(progname, "shutdown") == 0) {
		if (shutdown_action < 0)
			shutdown_action = SV_ACTION_SINGLE;
		if (argc < 1) {
			fprintf(stderr, "Usage: %s [OPTIONS] TIME [MESSAGE] "
					"(TIME argument required)\n", progname);
			exit(EXIT_FAILURE);
		}

		if (strncmp(*argv, "now", 3) == 0)
			m = 0L;
		else if (argv[0][0] == '+') {
			m = strtol(*argv, NULL, 0);
			if (errno == ERANGE)
				goto time_error;
		}
		else if (argv[0][1] == ':' || argv[0][2] == ':') {
			h = strtol(*argv, &ptr, 0);
			if (errno == ERANGE)
				goto time_error;
			ptr++, m += strtol(ptr, NULL, 0);
			if (errno == ERANGE)
				goto time_error;

			t = time(NULL);
			if (!(now = localtime(&t)))
				goto time_error;
			m = 60L*(h - now->tm_hour) + (m - now->tm_min);
			if (m < 0)
				goto time_error;
		}
		argv++;

		/* setup signal for shutdown cancellation */
		if (sigsetup()) goto setup_error;

		if (m) {
			if (fork() > 0)
				exit(EXIT_SUCCESS);

			while (m) {
				ts.tv_sec = 60, ts.tv_nsec = 0;

				if (!nologin && m < 6) {
					fp = fopen(_PATH_NOLOGIN, "w");
					if (fp) {
						if (*argv && argv[0][0])
							for (i = 0; argv[i]; i++)
								fprintf(fp, "%s\n", argv[i]);
						else {
							t = time(NULL)+m*60L;
							fprintf(fp, "The system is going down at %s", ctime(&t));
						}
						fclose(fp);
					}
					else
						WARN("Failed to open %s\n", _PATH_NOLOGIN);
					nologin++;
				}

				while (nanosleep(&ts, &ts))
					if (errno == EINTR)
						continue;
					else {
						ERR("Failed to nanosleep(ts.tv_sec=%ld...): %s\n",
								ts.tv_sec, strerror(errno));
						goto setup_error;
					}
				m--;
			}
		}
	}

	return sv_shutdown(argv);

time_error:
	ERR("invalid (TIME) argument -- `%s'\n", *argv);
setup_error:
	sighandler(-1);
	exit(EXIT_FAILURE);
reboot:
	while ((i = getopt(argc, argv, options)) != -1) {
		switch (i) {
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
			printf("Usage: %s [-fnp]\n", progname);
			puts(  "    -f    Force halt or reboot, don't call shutdown");
			puts(  "    -n    Don't sync before system halt or reboot");
			puts(  "    -p    Switch off the power when halting the system");
			return EXIT_FAILURE;
		}
	}

	return sv_shutdown(argv);
}
