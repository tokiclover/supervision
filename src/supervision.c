/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)supervision.c  0.15.0 2019/02/14
 */

#include <dirent.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include "config.h"
#include "error.h"
#include "sv-copyright.h"

#define SV_VERSION "0.15.0"
#define SV_PIDFILE ".tmp/supervision.pid"
#define SV_FIFO    ".tmp/supervision.ctl"
#define SVD EXEC_PREFIX PREFIX "/bin/svd"
#define SV_TIMEOUT 5LU
#define SV_ALLOC(siz) if ((siz) >= SV_SIZ) {                                   \
	SV_SIZ += (siz);                                                           \
	SV_ENT = err_realloc(SV_ENT, SV_SIZ*sizeof(struct svent));                 \
	memset(SV_ENT+(SV_SIZ-(siz))*sizeof(struct svent), 0, (siz)*sizeof(struct svent)); \
}

struct svent {
	ino_t se_ino;
	dev_t se_dev;
	pid_t se_pid;
	char *se_nam;
};

static void sv_sigaction(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)));
__attribute__((format(printf,2,3))) void err_syslog(int priority, const char *fmt, ...);
__attribute__((__unused__)) static int svd(pid_t pid, char *svc);
__attribute__((__unused__)) static pid_t collect_child(pid_t pid);

const char *progname;
static char *SV_DIR;
static int fd, df;
static off_t off;
static int scan_dir;
static pid_t scan_child;
static time_t time_old, scan_time;
static int log_err;
static struct svent *SV_ENT;
static size_t SV_SIZ;
static sigset_t mask;

static const char *shortopts = "dhsv";
static const struct option longopts[] = {
	{ "daemon" , 0, NULL, 'd' },
	{ "sid"    , 0, NULL, 's' },
	{ "help"   , 0, NULL, 'h' },
	{ "version", 0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Daemonize to the background",
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

__attribute__((format(printf,2,3))) void err_syslog(int priority, const char *fmt, ...)
{
	va_list ap;

	if (log_err) {
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
		fprintf(stderr, "%s: debug: ", progname);
		break;
	}
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

static void sv_sigaction(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)))
{
	int child, status;
	int serrno = errno;

	switch (sig) {
	case SIGCHLD:
		switch (si->si_signo) {
		case SIGSTOP:
			kill(si->si_pid, SIGCONT);
			break;
		case SIGABRT:
		case SIGSEGV:
		case SIGBUS :
		case SIGHUP :
		case SIGCONT:
			break;
		default: 
			(void)collect_child(si->si_pid);
			break;
		}
		break;
	case SIGALRM:
	case SIGUSR1:
	case SIGUSR2:
		time_old = scan_time;
		scan_time = time(NULL);
		if ((scan_time - time_old) > SV_TIMEOUT)
			scan_dir++;
		break;
	case SIGINT:
		kill(0, sig);
		exit(EXIT_FAILURE);
	case SIGTERM:
		kill(0, sig);
		while ((child = waitpid(0, &status, 0)) != -1) ;
		exit(EXIT_FAILURE);
		break;
	case SIGQUIT:
		kill(0, sig);
		while ((child = waitpid(0, &status, WNOHANG)) != -1) ;
		exit(EXIT_FAILURE);
		break;
	case SIGHUP:
		scan_time = time(NULL);
		scan_child = 0;
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
	int child, status;

	while ((child = waitpid(pid, &status, WNOHANG)) != -1) {
		for (no = 0LU; no < off; no++) {
			if (SV_ENT[no].se_pid == pid) {
				SV_ENT[no].se_pid = 0;
				if (WIFEXITED(status)) {
					/* FAILURE */
					if (WEXITSTATUS(status))
						scan_child = pid;
					/* SUCCESS */
					else {
						SV_ENT[no].se_ino = 0;
						free((void*)SV_ENT[no].se_nam);
						SV_ENT[no].se_nam = NULL;
					}
				}
				return child;
			}
		}
	}
	return 0;
}
__attribute__((__unused__)) static int svd(pid_t pid, char *svc)
{
	off_t no;
	struct stat st;
	static char *a = strrchr(SVD, '/');
	char *argv[4], p[512];

	memset(&st, 0, sizeof(st));
	if (lstat(svc, &st)) {
		err_syslog(LOG_ERR, "Failed to stat `%s'", svc);
		return -ENOENT;
	}
	if (!S_ISDIR(st.st_mode & S_IFMT)) return -EINVAL;
	
	/* structure recycling */
	if (pid)
		for (no = 0LU; no < off; no++)
			if (SV_ENT[no].se_pid == pid) break;
	else
		for (no = 0LU; no < off; no++)
			if (!SV_ENT[no].se_ino) break;

	if (no == off) {
		SV_ENT[no].se_ino = st.st_ino;
		SV_ENT[no].se_dev = st.st_dev;
		SV_ENT[no].se_nam = err_strdup(svc);
	}

	argv[0] = SVD; argv[1] = p; argv[2] = NULL;
	snprintf(p, sizeof(p), "%s/%s", SV_DIR, svc);

	do {
		SV_ENT[off].se_pid = fork();
		if (SV_ENT[off].se_pid == -1 && errno != EINTR) {
			err_syslog(LOG_ERR, "Failed to fork()");
			while (!collect_child(0));
		}
	} while (SV_ENT[off].se_pid == -1);

	if (SV_ENT[off].se_pid) { /* parent */
		off++;
		SV_ALLOC(off);
		return 0;
	}

	execv(a, argv); /* child */
	ERROR("Failed to `execv(%s,...)'", a);
}

int main(int argc, char *argv[])
{
	off_t no;
	int sid = 0;
	int pf, rv;
	DIR *dp;
	struct dirent *de;
	int *sig = (int []) { SIGHUP, SIGINT, SIGTERM, SIGQUIT, SIGUSR1, SIGUSR2, SIGALRM, 0 };
	char bf[256];
	struct sigaction action;
	struct stat st;

	progname = strrchr(argv[0], '/');
	if (!progname)
		progname = argv[0];
	else
		progname++;

	if (argc < 1)
		help_message(1);

	/* Parse options */
	while ((pf = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (pf) {
			case 'd':
				log_err++;
				break;
			case 's':
				sid++;
				break;
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
	SV_DIR = *argv;

	if (chdir(*argv)) ERROR("cannot change current directory to `%s'", *argv);

	/* set up signal handler */
	memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_sigaction = sv_sigaction;
	action.sa_flags = SA_SIGINFO | SA_RESTART;
	for ( ; *sig; off++, sig++) {
		if (sigaction(*sig, &action, NULL))
			err_syslog(LOG_ERR, "Failed to register `%d' signal: %s",
					*sig, strerror(errno));
	}

	if (!(dp = opendir(*argv)))
		ERROR("Failed to open `%s' directory", *argv);
	if (!(df = dirfd(dp)))
		ERROR("Failed to get `%s' directory description", *argv);
	if (mkdirat(df, ".tmp", 0755) && errno != EEXIST)
		ERROR("Failed to create `%s' directory", ".tmp");
	if (!(fd = open(SV_PIDFILE, O_CREAT | O_WRONLY, 0644)))
		ERROR("Failed to open `%s'", SV_PIDFILE);
	sprintf(bf, "%d", getpid());
	if (err_write(fd, bf, SV_PIDFILE)) exit(EXIT_FAILURE);
	close(fd);

	(void)unlink(SV_FIFO);
	if (mkfifoat(df, SV_FIFO, 0660))
		ERROR("Failed to make `%s' fifo", SV_FIFO);
	if ((pf = openat(df, SV_FIFO, O_RDONLY | O_NOCTTY | O_NONBLOCK)) == -1)
		ERROR("Failed to open `%s'", SV_FIFO);

	if (log_err) {
		do {
			pf = fork();
			if (pf == -1) {
				if (errno == EINTR) continue;
				ERROR("cannot fork the process", NULL);
			}
		} while(pf == -1);
		if (pf) _exit(EXIT_SUCCESS);
		openlog(progname, LOG_CONS | LOG_ODELAY | LOG_PID, LOG_DAEMON);
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}
	if (sid) {
		if (setsid() == -1) {
			err_syslog(LOG_ERR, "cannot start a new session: %s\n", strerror(errno));
			if (getpid() != getpgrp()) setpgrp();
		}
	}

	SV_ALLOC(32LU);
	off = 0LU;

	for (;;) {
		scan_dir = 0;
		while ((de = readdir(dp))) {
			if (*de->d_name == '.') continue;
#ifdef _DIRENT_HAVE_D_TYPE
			switch (de->d_type) {
			case DT_DIR:
			case DT_LNK:
				(void)svd(0, de->d_name);
				break;
			default: continue;
			}
#else
			(void)svd(0, de->d_name);
#endif
		}

		do {
			if (scan_dir) break;
			if (scan_child) {
				for (no = 0LU; no < off; no++)
					if (scan_child == SV_ENT[off].se_pid) {
						(void)svd(scan_child, SV_ENT[no].se_nam);
						scan_child = 0;
					}
				break;
			}
			rv = read(pf, bf, sizeof(bf));
			if (rv == -1) {
				if (errno == EINTR) {
					if (scan_dir) break;
					if (scan_child) break;
				}
				if (errno == EAGAIN) {
					/*sigsuspend(&mask);*/
					sleep(SV_TIMEOUT);
					time_old = scan_time;
					scan_time = time(NULL);
					if (scan_dir) continue;

					memset(&st, 0, sizeof(st));
					if (lstat(*argv, &st)) {
						err_syslog(LOG_ERR, "Failed to stat `%s'", *argv);
						continue;
					}
					if (st.st_mtim.tv_sec > time_old) {
						scan_dir++;
						break;
					}
				}
				continue;
			}
			if (rv) (void)svd(0, bf);
		} while (!scan_dir || !scan_child);

		rewinddir(dp);
	}

	return EXIT_FAILURE;
}
