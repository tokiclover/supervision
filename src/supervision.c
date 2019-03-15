/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)supervision.c  0.15.0 2019/03/14
 */

#include <dirent.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>
#include <fcntl.h>
#include "config.h"
#include "error.h"
#include "sv-copyright.h"
#include "supervision.h"
#include "timespec.h"

#define SV_VERSION "0.15.0"
#ifndef SV_SIGSYS_MAX
# define SV_SIGSYS_MAX 100
#endif
#ifndef SV_PIDFILE
# define SV_PIDFILE ".tmp/supervision.pid"
#endif
#ifndef SV_FIFO
# define SV_FIFO    ".tmp/supervision.ctl"
#endif
#ifndef SVD
# define SVD EXEC_PREFIX "/bin/svd"
#endif
#define SV_TIMEOUT 5LU

#define SV_ALLOC(siz) if ((siz) >= SV_SIZ) {                                   \
	SV_SIZ += (siz);                                                           \
	SV_ENT = err_realloc(SV_ENT, SV_SIZ*sizeof(struct svent));                 \
	memset(SV_ENT+(SV_SIZ-(siz))*sizeof(struct svent), 0, (siz)*sizeof(struct svent)); \
}

#define COLLECT_CHILD(pid) if (child) (void)collect_child(pid)

struct svent {
	ino_t se_ino;
	dev_t se_dev;
	pid_t se_pid;
	char *se_arg, *se_nam;
};

static void sv_sigaction(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)));
__attribute__((__unused__)) static int svd(long int off, char *svc, int flag);
__attribute__((__unused__)) static pid_t collect_child(pid_t pid);

extern char **environ;
const char *progname;
static int fd, df;
static int scan;
static pid_t child;
static int sid;
static struct timespec ts_old, ts_scan;
static struct svent *SV_ENT;
static size_t SV_SIZ;
static off_t SV_OFF;

static const char *shortopts = "dlhsv";
static const struct option longopts[] = {
	{ "debug"  , 0, NULL, 'd' },
	{ "syslog" , 0, NULL, 'l' },
	{ "sid"    , 0, NULL, 's' },
	{ "help"   , 0, NULL, 'h' },
	{ "version", 0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Enable debug mode",
	"Log events to system logger",
	"Set session identity",
	"Print help message",
	"Print version string",
	NULL
};

__attribute__((__noreturn__)) static void help_message(int retval)
{
	int i;
	printf("Usage: %s [OPTIONS] DIR\n", progname);
	for (i = 0; longopts_help[i]; i++)
		printf("    -%c, --%-14s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);
	exit(retval);
}

static void sv_sigaction(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)))
{
	int serrno = errno;
	static int sigsys;

	switch (sig)
	{
	case SIGCHLD:
		switch (si->si_signo)
		{
		case SIGSTOP:
			kill(si->si_pid, SIGCONT);
			break;
		case SIGABRT:
		case SIGSEGV:
		case SIGBUS :
		case SIGHUP :
		case SIGCONT:
		case SIGILL :
			err_syslog(LOG_DEBUG, "cannot supervise broken `%s' binary\n", SVD);
			sigsys++;
			if (sigsys > SV_SIGSYS_MAX) exit(EXIT_FAILURE);
		default: 
			child = si->si_pid;
			break;
		}
		break;
	case SIGALRM:
		break;
	case SIGUSR1:
		sid++;
	case SIGUSR2:
		break;
	case SIGINT:
		kill(0, sig);
		exit(EXIT_FAILURE);
	case SIGTERM:
		kill(0, sig);
		while (waitpid(0, NULL, 0) != -1) ;
		exit(EXIT_FAILURE);
		break;
	case SIGQUIT:
		kill(0, sig);
		while (waitpid(0, NULL, WNOHANG | WUNTRACED | WCONTINUED) != -1) ;
		exit(EXIT_FAILURE);
		break;
	case SIGHUP:
		scan++;
		break;
	default:
		err_syslog(LOG_DEBUG, "caught unhandled `%s' signal", strsignal(sig));
		break;
	}
	errno = serrno;
}

__attribute__((__unused__)) static pid_t collect_child(pid_t pid)
{
	off_t no;
	int rc, st;

	while ((rc = waitpid(pid, &st, WNOHANG | WUNTRACED | WCONTINUED)) != -1)
	{
		for (no = 0LU; no < SV_OFF; no++)
		{
			if (SV_ENT[no].se_pid == (pid ? pid : rc)) {
				SV_ENT[no].se_pid = 0;
				if (WIFEXITED(st)) {
					/* FAILURE */
					if (WEXITSTATUS(st))
						(void)svd(no, SV_ENT[no].se_nam, 0);
					/* SUCCESS */
					else {
						SV_ENT[no].se_ino = 0;
						free((void*)SV_ENT[no].se_nam);
						SV_ENT[no].se_arg = SV_ENT[no].se_nam = NULL;
					}
					if (pid) return pid;
				}
			}
		}
	}
	child = 0;
	return 0;
}

__attribute__((__unused__)) static int svd(long int off, char *svc, int flag)
{
	off_t no;
	static char *arg [2] = { "-s", "--sid" };
	static char *argv[4] = { SVD, [3] = NULL };
	char *p;
	struct stat st;

	if ((p = strchr(svc, ';'))) {
		*p++ = '\0';
		if (!strcmp(p, arg[0]) || !strcmp(p, arg[1])) sid++;
	}

	memset(&st, 0, sizeof(st));
	if (lstat(svc, &st)) {
		err_syslog(LOG_ERR, "Failed to stat `%s'", svc);
		return -ENOENT;
	}
	if (!S_ISDIR(st.st_mode)) return -EINVAL;
	
	if (off < 0) {
		for (no = 0LU; no < SV_OFF; no++)
			if (!SV_ENT[no].se_ino) {
				break;
			}
		if (no == SV_OFF) SV_OFF += 8LU;
		SV_ALLOC(SV_OFF);
		SV_ENT[no].se_ino = st.st_ino;
		SV_ENT[no].se_dev = st.st_dev;
		SV_ENT[no].se_nam = err_strdup(svc);
	}
	else no = off;

	if (sid > 0) {
		sid--;
		SV_ENT[no].se_arg = *arg;
	}
	if (SV_ENT[no].se_arg) {
		argv[2] = svc; argv[1] = SV_ENT[no].se_arg;
	}
	else {
		argv[1] = svc; argv[2] = NULL;
	}

	do {
		SV_ENT[no].se_pid = fork();
		if (SV_ENT[no].se_pid == -1 && errno != EINTR) {
			err_syslog(LOG_ERR, "%s: cannot fork", __func__);
			while (!collect_child(0)) ;
		}
	} while (SV_ENT[no].se_pid == -1);

	if (SV_ENT[no].se_pid) { /* parent */
		return 0;
	}

	execve(*argv, argv, environ); /* child */
	ERROR("Failed to `execve(%s,...)'", *argv);
}

static void sv_clean(void)
{
	(void)unlink(SV_PIDFILE);
	(void)unlink(SV_FIFO);
}

int main(int argc, char *argv[])
{
	int sid = 0;
	int rv;
	dev_t sd_dev;
	ino_t sd_ino;
	struct timespec sd_tim;
	DIR *dp;
	FILE *fp;
	struct dirent *de;
	int *sig = (int []) { SIGHUP, SIGINT, SIGTERM, SIGQUIT, SIGUSR1, SIGUSR2, SIGALRM, 0 };
	char bf[512];
	struct sigaction action;
	sigset_t mask;
	struct stat st;
	struct pollfd pfd[2];

	progname = strrchr(argv[0], '/');
	if (!progname)
		progname = argv[0];
	else
		progname++;

	if (argc < 1)
		help_message(1);

	/* Parse options */
	while ((rv = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
	{
		switch (rv)
		{
			case 'd': ERR_debug++ ; break;
			case 'l': ERR_syslog++; break;
			case 's': sid++       ; break;
			case 'v':
				printf("%s version %s\n\n", progname, SV_VERSION);
				puts(SV_COPYRIGHT);
				exit(EXIT_SUCCESS);
			case 'h':
				help_message(0);
				break;
			default:
				help_message(1);
				break;
		}
	}
	argc -= optind, argv += optind;
	if (argc < 1) {
		ERR("missing `DIR' argument\n", NULL);
		exit(EXIT_FAILURE);
	}

	memset(&st, 0, sizeof(st));
	if (lstat(*argv, &st)) ERROR("Failed to stat `%s'", *argv);
	if (chdir(*argv)) ERROR("cannot change current directory to `%s'", *argv);

	/* set up signal handler */
	memset(&action, 0, sizeof(action));
	sigemptyset(&mask);
	sigemptyset(&action.sa_mask);
	action.sa_sigaction = sv_sigaction;
	action.sa_flags = SA_SIGINFO | SA_RESTART;
	for ( ; *sig; sig++)
	{
		sigaddset(&mask, *sig);
		if (sigaction(*sig, &action, NULL))
			ERROR("Failed to register `%d' signal!", *sig);
	}
	/* set up the signal mask */
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL))
		ERROR("Failed to setup signal mask!", NULL);

	if (!(dp = opendir(*argv)))
		ERROR("Failed to open `%s' directory", *argv);
	if (!(df = dirfd(dp)))
		ERROR("Failed to get `%s' directory description", *argv);
	if (mkdirat(df, ".tmp", 0755) && errno != EEXIST)
		ERROR("Failed to create `%s' directory", ".tmp");

	if ((fd = openat(df, SV_PIDFILE, O_RDONLY)) != -1) {
		fp = fdopen(fd, "r");
		if (fscanf(fp, "%d", &rv)) {
			if (!kill(rv, 0)) {
				ERR("pid=%d} is already running!\n", rv);
				exit(EXIT_SUCCESS);
			}
		}
		close(fd);
	}
	atexit(sv_clean);
	if (!(fd = openat(df, SV_PIDFILE, O_CREAT | O_WRONLY, 0644)))
		ERROR("Failed to open `%s'", SV_PIDFILE);
#ifdef HAVE_FLOCK
	if ((rv = flock(fd, LOCK_EX | LOCK_NB)))
#else
	if ((rv = lockf(fd, F_LOCK | F_TLOCK, sizeof(pid_t))))
#endif
		ERR("Failed to lock `%s'", SV_PIDFILE);
	sprintf(bf, "%d", getpid());
	if (err_write(fd, bf, SV_PIDFILE)) exit(EXIT_FAILURE);
	/*close(fd);*/

	(void)unlink(SV_FIFO);
	if (mkfifoat(df, SV_FIFO, 0600))
		ERROR("Failed to make `%s' fifo", SV_FIFO);
	if ((fd = openat(df, SV_FIFO, O_RDONLY | O_NDELAY | O_NONBLOCK | O_CLOEXEC)) == -1)
		ERROR("Failed to open `%s' [read]", SV_FIFO);
	if (openat(df, SV_FIFO, O_WRONLY | O_NDELAY | O_NONBLOCK | O_CLOEXEC) == -1)
		ERROR("Failed to open `%s' [write]", SV_FIFO);

	if (ERR_syslog) openlog(progname, LOG_CONS | LOG_ODELAY | LOG_PID, LOG_DAEMON);
	if (sid) {
		if (setsid() == -1) {
			err_syslog(LOG_ERR, "cannot start a new session: %s\n", strerror(errno));
			if (getpid() != getpgrp()) setpgrp();
		}
	}

	pfd[0].fd = fd;
	pfd[0].events = POLLIN;

	TIMESPEC(&ts_scan);
	for (;;)
	{
		ts_old = ts_scan;
		if (!timespec_cmp(&sd_tim, &st.st_mtim)) {
			while (lstat(".", &st) == -1)
			{
				if (errno != EINTR)
					err_syslog(LOG_DEBUG, "cannot stat `%s': %s", *argv,
							strerror(errno));
				else COLLECT_CHILD(0);
			}
		}
		TIMESPEC(&ts_scan);
		sd_ino = st.st_ino;
		sd_dev = st.st_dev;
		sd_tim = st.st_mtim;

		while ((de = readdir(dp)))
		{
			if (*de->d_name == '.') continue;
#ifdef _DIRENT_HAVE_D_TYPE
			switch (de->d_type) {
			case DT_DIR:
			case DT_LNK:
				(void)svd(-1, de->d_name, 0);
				break;
			default: continue;
			}
#else
			(void)svd(-1, de->d_name, 0);
#endif
		}
		scan = 0;

		do {
			COLLECT_CHILD(0);
			do {
				rv = poll(pfd, 1LU, SV_TIMEOUT * 1000);
				if (rv == -1) {
					if (errno != EINTR)
						err_syslog(LOG_DEBUG, "cannot poll file descriptors: %s",
								strerror(errno));
					else COLLECT_CHILD(0);
				}
				else if (!rv) {
					memset(&st, 0, sizeof(st));
					while (lstat(".", &st) == -1)
					{
						if (errno != EINTR)
							err_syslog(LOG_ERR, "cannot stat `%s': %s", *argv,
									strerror(errno));
						else COLLECT_CHILD(0);
					}
					if ((st.st_ino != sd_ino) || (st.st_dev != sd_dev) ||
						(timespec_cmp(&sd_tim, &st.st_mtim) < 0))
						scan++;
				}
			} while (!scan && (rv <= 0));
			COLLECT_CHILD(0);

			rv = read(fd, bf, sizeof(bf));
			if (rv == -1) {
				if (errno == EINTR)  COLLECT_CHILD(0);
				if (errno == EAGAIN) break;
				err_syslog(LOG_DEBUG, "cannot read `%s': %s", SV_FIFO,
						strerror(errno));
				break;
			}
			if (rv) (void)svd(-1, bf, 1);
		} while (!scan);

		COLLECT_CHILD(0);
		rewinddir(dp);
	}

	return EXIT_FAILURE;
}
