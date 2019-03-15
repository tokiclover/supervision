/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)svd.c  0.15.0 2019/03/14
 */

#include <dirent.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <time.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include "error.h"
#include "helper.h"
#include "sv-copyright.h"
#include "timespec.h"
#include "svd.h"

#define SV_VERSION "0.15.0"

#ifndef SV_TIMEOUT
# define SV_TIMEOUT 1U
#endif
#ifndef SV_RUN_NUM
# define SV_RUN_NUM 100
#endif
#ifndef SVL
# define SVL EXEC_PREFIX "/bin/svl"
#endif
#define CHECK_CHILD(sd) ((sd)->sd_pid && !kill((sd)->sd_pid, 0))

/*enum {
	SW_DOWN   = 0x00,
#define SW_DOWN SW_DOWN
	SW_PAUSE  = 0x01,
#define SW_PAUSE SW_PAUSE
	SW_TERM   = 0x02,
#define SW_TERM SW_TERM
	SW_RUN    = 0x04,
#define SW_RUN SW_RUN
	SW_FINISH = 0x08,
#define SW_FINISH SW_FINISH
};

static struct sdent {
	int sd_pid, sd_stat, sd_ctrl, sd_want, sd_ret;
	int fd_lock, fd_ctrl, fd_status, sv_run, sv_log;
	void *fp_lock, *fp_ctrl, *fp_status;
	char *sv_pid, *sv_stat, *sv_status, *sv_control, *sv_lock, *sv_ok;
	struct timespec sv_time;
} sde[2] = {
	[0] = {
		.sd_want = SW_RUN,
		.sd_ctrl = SW_RUN,
		.sv_control = "supervise/control",
		.sv_pid     = "supervise/pid",
		.sv_status  = "supervise/status",
		.sv_stat    = "supervise/stat",
		.sv_lock    = "supervise/lock",
		.sv_ok      = "supervise/ok",
	},
	[1] = {
		.sd_ctrl = SW_RUN,
		.sv_control = "log/supervise/control",
		.sv_pid     = "log/supervise/pid",
		.sv_stat    = "log/supervise/stat",
		.sv_status  = "log/supervise/status",
		.sv_lock    = "log/supervise/lock",
		.sv_ok      = "log/supervise/ok",
	}
}, *sd_svc = &sde[0], *sd_log = &sde[1];*/

__attribute__((format(printf,2,3))) void err_syslog(int priority, const char *fmt, ...);
static void svd_sigaction(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)));
__attribute__((__unused__)) static int svd_stat(struct sdent *restrict sd);
static void svd_clean(void);
static void svd_exec(struct sdent *restrict sd);
/* XXX: compatiblity function */
__attribute__((__unused__)) static int svd_usr_ctrl(char c);
__attribute__((__unused__)) static void svd(void);
__attribute__((__unused__)) static int svd_ctrl(struct sdent *restrict sd, char c);
__attribute__((__unused__)) static int collect_child(pid_t pid);

extern char **environ;
const char *progname;
static char *SV_EXEC[2] = { "./run", "./finish" };
static int pipefd[2];
static int count;
static int run, sid;

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
	printf("Usage: %s [-l|--logger] [-s|--sid] SERVICE\n", progname);
	for (i = 0; longopts_help[i]; i++)
		printf("    -%c, --%-14s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);
	exit(retval);
}

/*__attribute__((format(printf,2,3))) void err_syslog(int priority, const char *fmt, ...)
{
	va_list ap;

	if (ERR_syslog) {
		if ((priority == LOG_DEBUG) && !ERR_debug) return;
		va_start(ap, fmt);
		vsyslog(priority, fmt, ap);
		va_end(ap);
		return;
	}

	switch (priority)
	{
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
}*/

__attribute__((__unused__)) static int collect_child(pid_t pid)
{
	int child, status;
	while ((child = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED)) != -1)
		if (WIFEXITED(status)) {
			if (child == sd_svc->sd_pid) {
				sd_svc->sd_pid = 0;
				sd_svc->sd_ret = status;
				if (sd_svc->sd_stat & SW_RUN) sd_svc->sd_want = SW_FINISH;
			}
			else if (child == sd_log->sd_pid) {
				sd_log->sd_pid = 0;
				sd_log->sd_ret = status;
				if (sd_log->sd_stat & SW_RUN) sd_log->sd_want = SW_FINISH;
			}
			return WEXITSTATUS(status);
		}
	return -1;
}

static void svd_sigaction(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)))
{
	int serrno = errno;
	static int sig_count;

	switch (sig)
	{
	case SIGALRM:
		count++;
		if (count >= SV_RUN_NUM)
			exit(EXIT_SUCCESS);
		break;
	case SIGCHLD:
		switch (si->si_signo) {
		case SIGSTOP:
			if (si->si_pid == sd_svc->sd_pid) {
				if (!(sd_svc->sd_ctrl & SW_PAUSE))
					kill(sd_svc->sd_pid, SIGCONT);
			}
			else if (si->si_pid == sd_log->sd_pid) {
				if (!(sd_log->sd_ctrl & SW_PAUSE))
					kill(sd_log->sd_pid, SIGCONT);
			}
			break;
		case SIGABRT:
		case SIGSEGV:
		case SIGBUS :
		case SIGILL :
		case SIGFPE :
			err_syslog(LOG_DEBUG, "child caught fatal `%s' signal", strsignal(sig));
			if (++sig_count > SV_RUN_NUM)
				exit(EXIT_SUCCESS);
			break;
		case SIGCONT:
		case SIGHUP :
		case SIGUSR1:
		case SIGUSR2:
			err_syslog(LOG_DEBUG, "child caught `%s' signal", strsignal(sig));
		default: 
			(void)collect_child(si->si_pid);
			break;
		}
		break;
	case SIGINT:
		kill(0, sig);
		exit(EXIT_FAILURE);
		break;
	case SIGTERM:
		kill(sd_svc->sd_pid, sig);
		if (sd_svc->sd_pid)
			waitpid(sd_svc->sd_pid, NULL, 0);
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
		kill(0, sig);
		break;
	case SIGUSR1:
	case SIGUSR2:
		break;
	default:
		err_syslog(LOG_ERR, "caught unhandled `%s' signal", strsignal(sig));
		exit(EXIT_FAILURE);
		break;
	}
	errno = serrno;
}

/* XXX: compatiblity function */
__attribute__((__unused__)) static int svd_usr_ctrl(char c)
{
	int child, status;
	char *argv[2], control[12] = "control/u";

	control[8] = c;
	if(access(control, R_OK | X_OK)) return -ENOENT;

	do {
		child = fork();
		if (child == -1) {
			if (errno == EINTR) continue;
			err_syslog(LOG_DEBUG, "%s: cannot fork: %s", __func__, strerror(errno));
			return -1;
		}
	} while (child == -1);

	/*parent*/
	if (child) {
		while (waitpid(child, &status, 0) == -1) {
			if (errno == EINTR) continue;
			err_syslog(LOG_DEBUG, "Failed to wait for (pid=%d) child: %s", child,
					strerror(errno));
			return -1;
		}
		return WEXITSTATUS(status);
	}

	/*child*/
	argv[0] = control; argv[1] = NULL;
	if (sd_log->sv_log && *pipefd && CHECK_CHILD(sd_log)) {
		dup2(STDOUT_FILENO, pipefd[1]);
		dup2(STDERR_FILENO, pipefd[1]);
		close(pipefd[0]);
		close(pipefd[1]);
	}
	execve(*argv, argv, environ);
	err_syslog(LOG_ERR, "Failed to execute `%s': %s", control, strerror(errno));
	_exit(EXIT_FAILURE);
}

__attribute__((__unused__)) static int svd_stat(struct sdent *restrict sd)
{
	register unsigned int o, l;
	int fd, rv;
	FILE *fp;

	if (!sd) return -ENOENT;
	sd->sd_stat = sd->sd_want;

	if ((fd = open(sd->sv_pid, O_CREAT | O_WRONLY | O_CLOEXEC | O_TRUNC, 0644)) == -1)
		err_syslog(LOG_ERR, "Failed to open `%s': %s", sd->sv_pid, strerror(errno));
	else {
		fp = fdopen(fd, "w");
		fprintf(fp, "%d\n", sd->sd_pid);
		fflush(fp);
		(void)close(fd);
	}

	if ((fd = open(sd->sv_stat, O_CREAT | O_WRONLY | O_CLOEXEC | O_TRUNC, 0644)) == -1)
		err_syslog(LOG_ERR, "Failed to open `%s': %s", sd->sv_stat, strerror(errno));
	else {
		fp = fdopen(fd, "w");
		if (sd->sd_stat == SW_DOWN)
			fprintf(fp, "down");
		else
			fprintf(fp, "%s", SV_EXEC[run]+2LU);
		if (sd->sd_ctrl & SW_PAUSE)
			fprintf(fp, ", %s", "pause");
		else if (sd->sd_ctrl & SW_TERM)
			fprintf(fp, ", %s", "term");
		else if (sd->sd_ctrl & SW_EXIT)
			fprintf(fp, ", %s", "exit");
		fprintf(fp, "\n");
		fflush(fp);
		(void)close(fd);
	}

	/*supervise(1) compatibility*/
	timespec_pack(st_buf, &sd->sv_time);
	l = o = TIMESPEC_SIZE;
	o += 4LU; /*sizeof(pid_t)*/
	rv = sd->sd_pid;
	while (--o > l) st_buf[o] = rv & 255, rv >>= CHAR_BIT;
	st_buf[o] = rv & 255;

	st_buf[ST_OFF_PAUSE] = sd->sd_ctrl & SW_PAUSE;
	st_buf[ST_OFF_UP   ] = sd->sd_want == SW_RUN ? ST_WANT_UP : ST_WANT_DOWN;
	st_buf[ST_OFF_TERM ] = sd->sd_ctrl & SW_TERM;
	st_buf[ST_OFF_RUN  ] = sd->sd_stat == SW_RUN ? ST_RUN : (sd->sd_stat == SW_FINISH ? ST_FINISH : ST_DOWN);

	if (sd->fd_status == -1) return -1;
	rewind(sd->fp_status);
	l = sizeof(st_buf);
	o = 0LU;
	do {
		rv = write(sd->fd_status, st_buf+o, l);
		if (rv == -1) {
			if (errno == EINTR) continue;
			err_syslog(LOG_ERR, "Failed to write to `%s': %s", sd->sv_status,
					strerror(errno));
			break;
		}
		o += rv; l -= rv;
	} while (l);
	fprintf(sd->fp_status, "%s", st_buf);
	fflush(sd->fp_status);

	return 0;
}

static void svd_exec(struct sdent *restrict sd)
{
	char *argv[4], arg[2][8];
	struct timespec dead;
	if (sd->sd_want < SW_RUN) return;

	TIMESPEC(&dead);
	if ((dead.tv_sec - sd->sv_time.tv_sec) < SV_TIMEOUT)
		sleep(SV_TIMEOUT);

	if (sd->sd_want == SW_RUN) 
		run = 0;
	else
		run = 1;
	if (access(SV_EXEC[run], R_OK | X_OK)) {
		if (run) return;
		err_syslog(LOG_ERR, "%s: executable not found!", SV_EXEC[run]+2LU);
		if (sd == sd_log) return;
		exit(EXIT_SUCCESS);
	}

	argv[0] = SV_EXEC[run]+2LU;
	if (run) {
		argv[1] = arg[0]; argv[2] = arg[1]; argv[3] = NULL;
		snprintf(arg[0], 8LU, "%d", sd->sd_ret);
		snprintf(arg[1], 8LU, "%c", WEXITSTATUS(sd->sd_ret));
	}
	else
		argv[1] = NULL;

	TIMESPEC(&sd->sv_time);
	while ((sd->sd_pid = fork()) == -1)
		err_syslog(LOG_DEBUG, "%s: cannot fork(): %s", __func__, strerror(errno));

	/*parent*/
	if (sd->sd_pid) {
		(void)svd_stat(sd);
		return;
	}
	/*child*/
	if (sid && (sd == sd_svc) && setsid() == -1)
		err_syslog(LOG_ERR, "Failed to start a new session: %s\n", strerror(errno));

	/*logging*/
	if (*pipefd && sd_log->sd_pid) {
		if (sd == sd_svc) {
			dup2(STDOUT_FILENO, pipefd[1]);
			dup2(STDERR_FILENO, pipefd[1]);
		}
		else {
			dup2(STDIN_FILENO, pipefd[0]);
			if (chdir("log")) ERROR("cannot change directory to `%s'", "log");
		}
		close(pipefd[0]);
		close(pipefd[1]);
	}

	execve(SV_EXEC[run], argv, environ);
	err_syslog(LOG_ERR, "Failed to execute `%s': %s\n", SV_EXEC[run]+2LU, strerror(errno));
	_exit(EXIT_SUCCESS);
}

static void svd_clean(void)
{
	int i;
	for (i = 0; i <= sde[1].sv_log; i++) {
		(void)unlink(sde[i].sv_control);
		(void)unlink(sde[i].sv_lock);
		(void)unlink(sde[i].sv_status);
		(void)unlink(sde[i].sv_stat);
		(void)unlink(sde[i].sv_pid);
		/*(void)unlink(sde[i].sv_ok);*/
	}
}

__attribute__((__unused__)) static void svd(void)
{
	if (!access("down", R_OK)) {
		sd_svc->sd_want = SW_DOWN;
		return;
	}

	if (sd_log->sv_log) {
		if (!*pipefd) {
			if (!pipe(pipefd)) {
				fcntl(pipefd[0], F_SETFL, fcntl(pipefd[0], F_GETFL, 0) | O_NONBLOCK);
				fcntl(pipefd[1], F_SETFL, fcntl(pipefd[1], F_GETFL, 0) | O_NONBLOCK | O_APPEND);
			}
			else
				err_syslog(LOG_DEBUG, "Failed to create pipe: %s", strerror(errno));
		}
		if (!CHECK_CHILD(sd_log)) svd_exec(sd_log);
	}

	if (CHECK_CHILD(sd_svc)) return;
	svd_exec(sd_svc);
}

__attribute__((__unused__)) static int svd_ctrl(struct sdent *restrict sd, char c)
{
	int rv = svd_usr_ctrl(c);
	int rc = CHECK_CHILD(sd);

	if (!rc) {
		sd->sd_stat = SW_DOWN;
		sd->sd_pid = 0;
	}

	switch (c)
	{
	case 'u':
		sd->sd_ctrl &= SW_RUN;
		sd->sd_want  = SW_RUN;
		svd_stat(sd);
		if (sd->sd_stat & SW_PAUSE) {
			if (!rv || !kill(0, SIGCONT))
				sd->sd_ctrl &= ~SW_PAUSE;
			if (rc) {
				svd_stat(sd);
				return 0;
			}
		}
		else if (sd->sd_stat == SW_DOWN) (void)unlink("down");
		return 1;
	case 'd':
		sd->sd_want = SW_DOWN;
		if (rc) {
			if (sd->sd_ctrl & SW_PAUSE)
				if (!kill(sd->sd_pid, SIGCONT)) {
					sd->sd_ctrl &= ~SW_PAUSE;
				}
			sd->sd_ctrl = SW_DOWN;
			svd_stat(sd);
			if (rv) kill(sd->sd_pid, SIGTERM);
			kill(0, SIGCONT);
		}
		return 1;
	case 'o':
		if (rc) {
			if (sd->sd_ctrl & SW_PAUSE) {
				if (!kill(0, SIGCONT)) {
					sd->sd_ctrl &= ~SW_PAUSE;
				}
			}
			sd->sd_ctrl = SW_DOWN;
			svd_stat(sd);
			return 0;
		}
		else {
			sd->sd_ctrl = SW_DOWN;
			sd->sd_want = SW_RUN;
			svd_stat(sd);
			return 1;
		}
	}

	if (sd->sd_stat < SW_RUN) return 0;
	if (!rc) return 0;

	switch (c)
	{
	case 'p':
		sd->sd_ctrl |= SW_PAUSE;
		if (rv) rv = kill(sd->sd_pid, SIGSTOP);
		svd_stat(sd);
		return 0;
	case 'c':
		if (sd->sd_ctrl & SW_PAUSE) {
			sd->sd_ctrl &= ~SW_PAUSE;
			if (rv) rv = kill(sd->sd_pid, SIGCONT);
			svd_stat(sd);
		}
		return 0;
	}

	if (!rv) return 0;
	sd->sd_ctrl &= SW_TERM;
	svd_stat(sd);

	if (c == 'h')
		kill(sd->sd_pid, SIGHUP);
	else if (c == 'a')
		kill(sd->sd_pid, SIGALRM);
	else if (c == 'i')
		kill(sd->sd_pid, SIGINT);
	else if (c == 'q')
		kill(sd->sd_pid, SIGQUIT);
	else if (c == '1')
		kill(sd->sd_pid, SIGUSR1);
	else if (c == '2')
		kill(sd->sd_pid, SIGUSR2);
	else if (c == 't')
		kill(sd->sd_pid, SIGTERM);
	else if (c == 'k')
		kill(sd->sd_pid, SIGKILL);
	else if (c == 'x') {
		sd_log->sd_ctrl &= SW_EXIT;
		sd_log->sd_want  = SW_DOWN;
		svd_stat(sd);
		kill(sd->sd_pid, SIGTERM);
		if (sd->sd_want == SW_DOWN) kill(0, SIGCONT);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int fd, rv, i;
	int *sig = (int []) { SIGHUP, SIGINT, SIGTERM, SIGQUIT, SIGUSR1, SIGUSR2, SIGALRM, 0 };
	char bf[8];
	FILE *fp;
	struct sigaction action;
	sigset_t mask;
	struct pollfd fds[4];

	progname = strrchr(argv[0], '/');
	if (!progname)
		progname = argv[0];
	else
		progname++;

	if (argc < 2)
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
		ERR("missing `SERVICE' argument\n", NULL);
		exit(EXIT_FAILURE);
	}

	if (chdir(*argv))
		ERROR("Failed to change directory to `%s'", *argv);
	if (mkdir("supervise", 0755) && errno != EEXIST)
		ERROR("Failed to create `%s' directory", "supervise");
	if ((fd = open(sd_svc->sv_lock, O_RDONLY | O_CLOEXEC)) != -1) {
		fp = fdopen(fd, "r");
		if (fscanf(fp, "%d", &rv)) {
			if (!kill(rv, 0)) {
				WARN("%s: {pid=%d} `%s' is already running!\n", *argv, rv, "svd");
				exit(EXIT_SUCCESS);
			}
		}
		close(fd);
	}
	atexit(svd_clean);

	if (!access("log", F_OK)) {
		sd_log->sv_log++;
		if (!access("log/down", R_OK))
			sd_log->sd_want = SW_DOWN;
		else
			sd_log->sd_want = SW_RUN;
		if (mkdir("log/supervise", 0755) && errno != EEXIST) {
			ERR("Failed to create `%s' directory: %s\n", "log/supervise",
					strerror(errno));
			sd_log->sv_log--;
		}
	}

	for (i = 0; i <= sd_log->sv_log; i++)
	{
	if ((sde[i].fd_lock = open(sde[i].sv_lock, O_CREAT | O_WRONLY | O_CLOEXEC | O_TRUNC, 0644)) == -1)
		ERROR("Failed to open `%s'", sde[i].sv_lock);
#ifdef HAVE_FLOCK
	if ((rv = flock(sde[i].fd_lock, LOCK_EX | LOCK_NB)))
#else
	if ((rv = lockf(sde[i].fd_lock, F_LOCK | F_TLOCK, sizeof(pid_t))))
#endif
		ERROR("Failed to lock `%s'", sde[i].sv_lock);
	sde[i].fp_lock = fdopen(sde[i].fd_lock, "w");
	fprintf(sde[i].fp_lock, "%d\n", getpid());
	fflush(sde[i].fp_lock);
	/*if (!rv)
#ifdef HAVE_FLOCK
		(void)flock(sde[i].fd_lock, LOCK_UN);
#else
		(void)lockf(sde[i].fd_lock, F_ULOCK, sizeof(pid_t));
#endif
	close(sde[i].fd_lock);*/
	if ((sde[i].fd_status = open(sde[i].sv_status, O_CREAT | O_WRONLY | O_CLOEXEC, 0644)) == -1)
		ERROR("Failed to open `%s'", sde[i].sv_status);
	sde[i].fp_status = fdopen(sde[i].fd_status, "w");

	TIMESPEC(&sde[i].sv_time);
	(void)unlink(sde[i].sv_control);
	if (mkfifo(sde[i].sv_control, 0600))
		ERROR("Failed to make `%s' (read)", sde[i].sv_control);
	if ((sde[i].fd_ctrl = open(sde[i].sv_control, O_RDONLY | O_NDELAY | O_NONBLOCK | O_CLOEXEC)) == -1)
		ERROR("Failed to open `%s'", sde[i].sv_control);
	if ((fd = open(sde[i].sv_control, O_WRONLY | O_NDELAY | O_NONBLOCK | O_CLOEXEC)) == -1)
		ERROR("Failed to open `%s' (write)", sde[i].sv_control);

	/*(void)unlink(sde[i].sv_ok);
	if (mkfifo(sde[i].sv_ok, 0600))
		ERROR("Failed to make `%s' fifo", sde[i].sv_ok);
	if ((fd = open(sde[i].sv_ok, O_RDONLY | O_NDELAY | O_CLOEXEC)) == -1)
		ERROR("Failed to open `%s'", sde[i].sv_ok);
	close(fd);*/
	}

	if (ERR_syslog) openlog(progname, LOG_CONS | LOG_ODELAY | LOG_PID, LOG_DAEMON);
	/* set up signal handler */
	memset(&action, 0, sizeof(action));
	sigemptyset(&mask);
	sigemptyset(&action.sa_mask);
	action.sa_sigaction = svd_sigaction;
	action.sa_flags = SA_SIGINFO | SA_RESTART;
	for ( ; *sig; sig++)
	{
		sigaddset(&mask, *sig);
		if (sigaction(*sig, &action, NULL))
			err_syslog(LOG_ERR, "Failed to register `%s' signal: %s",
					strsignal(*sig), strerror(errno));
	}
	/* set up the signal mask */
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL))
		ERROR("Failed to setup signal mask!", NULL);

	fds[0].fd = sd_svc->fd_ctrl;
	fds[1].fd = sd_log->sv_log ? sd_log->fd_ctrl : -1;
	fds[2].fd = *pipefd ? *pipefd : -1;
	fds[0].events = fds[1].events = fds[2].events = POLLIN;

	for (;;)
	{
		svd();
		do {
			fd = sd_svc->fd_ctrl;
			rv = poll(fds, 3U, 3600000);
			if (rv == -1) {
				if (errno == EINTR) break;
				err_syslog(LOG_DEBUG, "cannot poll file descriptors");
			}
			else if (rv > 0) {
				if (*pipefd) {
					if (fds[2].revents & POLLIN) fd = *pipefd;
				}
				else if (sd_log->sv_log) {
					if (fds[1].revents & POLLIN) fd = sd_log->fd_ctrl;
				}
			}

			do {
				rv = read(fd, bf, sizeof(bf));
				if ((fd == *pipefd) && !sd_log->sd_pid) printf("%s", bf);
				else break;
			} while (rv > 0);
			if (fd == *pipefd) continue;

			if (rv == -1) {
				if (errno == EINTR)
					break;
				else if (errno == EAGAIN)
					continue;
				else {
					err_syslog(LOG_DEBUG, "Failed to read `%s' named pipe: %s",
							sd_svc->sv_control, strerror(errno));
					continue;
				}
			}
			else if (!rv) continue;

			switch (*bf)
			{
			case 'x':
				if ((sd_svc->sd_want == SW_DOWN) && !sd_svc->sd_pid) {
					if (sd_log->sd_pid)
						svd_ctrl(sd_log, *bf);
					else exit(EXIT_SUCCESS);
				}
			case 'u': case 'd': case 'o': case 'p': case 'c': case 'h':
			case 'a': case 'i': case 'q': case '1': case '2': case 't':
			case 'k':
				if (fd == sd_svc->fd_ctrl) {
					if (!svd_ctrl(sd_svc, *bf)) continue;
				}
				else if (fd == sd_log->fd_ctrl) {
					if (!svd_ctrl(sd_log, *bf)) continue;
				}
				else continue;
				break;
			default:
				continue;
			}
		} while (rv == -1);
	}

	return EXIT_SUCCESS;
}
