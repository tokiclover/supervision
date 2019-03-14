/*
 * Copyright (c) 2016-2019 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-init.c  0.15.0 2019/01/31
 */

#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <paths.h>
#include <utmpx.h>
#include "config.h"
#include "error.h"
#include "sv-copyright.h"

#define VERSION "0.15.0"
#define SV_LIBDIR LIBDIR "/sv"

const char *progname;

static void signal_handler(int sig);
__attribute__((__noreturn__)) static void help_message(int status);

#define SHUTDOWN_TIMEOUT 120LU
#define DEATH_TIMEOUT 10LU
static int timeout;

enum SV_INIT_LEVEL {
	SV_SHUTDOWN_LEVEL,
#define SV_SHUTDOWN_LEVEL 0U
	SV_SINGLE_LEVEL,
#define SV_SINGLE_LEVEL 1U
	SV_NONETWORK_LEVEL,
#define SV_NONETWORK_LEVEL 2U
	SV_DEFAULT_LEVEL,
#define SV_DEFAULT_LEVEL 3U
	SV_SYSINIT_LEVEL,
#define SV_SYSINIT_LEVEL 4U
	SV_SYSBOOT_LEVEL,
#define SV_SYSBOOT_LEVEL 5U
	SV_REBOOT_LEVEL
#define SV_REBOOT_LEVEL 6U
};
#define SV_SHUTDOWN_RUNLEVEL "shutdown"
#define SV_SINGLE_RUNLEVEL "single"
#define SV_DEFAULT_RUNLEVEL "default"
#define SV_SYSINIT_RUNLEVEL "sysinit"
#define SV_SYSBOOT_RUNLEVEL "sysboot"
#define SV_REBOOT_RUNLEVEL "reboot"
#define SV_NONETWORK_RUNLEVEL "nonetwork"
/* !!! order matter (defined constant/enumeration) !!! */
static const char *const restrict SV_INIT_LEVEL[] = {
	SV_SHUTDOWN_RUNLEVEL,
	SV_SINGLE_RUNLEVEL,
	SV_NONETWORK_RUNLEVEL,
	SV_DEFAULT_RUNLEVEL,
	SV_SYSINIT_RUNLEVEL,
	SV_SYSBOOT_RUNLEVEL,
	SV_REBOOT_RUNLEVEL,
	NULL
};

#define SV_SHUTDOWN_SH SYSCONFDIR "/sv.shutdown"
static const char **restrict SV_INIT_SH = (const char *restrict []) {
	SYSCONFDIR "/sv.init",
	LIBDIR "/sv/sh/sv-init.sh",
	EXEC_PREFIX "/sbin/sv-rc",
	NULL
};

static int runlevel = SV_DEFAULT_LEVEL;
static int level;

static const char *shortopts = "dfhsv";
static const struct option longopts[] = {
	{ "debug",    0, NULL, 'd' },
	{ "fast",     0, NULL, 'f' },
	{ "single",   0, NULL, 's' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Enable debug mode",
	"Enable fast boot",
	"Enable single-user",
	"Print help message",
	"Print version string",
	NULL
};

__attribute__((__noreturn__)) static void help_message(int status)
{
	int i;
	printf("Usage: %s [-d|--debug] [0|1|6]\n", progname);
	for ( i = 0; longopts_help[i]; i++)
		printf("     -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);
	exit(status);
}

static void signal_handler(int sig)
{
	static int sigsys;
	static int sigsegv;
	int serrno = errno;

	switch (sig) {
	case SIGSYS:
		/* allow 25 SIGSYS signal */
		if (sigsys < 25) {
			sigsys++;
			return;
		}
		err_syslog(LOG_ERR, "caught 25 SIGSYS signal!");
		closelog();
		exit(EXIT_FAILURE);
	case SIGSEGV:
		/* allow 25 SIGSEGV signal */
		if (sigsegv < 25) {
			sigsegv++;
			return;
		}
		err_syslog(LOG_ERR, "caught 25 SIGSEGV signal!");
		closelog();
		exit(EXIT_FAILURE);
	case SIGCHLD:
		break;
	case SIGUSR1:
	case SIGUSR2:
		runlevel = SV_SHUTDOWN_LEVEL;
		break;
	case SIGINT:
		runlevel = SV_REBOOT_LEVEL;
		break;
	case SIGTERM:
		runlevel = SV_SINGLE_LEVEL;
		break;
	case SIGHUP:
		/*if (ERR_debug)
			ERR_debug--;
		else
			ERR_debug++;*/
		break;
	case SIGSTOP:
		break;
	case SIGALRM:
		timeout = 1;
		break;
	default:
		err_syslog(LOG_DEBUG, "caught unhandled `%s' signal", strsignal(sig));
		break;
	}
	errno = serrno;
}

#define OPEN_CONSOLE(fd)                                                  \
	if ((fd = open(_PATH_CONSOLE, O_APPEND | O_RDWR | O_NONBLOCK)) > 0) { \
		(void)fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);       \
		if (fd != STDIN_FILENO)                                           \
		dup2(fd, STDIN_FILENO);                                           \
		dup2(fd, STDOUT_FILENO);                                          \
		dup2(fd, STDERR_FILENO);                                          \
		if (fd > STDERR_FILENO) (void)close(fd);                          \
	}                                                                     \
	else err_syslog(LOG_ERR, "Failed to open `%s'!", _PATH_CONSOLE);

int main(int argc, char *argv[])
{
	int i;
	int fd;
	int status;
	int single_emergency = 0;
	int *sig = (int []) { SIGABRT,  SIGFPE, SIGILL, SIGSEGV,
			SIGBUS, SIGXCPU, SIGXFSZ, SIGHUP, SIGINT, SIGTERM, SIGSTOP,
			SIGUSR1, SIGUSR2, SIGALRM, 0 };
	sigset_t mask;
	struct sigaction action;
	char ARG[128], *ARGV[8];
	pid_t child, pid = getpid();
	time_t level_time;
	struct utmpx ut;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = *argv;
	else
		progname++;
	ERR_syslog++;
	openlog(progname, LOG_CONS | LOG_ODELAY, LOG_AUTH);

	if (setsid() == -1)
		err_syslog(LOG_ERR, "Failed to setsid(): %s", strerror(errno));
	if (getuid()) {
		errno = EPERM;
		err_syslog(LOG_ERR, "Permission denied for UID=%d: %s", getuid(), strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* send requested signal to pid=1 */
	if (pid != 1) {
		switch(**argv) {
		case '0':
			kill(1, SIGUSR2);
			break;
		case '1':
			kill(1, SIGTERM);
			break;
		case '6':
			kill(1, SIGINT);
			break;
		default:
			goto getopts;
		}
		exit(EXIT_SUCCESS);
	}

getopts:
	while ((i = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
	{
		switch(i) {
		case 'd':
			ERR_debug++;
			break;
		case 'f': /*IGNORED*/
			break;
		case 's':
			runlevel = SV_SINGLE_LEVEL;
			break;
		case 'h':
			if (pid != 1)
				help_message(EXIT_SUCCESS);
			break;
		case 'v':
			if (pid != 1) {
				printf(SV_COPYRIGHT);
				printf("%s version %s\n", progname, VERSION);
				return EXIT_SUCCESS;
			}
			break;
		case '?':
			if (pid != 1)
				return EXIT_FAILURE;
			break;
		default:
			if (pid != 1) {
				ERR("unkown option -- `%c'", *argv[optind]);
				help_message(EXIT_FAILURE);
			}
			else
				err_syslog(LOG_WARNING, "ignoring invalid option -- `%c'", i);
			break;
		}
	}
	if (pid != 1) return EXIT_FAILURE;
	argc -= optind; argv += optind;
	/* single runlevel can be requested on the command line */
	if (argc) {
		if (**argv == '1') runlevel = SV_SINGLE_LEVEL;
		argc++, argv++;
	}
	if (argc) err_syslog(LOG_WARNING, "ignoring excess argument(s)");

	/* set up signal handler */
	memset(&action, 0, sizeof(action));
	sigfillset(&mask);
	memcpy(&action.sa_mask, &mask, sizeof(mask));
	action.sa_handler = signal_handler;
	action.sa_flags = SA_RESTART;
	for ( ; *sig; sig++) {
		sigdelset(&mask, *sig);
		if (*sig == SIGCHLD) action.sa_flags |= SA_NOCLDSTOP;
		if (sigaction(*sig, &action, NULL))
			err_syslog(LOG_ERR, "Failed to register sigaction(%d, ...): %s",
					*sig, strerror(errno));
	}
	/* set up the signal mask */
	if (sigprocmask(SIG_SETMASK, &mask, NULL))
		err_syslog(LOG_ERR, "Failed to setup signal mask: %s", strerror(errno));

	memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = SIG_IGN;
	(void)sigaction(SIGTTIN, &action, NULL);
	(void)sigaction(SIGTTOU, &action, NULL);

#ifdef RB_DISABLE_CAD
	/* handle C[ontrol]-A[lt]-D[el] C-A-D keystroke */
	if (RB_DISABLE_CAD == 0) reboot(0);
#endif

	/* open console */
	OPEN_CONSOLE(fd);
	err_syslog(LOG_INFO, "%s v%s", progname, VERSION);

	/* check the init-system binary to use */
	for ( ; *SV_INIT_SH; SV_INIT_SH++)
		if (!access(*SV_INIT_SH, R_OK | X_OK))
			break;
	if (!*SV_INIT_SH) {
		err_syslog(LOG_ERR, "\007*** NO INIT-SYSTEM BINAY FOUND!!! \007***");
		return EXIT_FAILURE;
	}

	/* runlevel */
	memcpy(ARG, *SV_INIT_SH, strlen(*SV_INIT_SH)+1LU);
	ARGV[0] = strrchr(ARG, '/')+1LU;
	ARGV[2] = NULL;
	memset(&ut, 0, sizeof(ut));
	memcpy(ut.ut_id, "~", 2);

	for (;;) {
		level_time = time(NULL);
		if (level)
			level = runlevel;
		else
			level = SV_SYSINIT_LEVEL;
		ARGV[1] =  SV_INIT_LEVEL[level];

		do {
			pid = fork();
			if (pid == -1 && errno != EINTR) {
				err_syslog(LOG_DEBUG, "Failed to fork()");
				while ((child = waitpid(-1, &status, WNOHANG)) != -1) ;
				continue;
			}
		} while (pid == -1);

		/* child */
		if (!pid) {
			openlog(progname, LOG_CONS | LOG_ODELAY, LOG_AUTH);

			switch (level) {
			case SV_DEFAULT_LEVEL:
				OPEN_CONSOLE(fd);
#ifdef TIOCSCTTY
				if (fd != -1) ioctl(STDIN_FILENO, TIOCSCTTY, NULL);
#endif
			case SV_SINGLE_LEVEL:
				if (single_emergency) {
					memcpy(ARG, "/bin/login", 11LU);
					memcpy(ARG+16LU, "root", 5LU);
					ARGV[0] = ARG+5LU;
					ARGV[1] = ARG+16LU;
				}
			case SV_SYSBOOT_LEVEL:
				setsid();
				break;
			case SV_SHUTDOWN_LEVEL:
			case SV_REBOOT_LEVEL:
				if (!access(SV_SHUTDOWN_SH, R_OK | X_OK)) {
					memcpy(ARG, SV_SHUTDOWN_SH, strlen(SV_SHUTDOWN_SH)+1LU);
					ARGV[0] = strrchr(ARG, '/')+1LU;
				}
				break;
			}

			(void)sigaction(SIGTSTP, &action, NULL);
			(void)sigaction(SIGHUP , &action, NULL);
			if (sigprocmask(SIG_SETMASK, &action.sa_mask, NULL))
				err_syslog(LOG_ERR, "Failed to setup signal mask: %s", strerror(errno));

			closelog();
			execv(ARG, ARGV);
			ERROR("Failed to `execv(%s,..)'", *ARGV);
			_exit(EXIT_SUCCESS);
		}

		/* parent */
		switch (level) {
		case SV_REBOOT_LEVEL:
		case SV_SHUTDOWN_LEVEL:
			if (strcmp(ARG, *SV_INIT_SH)) {
				memcpy(ARG, *SV_INIT_SH, strlen(*SV_INIT_SH)+1LU);
				ARGV[0] = strrchr(ARG, '/')+1LU;
			}
			timeout = 0;
			sleep(SHUTDOWN_TIMEOUT);
			break;
		}

collect_child:
		do {
			/* WAIT CHILD */
			child = waitpid(-1, &status, WNOHANG);
			if (child == -1) {
				if (errno == ECHILD) sigsuspend(&mask);
				if (errno == EINTR )
					if (timeout && ((level == SV_REBOOT_LEVEL) || (level == SV_SHUTDOWN_LEVEL))) {
						kill(pid, SIGTERM);
						err_syslog(LOG_WARNING, "killed `%s', timeout expired!", *ARGV);
						runlevel = SV_SINGLE_LEVEL;
						break;
					}
				/* do not interrupt any runlevel but default */
				if ((runlevel != level) && (level == SV_DEFAULT_LEVEL)) break;
				continue;
			}
			/* HANDLE CHILD RETURN VALUE */
			if (child != pid) continue;
			if (WIFSTOPPED(status)) {
				kill(pid, SIGCONT);
				continue;
			}
			if (WIFCONTINUED(status)) continue;
			if (WIFEXITED(status)) {
				if (WIFSIGNALED(status))
					err_syslog(LOG_WARNING, "%s signaled [%s]", *ARGV, strsignal(WTERMSIG(status)));
				/* FAILURE */
				if (WEXITSTATUS(status)) {
					err_syslog(LOG_WARNING, "%s failed [%d]", *ARGV, WEXITSTATUS(status));
					runlevel = SV_SINGLE_LEVEL;
					if (level == SV_SINGLE_LEVEL) {
						err_syslog(LOG_EMERG, "cannot switch to single user level");
						if (single_emergency) return EXIT_FAILURE;
						err_syslog(LOG_NOTICE, "launching a login shell");
						single_emergency++;
					}
					continue;
				}
				/* SUCCESS */
				else {
					switch (level) {
					case SV_SYSINIT_LEVEL:
					case SV_SYSBOOT_LEVEL:
						break;
					case SV_DEFAULT_LEVEL:
						ut.ut_type = BOOT_TIME;
					case SV_REBOOT_LEVEL:
					case SV_SHUTDOWN_LEVEL:
						ut.ut_type = INIT_PROCESS;
					default:
						/* write utmp record */
						strncpy(ut.ut_user, SV_INIT_LEVEL[level], sizeof(ut.ut_user));
						ut.ut_tv.tv_sec = level_time;
						if (pututxline((const struct utmpx*)&ut))
							err_syslog(LOG_WARNING, "cannot add utmpx record for %s", ut.ut_user);
					}
					if (level == SV_DEFAULT_LEVEL) continue;
				}
			}
		} while (pid != child);

		/* allow to interrupt {default,shutdown,reboot} runlevel */
		if (level != runlevel) continue;

		/* system shutdown or reboot */
		switch (level) {
		case SV_REBOOT_LEVEL:
		case SV_SHUTDOWN_LEVEL:
			sleep(0U);
			break;
		/* multi-user level */
		case SV_SYSINIT_LEVEL:
		case SV_SYSBOOT_LEVEL:
		case SV_SINGLE_LEVEL:
			runlevel = SV_DEFAULT_LEVEL;
			continue;
		/* SAFETY NET */
		default: goto collect_child;
		}

		OPEN_CONSOLE(fd);

		err_syslog(LOG_WARNING, "Sending SIGTERM to processes...");
		kill(-1, SIGTERM);
		timeout = 0;
		sleep(DEATH_TIMEOUT);
		do {
			child = waitpid(-1, &status, WNOHANG);
			if (child == -1) {
				if (errno == ECHILD) break;
				if (errno == ESRCH ) break;
				if (errno == EINTR )
					if (timeout) break;
				continue;
			}
		} while (child != -1);
		sleep(0U);

		err_syslog(LOG_WARNING, "Sending SIGKILL to processes...");
		kill(-1, SIGKILL);
		err_syslog(LOG_INFO, "system %s", SV_INIT_LEVEL[level]);
		exit(1);

#ifdef RB_AUTOBOOT
		if (level == SV_REBOOT_LEVEL)
			reboot(RB_AUTOBOOT);
#endif
#ifdef RB_POWER_OFF
		err_syslog(LOG_INFO, "system poweroff");
		reboot(RB_POWER_OFF);
#endif
#ifdef RB_POWEROFF
		reboot(RB_POWEROFF);
#endif
		err_syslog(LOG_INFO, "system halt");
#ifdef RB_HALT_SYSTEM
		reboot(RB_HALT_SYSTEM);
#endif
#ifdef RB_HALT
		reboot(RB_HALT);
#endif
		while ((child = waitpid(-1, &status, WNOHANG)) == -1) ;
		return EXIT_FAILURE;
	}
}

#undef OPEN_CONSOLE
