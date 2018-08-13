/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-run.c  0.14.0 2018/08/06
 */

#include <stdio.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>
#include <pthread.h>
#include "sv-deps.h"

#define OFFSET_T_SIZE(SIZE, align, remind)                                   \
	(SIZE-(SIZE % sizeof(int))) % (align*sizeof(int)) > remind*sizeof(int) ? \
	(align+remind)*sizeof(int)-(SIZE-(SIZE % sizeof(int))) % (align*sizeof(int)) : \
	remind*sizeof(int)-(SIZE-(SIZE % sizeof(int))) % (align*sizeof(int))

#define THREAD_T_SIZE (sizeof(pthread_cond_t)+sizeof(pthread_mutex_t)*2LU+sizeof(pthread_rwlock_t)+sizeof(pthread_t))
struct svcrun_list {
	unsigned int rl_lid;
	int argc;
	const char **argv;
	SV_StringList_T *list;
	struct svcrun *run;
	int retval;
	size_t rl_job, rl_len, rl_siz, rl_count;
	struct svcrun_list *rl_next, *rl_prev;
	pthread_cond_t rl_cond;
	pthread_mutex_t rl_mutex;
	pthread_rwlock_t rl_lock;
	pthread_t rl_tid;
	pthread_mutex_t rl_pid;
	char __pad[OFFSET_T_SIZE(THREAD_T_SIZE, 4LU, 4LU)];
};
#undef THREAD_T_SIZE
#undef OFSET_T_SIZE

extern pid_t sv_pid;

static struct svcrun *RUN;
static struct svcrun_list *RL_SVC;
static unsigned int RL_COUNT = 1U;
static pthread_attr_t  RL_SVC_ATTR;
static pthread_rwlock_t RL_SVC_LOCK;
static pthread_t RL_SVC_SIGHANDLER_TID;
static sigset_t ss_thread;
static pthread_t RL_PID_TID;
static pthread_cond_t RL_PID_COND = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t RL_PID_MUTEX = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutexattr_t RL_PID_MUTEX_ATTR;

const char *progname;
const char *signame[] = { "SIGHUP", "SIGINT", "SIGQUIT", "SIGTERM", "SIGUSR1",
	"SIGUSR2", "SIGKILL" };
static const char *applet = "sv-run";

/* !!! likewise (service command) !!! */
const char *const sv_svc_cmd[] = { "stop", "start",
	"add", "del", "desc", "remove", "restart", "status", "zap"
};

static const char *const environ_whitelist[] = {
	"PATH", "SHELL", "SHLVL", "USER", "HOME", "TERM", "TMP", "TMPDIR",
	"LANG", "LC_ALL", "LC_ADDRESS", "LC_COLLATE", "LC_CTYPE", "LC_NUMERIC",
	"LC_MEASUREMENT", "LC_MONETARY", "LC_MESSAGES", "LC_NAME", "LC_PAPER",
	"LC_IDENTIFICATION", "LC_TELEPHONE", "LC_TIME", "PWD", "OLDPWD", "LOGNAME",
	"COLUMNS", "LINES", "UID", "GID", "EUID", "EGID",  "__SV_DEBUG_FD__", NULL
};
static const char *environ_list[] = {
	"COLUMNS", "SVC_DEBUG", "SVC_TRACE", "__SVC_WAIT__", "SV_RUNDIR", "SV_SVCDIR",
	"SV_LIBDIR", "SV_SYSBOOT_LEVEL", "SV_SHUTDOWN_LEVEL", "SV_VERSION",
	"SV_SYSTEM", "SV_PREFIX", "SV_RUNLEVEL", "SV_INITLEVEL",
	NULL
};

static void thread_signal_setup(void);
static void thread_signal_action(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)));
__attribute__((__noreturn__)) static void *thread_sigchld_handler(void *arg __attribute__((__unused__)));
__attribute__((__noreturn__)) static void *thread_signal_worker(void *arg __attribute__((__unused__)));
static void *thread_worker_handler(void *arg);
static void  thread_worker_cleanup(struct svcrun_list *p);

/* execute a service command;
 * @run: an svcrun structure;
 * @return: -errno on errors,
 */
int svc_cmd(struct svcrun *run);
static int svc_run(struct svcrun *run);
__attribute__((__unused__)) static int svc_waitpid(struct svcrun *run, int flags);
#define SVC_WAITPID 0x0100

/*
 * setup service dependencies
 * @run: svcrun structure;
 * @return: 0 on succsess,
 *        > 0 for non fatals errors
 *        < 0 for fatals errors;
 */
static int svc_depend(struct svcrun *run);

/*
 * querry service status
 * @svc: service name;
 * @status: int value, see the SV_SVC_(STATE|MARK)_* macros;
 * @return: true/false;
 */
static int svc_state(const char *svc, int status);

/*
 * set service status
 * @svc: service name; or @run instead;
 * @status: int value [dfrs], see the SV_SVC_(STATE|MARK)_* macros;
 * @return: 0 on success;
 */
static int svc_mark(struct svcrun *run, int status, const char *what);

/*
 * lock file for service to start/stop
 * @svc: service name;
 * @lock_fd: SVC_LOCK to lock, lock_fd >= 0 to unlock;
 * @timeout: timeout to use to poll the lockfile (SVC_TIMEOUT_SECS);
 * @return: fd >= 0 on success; negative (usually -errno) on failure;
 */
static int svc_lock(const char *svc, int lock_fd, int timeout);
#define SVC_LOCK -1 /* magic number to lock a service */

/*
 * wait for the availability of the lock file;
 * @svc: service name;
 * @timeout: timeout to use to poll the lockfile;
 * @return: 0 on success (lockfile available);
 */
static int svc_wait(const char *svc, int timeout, int lock_fd, pid_t pid);
#define SVC_TIMEOUT_SECS 60    /* default delay */
#define SVC_TIMEOUT_MSEC 1000  /* interval for displaying warning */
#define SVC_TIMEOUT_POLL 100   /* poll interval */

/*
 * execute a service with the appended arguments
 */
int svc_exec(int argc, const char *argv[]);

/*
 * execute a service list (called from svc_init())
 * @return 0 on success or number of failed services
 */
int svc_execl(SV_StringList_T *list, int argc, const char *argv[]);

/*
 * remove service temporary files
 */
static void svc_zap(const char *svc);

/* signal handler/setup */
static void rs_sighandler(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)));
static void rs_sigsetup(void);
void svc_sigsetup(void);
extern sigset_t ss_child, ss_null, ss_full, ss_old;

/*
 * generate a default environment for service
 */
static const char **svc_environ;
static void svc_env(void);
static int svc_print_status(struct svcrun *run, struct stat *st_buf, char *buf, char *type);

static int svc_print_status(struct svcrun *run, struct stat *st_buf, char *buf, char *type)
{
	int fd, retval;
	char *off, *tmp;
	size_t LEN, len, OFF;
	struct tm lt;
#define STRFTIME_OFF 32
	char *ptr = buf+1024LU-STRFTIME_OFF;
#define MK_STRFTIME(tmpdir) do {                                    \
	snprintf(tmp, 1024LU-OFF, "%s/%s", tmpdir, run->name);          \
	stat(tmp, st_buf);                                              \
	localtime_r(&st_buf->st_mtime, &lt);                            \
	strftime(ptr, STRFTIME_OFF, "%F %T", (const struct tm*)&lt);    \
} while (0/*CONST COND*/)

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%p, %p, %p)\n", __func__, run, st_buf, buf);
#endif

	OFF = strlen(buf)+1LU;
	/* find the runlevel */
	for (fd = 0; sv_init_level[fd]; fd++) {
		snprintf(buf+OFF, 1024LU-OFF, "%s.init.d/%s/%s", SV_SVCDIR,
				 sv_init_level[fd], run->name);
		if (!lstat(buf+OFF, st_buf)) break;
	}
	len = 1024LU-STRFTIME_OFF-13LU;
	if (sv_init_level[fd])
		snprintf(buf+len, 13LU, "%s", sv_init_level[fd]);
	else
		snprintf(buf+len, 13LU, "  ");

	/* output format preparation to the following:
	 * SERVICE (type=rs|sv pid=int8_t service=SERVICE_PATH) [STATE] {at|since DATE} *command=COMMAND*
	 */
	st_buf->st_mtime = 0;
	len = strlen(run->name);
	if (len > 24LU) len +=  2LU;
	else           len   = 24LU;
	LEN = len+16LU+8LU+7LU*3LU+10LU*6LU+1LU;

	memmove(buf+LEN+104LU+1LU+OFF, buf, OFF);
	retval  = snprintf(buf, LEN+104LU, "%-*.128s %s(%stype=%s%s%s ",
			len, run->name, print_color(COLOR_CYN, COLOR_FG),
			print_color(COLOR_RST, COLOR_RST),
			print_color(COLOR_BLU, COLOR_FG), type,
			print_color(COLOR_RST, COLOR_RST));
	off = buf+retval;
	len = retval;
	retval += snprintf(off, LEN+104LU-retval, "             %s{%s%7.8s%s})%s",
			print_color(COLOR_CYN, COLOR_FG),
			print_color(COLOR_MAG, COLOR_FG), buf+1024LU-STRFTIME_OFF-13LU,
			print_color(COLOR_CYN, COLOR_FG),
			print_color(COLOR_RST, COLOR_RST));
	OFF += LEN+104LU+13LU+1LU;
	tmp = buf+OFF+2LU;

	if (svc_state(run->name, SV_SVC_STAT_DOWN)) {
		MK_STRFTIME(SV_TMPDIR_DOWN);

		printf("%s %s[%sdown%s] {%sat %s%s}%s\n", buf,
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_MAG, COLOR_FG),
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST), ptr,
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST));
		return 8;
	}
	else if (svc_state(run->name, SV_SVC_STAT_STAR) ||
		     svc_state(run->name, SV_SVC_STAT_PIDS)) {
		if (svc_state(run->name, SV_SVC_STAT_PIDS)) {
			MK_STRFTIME(SV_TMPDIR_PIDS);

			/* get the pidfile */
			if ((fd = open(tmp, O_RDONLY)) > 0) {
				sprintf(tmp, "pid=");
				if ((retval = read(fd, tmp+4LU, 8LU)) > 0) {
					LEN = 3LU+retval;
					if (*(tmp+LEN) == '\n') {
						*(tmp+LEN) = '\0';
						LEN--;
					}
					memcpy(buf+len, tmp, LEN);
				}
				close(fd);
			}
		}
		else
			MK_STRFTIME(SV_TMPDIR_STAR);

		printf("%s %s[%sstarted%s] {%sat %s%s}%s\n", buf,
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_GRN, COLOR_FG),
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST), ptr,
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST));
		return 0;
	}
	else if (svc_state(run->name, SV_SVC_STAT_FAIL)) {
		MK_STRFTIME(SV_TMPDIR_FAIL);

		if ((fd = open(tmp, O_RDONLY)) > 0) {
			if ((retval = read(fd, tmp, 1024LU-(tmp-buf)-STRFTIME_OFF)) > 0) {
				if (*(tmp+retval) == '\n') *(tmp+retval) = '\0';
				else if (*(tmp+retval) == 's') *(tmp+retval) = '\0'; /* BUGFIX */
				else tmp[++retval] = '\0';
				printf("%s %s[%sfailed%s]  {%sat %s%s} *%scommand=%s%s*%s\n", buf,
						print_color(COLOR_CYN, COLOR_FG),
						print_color(COLOR_RED, COLOR_FG),
						print_color(COLOR_CYN, COLOR_FG),
						print_color(COLOR_RST, COLOR_RST), ptr,
						print_color(COLOR_RED, COLOR_FG),
						print_color(COLOR_RST, COLOR_RST), tmp,
						print_color(COLOR_RED, COLOR_FG),
						print_color(COLOR_RST, COLOR_RST));
			}
			close(fd);
		}
		else
			printf("%s %s[%sfailed%s]  {%sat %s%s}%s\n", buf,
					print_color(COLOR_CYN, COLOR_FG),
					print_color(COLOR_RED, COLOR_FG),
					print_color(COLOR_CYN, COLOR_FG),
					print_color(COLOR_RST, COLOR_RST), ptr,
					print_color(COLOR_CYN, COLOR_FG),
					print_color(COLOR_RST, COLOR_RST));
		return 9;
	}
	else if (svc_state(run->name, SV_SVC_STAT_WAIT)) {
		MK_STRFTIME(SV_TMPDIR_WAIT);

		if ((fd = open(tmp, O_RDONLY)) > 0) {
			if ((retval = read(fd, tmp+4LU, sizeof(buf)-(tmp-buf)-STRFTIME_OFF)) > 0) {
				if (*(tmp+retval) == '\n') *(tmp+retval) = '\0';
				else if (*(tmp+retval) == 's') *(tmp+retval) = '\0'; /* BUGFIX */
				else *(tmp+retval++) = '\0';
				off = strchr(tmp, ':');
				*off++ = '\0';
				memcpy(buf+len, tmp, strlen(tmp) > 12LU ? 12LU : strlen(tmp));
				len = strlen(off);
				printf("%s %s[%swaiting%s] {%since %s%s} *%s%s*%s\n", buf,
						print_color(COLOR_CYN, COLOR_FG),
						print_color(COLOR_YLW, COLOR_FG),
						print_color(COLOR_CYN, COLOR_FG),
						print_color(COLOR_RST, COLOR_RST), ptr,
						print_color(COLOR_YLW, COLOR_FG), off,
						print_color(COLOR_YLW, COLOR_FG),
						print_color(COLOR_RST, COLOR_RST));
			}
			close(fd);
		}
		else
			printf("%s %s[%swaiting%s] {%ssince %s%s}%s\n", buf,
					print_color(COLOR_CYN, COLOR_FG),
					print_color(COLOR_YLW, COLOR_FG),
					print_color(COLOR_CYN, COLOR_FG), ptr,
					print_color(COLOR_RST, COLOR_RST),
					print_color(COLOR_CYN, COLOR_FG),
					print_color(COLOR_RST, COLOR_RST));
		return 10;
	}
	else if (svc_state(run->name, SV_SVC_STAT_ACTIVE)) {
		MK_STRFTIME(SV_TMPDIR);

		printf("%s %s[%sactive%s] {%ssince %s%s}%s\n", buf,
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_MAG, COLOR_FG),
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST), ptr,
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST));
		return 8;
	}
	else {
		printf("%s %s[%sstopped%s]%s\n", buf,
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_BLU, COLOR_FG),
				print_color(COLOR_CYN, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST));
		return 7;
	}
#undef STRFTIME_OFF
#undef MK_STRFTIME
}

int svc_cmd(struct svcrun *run)
{
	static char *deps[2] = { "--deps", "--nodeps" };
	static char *type[2] = { "--rs", "--sv" };
	static const char *svcd[] = { NULL, NULL, NULL };
	static struct stat st_dep = { .st_mtime = 0 };
	struct stat st_buf;
	int retval;
	int i;
	char *cmd = (char*)run->argv[4];
	char buf[10124] = { "" };

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, run);
#endif

	if (!st_dep.st_mtime) {
		stat(SV_SVCDEPS_FILE, &st_dep);
#if defined(PREFIX) && !defined(__linux__)
		svcd[1] = PREFIX;
		svcd[2] = sv_getconf("SV_PREFIX");
#else
		svcd[1] = sv_getconf("SV_PREFIX");
#endif
	}
	run->ARGV = NULL;
	run->cmd = -1;
	run->dep = NULL;
	run->mark = 0;

	/* get service path */
	if (!run->path) {
		retval = -ENOENT;
		for (i = 0; i < sizeof(svcd); i++) {
			if (svcd[i])
				snprintf(buf, sizeof(buf), "%s/%s/%s", svcd[i], SV_SVCDIR, run->name);
			else if (!i)
				snprintf(buf, sizeof(buf), "%s/%s", SV_SVCDIR, run->name);
			else
				break;
			if (!access(buf, F_OK)) {
				retval = 0;
				break;
			}
		}
		if (retval)
			return -ENOENT;
	}
	/* get service type */
	stat(buf, &st_buf);
	retval = S_ISDIR(st_buf.st_mode);

	if (strcmp(cmd, sv_svc_cmd[SV_SVC_CMD_START]) == 0) {
		run->mark = SV_SVC_STAT_STAR;
		if (svc_state(run->name, SV_SVC_STAT_STAR) ||
			svc_state(run->name, SV_SVC_STAT_PIDS)) {
			return -EBUSY;
		}
		run->cmd  = SV_SVC_CMD_START;
	}
	else if (strcmp(cmd, sv_svc_cmd[SV_SVC_CMD_STOP]) == 0) {
		run->mark = SV_SVC_MARK_STAR;
		if (!(svc_state(run->name, SV_SVC_STAT_STAR) ||
			  svc_state(run->name, SV_SVC_STAT_PIDS))) {
			return -EINVAL;
		}
		run->cmd  = SV_SVC_CMD_STOP;
	}
	else if (strcmp(cmd, sv_svc_cmd[SV_SVC_CMD_ADD]) == 0)
		run->cmd  = SV_SVC_CMD_ADD;
	else if (strcmp(cmd, sv_svc_cmd[SV_SVC_CMD_DEL]) == 0)
		run->cmd  = SV_SVC_CMD_DEL;
	else if (strcmp(cmd, sv_svc_cmd[SV_SVC_CMD_ZAP]) == 0) {
		svc_zap(run->name);
		return 0;
	}
	else if (strcmp(cmd, sv_svc_cmd[SV_SVC_CMD_STATUS]) == 0) {
		return svc_print_status(run, &st_buf, buf, type[retval]+2U);
	}

	run->path = err_strdup(buf);
	if (!run->svc->data)
		run->svc->data = sv_svcdeps_load(run->name);
	run->dep = run->svc->data;
	if (!run->dep) {
		retval = -ENOENT;
		goto reterr;
	}
	if (!run->dep->timeout) run->dep->timeout = sv_timeout;

	/* get service status */
	switch(run->cmd) {
	case SV_SVC_CMD_ADD:
	case SV_SVC_CMD_DEL:
		if (sv_init < 0) {
			fprintf(stderr, "%s: runlevel argument is required\n", progname);
			fprintf(stderr, "Usage: %s -(0|1|3|4|5) %s %s\n", progname,
					run->name, sv_svc_cmd[run->cmd]);
			return -EINVAL;
		}

		snprintf(buf, sizeof(buf), "%s.init.d/%s/%s", SV_SVCDIR, sv_init_level[sv_init],
				run->name);
		if (!access(buf, F_OK)) {
			if (run->cmd == SV_SVC_CMD_DEL)
				unlink(buf);
			return 0;
		}

		if (symlink(run->path, buf)) {
			ERR("%s: Failed to add service: %s\n", run->name, strerror(errno));
			return 1;
		}
		else
			return 0;
		break;

	case SV_SVC_CMD_START:
		if (sv_system > SV_KEYWORD_SHUTDOWN && SV_KEYWORD_GET(run->dep, sv_system)) {
			LOG_WARN("`%s' has `%s' keyword\n", run->name, sv_keywords[sv_system]);
			retval = -EPERM;
			goto reterr;
		}
		break;
	case SV_SVC_CMD_STOP:
		if (SV_KEYWORD_GET(run->dep, SV_KEYWORD_SHUTDOWN)) {
			LOG_WARN("`%s' has `%s' keyword\n", run->name,
					sv_keywords[SV_KEYWORD_SHUTDOWN]);
			retval = -EPERM;
			goto reterr;
		}
		if (sv_system > SV_KEYWORD_SHUTDOWN && SV_KEYWORD_GET(run->dep, sv_system)) {
			LOG_WARN("`%s' has `%s' keyword\n", run->name, sv_keywords[sv_system]);
			retval = -EPERM;
			goto reterr;
		}
		break;
	}

	run->ARGV = err_calloc(run->argc, sizeof(void*));
	for (i = 0; i <= run->argc; i++)
		run->ARGV[i] = run->argv[i];

	/* set service type */
	if (retval)
		run->ARGV[1] = type[1];
	else
		run->ARGV[1] = type[0];
	run->ARGV[2] = deps[0];
	run->ARGV[3] = run->path;

	/* check service mtime */
	if (st_buf.st_mtime > st_dep.st_mtime)
		LOG_WARN("%s was updated -- `scan' command might be necessary?\n",
				run->name);

	/* setup dependencies */
	if (run->cmd == SV_SVC_CMD_START && svc_deps) {
		retval = svc_depend(run);
		if (retval) {
			LOG_ERR("%s: Failed to set up service dependencies\n", run->name);
			svc_mark(run, SV_SVC_STAT_FAIL, cmd);
			retval = -ECANCELED;
			goto reterr;
		}
		else
			run->ARGV[2] = deps[1];
	}

	if (!svc_quiet)
		svc_log("[%s] service %s...\n", run->name, cmd);
	if (!svc_environ) svc_env();
	run->envp = svc_environ;
	return svc_run(run);
reterr:
	if (*buf) {
		free((void*)run->path);
		run->path = NULL;
	}
	if (run->ARGV) {
		free((void*)run->ARGV);
		run->ARGV = NULL;
	}
	return retval;
}

static int svc_run(struct svcrun *run)
{
	size_t len;
	int val;
	off_t off = 0;
	char buf[128];
	pid_t pid;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, run);
#endif

	if (SV_KEYWORD_GET(run->dep, SV_KEYWORD_TIMEOUT))
		run->dep->timeout = -1;
	run->status = -1;
	run->sig = 0;

	/* block signal before fork() */
	sigprocmask(SIG_SETMASK, &ss_full, NULL);
	pid = fork();
	if (pid > 0) { /* parent */
		/* restore signal mask */
		if (run->rl_svc) {
			sigprocmask(SIG_SETMASK, &ss_thread, NULL);
			pthread_mutex_lock  (&((struct svcrun_list*)(run->rl_svc))->rl_pid);
			run->pid = pid;
			pthread_mutex_unlock(&((struct svcrun_list*)(run->rl_svc))->rl_pid);
		}
		else {
			sigprocmask(SIG_SETMASK, &ss_child, NULL);
			run->pid = pid;
		}
		return SVC_WAITPID;
	}
	else if (run->pid == 0) /* child */
		goto runsvc;
	else {
		ERROR("%s:%d: Failed to fork()", __func__, __LINE__);
	}

runsvc:
	/* restore signal mask */
	sigprocmask(SIG_SETMASK, &ss_child, NULL);

	/* lock the lock file before any command */
	if ((run->lock = svc_lock(run->name, SVC_LOCK, SVC_TIMEOUT_SECS)) < 0) {
		LOG_ERR("%s: Failed to setup lockfile for service\n", run->name);
		_exit(ETIMEDOUT);
	}
	/* close the lockfile to be able to mount rootfs read-only */
	if (sv_init == SV_SHUTDOWN_LEVEL && run->cmd == SV_SVC_CMD_START)
		svc_lock(run->name, run->lock, 0);

	/* supervise the service */
	if (run->dep->timeout) {
		/* block signal before fork() */
		sigprocmask(SIG_SETMASK, &ss_full, NULL);

		if ((run->cld = fork()) > 0) /* parent */
			goto supervise;
		else if (run->cld < 0) { /* child */
			ERR("%s:%d: Failed to fork(): %s\n", __func__, __LINE__,
					strerror(errno));
			_exit(EXIT_FAILURE);
		}
		else
			/* restore signal mask */
			sigprocmask(SIG_SETMASK, &ss_old, NULL);
	}
	else {
#ifdef SV_DEBUG
		if (sv_debug) DBG("executing service=%s command=%s (pid=%d)\n",
				run->name, run->argv[4], getpid());
#endif
	}

	/* write the service command and the pid to the lock file */
	snprintf(buf, sizeof(buf), "pid=%d:command=%s", getpid(), run->argv[4]);
	len = strlen(buf);
	do {
		val = write(run->lock, buf+off, len);
		if (val < 0)
			if (errno != EINTR) {
				LOG_ERR("Failed to write service command to `%s/%s': %s\n",
					SV_TMPDIR_WAIT, run->name, strerror(errno));
				break;
			}
		off += val; len -= val;
	} while (len);

	/* restore previous default signal handling */
	svc_sigsetup();

	execve(SV_RUN_SH, (char *const*)run->ARGV, (char *const*)run->envp);
	ERR("%s:%d: Failed to execve(): %s\n", __func__, __LINE__, strerror(errno));
	_exit(EXIT_FAILURE);
supervise:
	fprintf(debugfp, "%s\n", buf);
	RUN = run;
	/* restore signal mask */
	sigprocmask(SIG_SETMASK, &ss_child, NULL);
	/* setup SIGCHILD,SIGALRM and unblock SIGCHILD */
	rs_sigsetup();

	while (run->cld) {
		/* setup a timeout and wait for the child */
		if (run->dep->timeout)
			alarm(run->dep->timeout);
#ifdef SV_DEBUG
		if (sv_debug) DBG("waiting pid=%d (service=%s)\n", run->cld, run->name);
#endif
		sigsuspend(&ss_old);
	}
	if (run->status > 0)
		_exit(run->status);
	_exit(EXIT_FAILURE);
}

__attribute__((__unused__)) static int svc_waitpid(struct svcrun *run, int flags)
{
	int status = 0;
	pid_t pid = 0;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%p[=%s], %d)\n", __func__, run, run->name, flags);
#endif

	/* do this hack to only mark children status */
	if (run->status != -1)
		status = run->status;
	else
	do {
#ifdef SV_DEBUG
		if (sv_debug) DBG("waiting for pid=%d (service=%s)\n", run->cld, run->name);
#endif
		pid = waitpid(run->cld, &status, flags);
		if (pid < 0) {
			if (errno != EINTR) {
				LOG_ERR("%s:%d:service=%s: Failed to waitpid(%d,..): %s\n",
						__func__, __LINE__, run->name, run->cld, strerror(errno));
				return -1;
			}
		}
		if (pid == 0)
			return SVC_WAITPID;
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));

	if (WIFEXITED(status))
		run->status = WEXITSTATUS(status);
	else if (flags & WUNTRACED && WIFSTOPPED(status))
		return SVC_WAITPID;
	else if (WIFSIGNALED(status))
		run->status = WTERMSIG(status);
	if (run->lock)
		svc_lock(run->name, run->lock, 0);
	run->cld = 0;

	/* do not mark service status twice */
	if (run->dep->timeout && !pid)
		return run->status;
	svc_mark(run, SV_SVC_MARK_WAIT, NULL);
	if (!svc_quiet)
		svc_end(run->name, run->status);
	if (run->status)
		svc_mark(run, SV_SVC_STAT_FAIL, run->argv[4]);
	else if (run->mark)
		svc_mark(run, run->mark, NULL);

	return run->status;
}

static int svc_depend(struct svcrun *run)
{
	int type, val = 0, retval = 0;
	int p;
	SV_DepTree_T deptree = { NULL, NULL, 0, 0 };

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, run);
#endif

	/* skip before deps type */
	for (type = SV_SVCDEPS_USE; type <= SV_SVCDEPS_NEED; type++) {
		if (TAILQ_EMPTY(run->dep->deps[type]))
			continue;
		/* build a deptree to avoid segfault because cyclical dependencies */
		deptree.list = run->dep->deps[type];
		svc_deptree_load(&deptree);
		if (!strcmp(run->argv[4], sv_svc_cmd[SV_SVC_CMD_START]))
			p = deptree.size-1;
		else p = 0;
		while (p >= 0 && p < deptree.size) { /* PRIORITY_LEVEL_LOOP */
			if (!TAILQ_EMPTY(deptree.tree[p]))
				val += svc_execl(deptree.tree[p], run->argc, run->argv);
			--p;
		} /* PRIORITY_LEVEL_LOOP */
		sv_deptree_free(&deptree);
		if (val > 0 && type == SV_SVCDEPS_NEED)
			retval = val;
	}
	return retval;
}

off_t ENVIRON_OFF = ARRAY_SIZE(environ_list)-3;
static long environ_off;
static int  environ_fd;
static FILE *environ_fp;
static void svc_env(void)
{
	size_t len = 8;
	char buf[1024], *ptr;
	int i = 0, j;

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif

	if (svc_environ)
		return;
	svc_environ = err_calloc(len, sizeof(void *));

	if (!getenv("COLUMNS")) {
		sprintf(buf, "%d", get_term_cols());
		setenv("COLUMNS", buf, 1);
	}

	for (j = 0; environ_whitelist[j]; j++) {
		ptr = getenv(environ_whitelist[j]);
		if (ptr) {
			snprintf(buf, sizeof(buf), "%s='%s'", environ_whitelist[j], ptr);
			svc_environ[i++] = err_strdup(buf);
		}
		if (i == len) {
			len += 8;
			svc_environ = err_realloc(svc_environ, len*sizeof(void*));
		}
	}
	svc_environ[i++] = (char *)0;
	svc_environ_update(0L);
}
int svc_environ_update(off_t off)
{
	int j;
	char *ptr;

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%ld)\n", __func__, off);
#endif

	if (!environ_fd) {
		if ((environ_fd = open(SV_ENVIRON, O_CREAT|O_WRONLY|O_TRUNC|O_APPEND|O_CLOEXEC, 0644)) < 0)
			return -1;
		if (flock(environ_fd, LOCK_EX|LOCK_NB) < 0)
			return -1;
	}
	if (!environ_fp)
		if (!(environ_fp = fdopen(environ_fd, "w")))
			return -1;
	if (off)
		ftruncate(environ_fd, (off_t)environ_off);
	for (j = off; environ_list[j]; j++) {
		if (!off && j == ENVIRON_OFF)
			environ_off = ftell(environ_fp);
		if ((ptr = getenv(environ_list[j])))
			fprintf(environ_fp, "%s='%s'\n", environ_list[j], ptr);
	}
	fflush(environ_fp);
	return 0;
}

#define SVC_WAIT_LOCK                                    \
	if ((fp = fopen(f_path, "r"))) {                     \
		if (fscanf(fp, "pid=%d:", &pd) && !kill(pd, 0))  \
			w = svc_wait(svc, timeout, 0, pd);           \
		fclose(fp);                                      \
	}                                                    \
	else LOG_ERR("Failed to `fopen(%s,..)': %s\n", f_path, strerror(errno));

static int svc_lock(const char *svc, int lock_fd, int timeout)
{
	char f_path[PATH_MAX];
	int fd;
	FILE *fp;
	pid_t pd;
	int w;
	mode_t m;
	static int f_flags = O_NONBLOCK | O_CREAT | O_WRONLY | O_CLOEXEC;
	static mode_t f_mode = 0644;

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%s, %d, %d)\n", __func__, svc, lock_fd, timeout);
#endif

	if (svc == NULL)
		return -ENOENT;
	snprintf(f_path, sizeof(f_path), "%s/%s", SV_TMPDIR_WAIT, svc);

	if (lock_fd == SVC_LOCK) {
		if (!(w = access(f_path, F_OK))) {
			/* check the holder of the lock file */
			SVC_WAIT_LOCK;
		}
		m = umask(0);
		fd = open(f_path, f_flags, f_mode);
		/* got different behaviours of open(3p)/O_CREAT when the file is locked
		 * (try that mode | S_ISGID hack of SVR3 to enable mandatory lock?)
		 */
		if (!w && fd < 0) {
			switch (errno) {
				case EWOULDBLOCK:
				case EEXIST:
					SVC_WAIT_LOCK;
					if (w < 0)
						return -1;
					break;
				default:
					return -1;
			}
			if (!access(f_path, F_OK))
				unlink(f_path);
			fd = open(f_path, f_flags, f_mode);
		}
		umask(m);
		if (fd < 0) {
			ERR("Failed to open(%s...): %s\n", f_path, strerror(errno));
			return fd;
		}

		if (flock(fd, LOCK_EX|LOCK_NB) == -1)
			switch(errno) {
			case EWOULDBLOCK:
				if (svc_wait(svc, timeout, lock_fd, 0) > 0)
					return fd;
			default:
				LOG_ERR("%s: Failed to flock(%d, LOCK_EX...): %s\n", svc, fd,
							strerror(errno));
				close(fd);
				return -1;
			}
		return fd;
	}
	else if (lock_fd > 0)
		close(lock_fd);
	if (!access(f_path, F_OK))
		unlink(f_path);
	return 0;
}
#undef SVC_WAIT_LOCK

static int svc_wait(const char *svc, int timeout, int lock_fd, pid_t pid)
{
	int i, j;
	int err;
	int msec = SVC_TIMEOUT_MSEC, nsec, ssec = 10;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%s, %d, %d)\n", __func__, svc, timeout, lock_fd);
#endif
	if (timeout < ssec) {
		nsec = timeout;
		msec = 1000*timeout;
	}
	else
		nsec = timeout % ssec;
	nsec = nsec ? nsec : ssec;

	for (i = 0; i < timeout; ) {
		for (j = SVC_TIMEOUT_POLL; j <= msec; j += SVC_TIMEOUT_POLL) {
			if (svc_state(svc, SV_SVC_STAT_WAIT) < 1)
				return 0;
			if (pid && kill(pid, 0))
				return 0;
			/* add some insurence for failed services */
			if (lock_fd) {
				err = errno;
				if (flock(lock_fd, LOCK_EX|LOCK_NB) == 0)
					return lock_fd;
				errno = err;
			}
			/* use poll(3p) as a milliseconds timer (sleep(3) replacement) */
			if (poll(0, 0, SVC_TIMEOUT_POLL) < 0)
				return -1;
		}
		if (!(++i % ssec))
			WARN("waiting for %s (%d seconds)\n", svc, i);
	}
	return svc_state(svc, SV_SVC_STAT_WAIT) ? -1 : 0;
}

static void svc_zap(const char *svc)
{
	int i;
	char path[PATH_MAX];
	char *dirs[] = { SV_TMPDIR_DOWN, SV_TMPDIR_FAIL, SV_TMPDIR_STAR,
		SV_TMPDIR_PIDS, SV_TMPDIR_WAIT,
		SV_TMPDIR "/envs", SV_TMPDIR "/opts", NULL };

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%s)\n", __func__, svc);
#endif

	for (i = 0; dirs[i]; i++) {
		snprintf(path, sizeof(path), "%s/%s", dirs[i], svc);
		if (!access(path, F_OK))
			unlink(path);
	}
}

static int svc_mark(struct svcrun *run, int status, const char *what)
{
	char path[PATH_MAX], *ptr;
	int fd;
	int i;
	mode_t m;

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%p[=%s], %c, %s)\n", __func__, run, run->name, status, what);
#endif

	if (!run)
		return -ENOENT;
	run->dep->status = status;

	switch(status) {
		case SV_SVC_STAT_FAIL:
		case SV_SVC_MARK_FAIL:
			ptr = SV_TMPDIR_FAIL;
			break;
		case SV_SVC_STAT_DOWN:
		case SV_SVC_MARK_DOWN:
			ptr = SV_TMPDIR_DOWN;
			break;
		case SV_SVC_STAT_STAR:
		case SV_SVC_MARK_STAR:
			ptr = SV_TMPDIR_STAR;
			break;
		case SV_SVC_STAT_WAIT:
		case SV_SVC_MARK_WAIT:
			ptr = SV_TMPDIR_WAIT;
			break;
		default:
			return -EINVAL;
	}

	switch (status) {
		case SV_SVC_STAT_STAR:
			if (run->dep->virt) {
				snprintf(path, sizeof(path), "%s/%s", ptr, run->dep->virt);
				m = umask(0);
				fd = open(path, O_CREAT|O_WRONLY|O_NONBLOCK, 0644);
				umask(m);
				if (fd > 0) close(fd);
			}
		case SV_SVC_STAT_DOWN:
		case SV_SVC_STAT_FAIL:
		case SV_SVC_STAT_WAIT:
			snprintf(path, sizeof(path), "%s/%s", ptr, run->name);
			m = umask(0);
			fd = open(path, O_CREAT|O_WRONLY|O_NONBLOCK, 0644);
			umask(m);
			if (fd > 0) {
				if (what)
					(void)err_write(fd, (const char*)what, (const char*)ptr);
				close(fd);
				return 0;
			}
			return -1;
		case SV_SVC_MARK_STAR:
			svc_mark(run, SV_SVC_MARK_FAIL, NULL);
			if (run->dep->virt) {
				/* do not remove virtual service if there is another provider */
				fd = i = 0;
				do {
					if (strcmp(run->dep->virt, SERVICES.virt_svcdeps[i]->virt) == 0 &&
						svc_state(SERVICES.virt_svcdeps[i]->svc, SV_SVC_STAT_STAR))
						fd++;
					i++;
				} while (i < SERVICES.virt_count);

				if (fd == 1) {
					snprintf(path, sizeof(path), "%s/%s", ptr, run->dep->virt);
					if (!access(path, F_OK))
						unlink(path);
				}
			}
		default:
			snprintf(path, sizeof(path), "%s/%s", ptr, run->name);
			if (!access(path, F_OK))
				return unlink(path);
			else
				return 0;
	}
}

static int svc_state(const char *svc, int status)
{
	char path[PATH_MAX], *ptr = NULL;

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%s, %c)\n", __func__, svc, status);
#endif

	if (!svc)
		return 0;

	switch(status) {
		case SV_SVC_STAT_ACTIVE:
			ptr = SV_RUNDIR;
			break;
		case SV_SVC_STAT_FAIL:
		case SV_SVC_MARK_FAIL:
			ptr = SV_TMPDIR_FAIL;
			break;
		case SV_SVC_STAT_DOWN:
		case SV_SVC_MARK_DOWN:
			ptr = SV_TMPDIR_DOWN;
			break;
		case SV_SVC_STAT_PIDS:
			ptr = SV_TMPDIR_PIDS;
			break;
		case SV_SVC_STAT_STAR:
		case SV_SVC_MARK_STAR:
			ptr = SV_TMPDIR_STAR;
			break;
		case SV_SVC_STAT_WAIT:
		case SV_SVC_MARK_WAIT:
			ptr = SV_TMPDIR_WAIT;
			break;
		default:
			return 0;
	}
	snprintf(path, sizeof(path), "%s/%s", ptr, svc);
	if (access(path, F_OK)) return 0;
	return 1;
}

static void rs_sighandler(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)))
{
	int i = -1, serrno = errno;
	int j = -1;
	static const char *sn[] = { "SIGCONT", "SIGSTOP", "SIGTRAP" };

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%d, %p, %p)\n", __func__, sig, si, ctx);
#endif

	switch (sig) {
	case SIGALRM:
		if (RUN) {
			if (!RUN->sig)
				i = 3, RUN->sig = SIGTERM;
			else if (RUN->sig == SIGTERM)
				i = 2, RUN->sig = SIGQUIT;
			else
				i = 6, RUN->sig = SIGKILL;
			LOG_WARN("sending %s to process PID=%d (service=%s)!!!\n", signame[i],
					RUN->cld, RUN->name);
			kill(RUN->cld, RUN->sig);
		}
		break;
	case SIGCHLD:
		switch(si->si_code) {
		case CLD_CONTINUED:
			if (j < 0) j = 0;
		case CLD_STOPPED:
			if (j < 0) j = 1;
		case CLD_TRAPPED:
			if (j < 0) j = 2;
#ifdef SV_DEBUG
			if (sv_debug) DBG("pid=%d (service=%s) received %s signal\n", RUN->cld, RUN->name, sn[j]);
#endif
			errno = serrno;
			return;
		}
		if (RUN)
			(void)svc_waitpid(RUN, WNOHANG);
		break;
	default:
		ERR("caught unknown signal %d\n", sig);
	}

	/* restore errno */
	errno = serrno;
}

static void rs_sigsetup(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif

	sa.sa_sigaction = rs_sighandler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	sigprocmask(SIG_UNBLOCK, &ss_child, NULL);
}

void svc_sigsetup(void)
{
	struct sigaction sa;
	int *sig = (int []){ SIGALRM, SIGCHLD, SIGHUP, SIGINT, SIGQUIT, SIGTERM,
		SIGUSR1, 0 };
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif
	memset(&sa, 0, sizeof(sa));

	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	for ( ; *sig; sig++)
		if (sigaction(*sig, &sa, NULL))
			ERROR("%s:%d: sigaction", __func__, __LINE__);
	sigprocmask(SIG_SETMASK, &ss_old, NULL);
}

int svc_exec(int argc, const char *argv[]) {
	int i = 0, j, retval;
	struct svcrun run;
	SV_String_T svc;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%d, %p)\n", __func__, argc, argv);
#endif
	memset(&svc, 0, sizeof(svc));
	memset(&run, 0, sizeof(run));

	run.argc = argc+8+8-(argc % 8);
	run.argv = err_malloc(run.argc*sizeof(void*));
	run.svc = &svc;

	if (argv[0][0] == '/') {
		run.argv[3] = run.path = argv[0];
		run.name = strrchr(argv[0], '/')+1;
	}
	else {
		run.name = argv[0];
		run.path = NULL;
	}
	svc.str = (char*)run.name;
	run.argv[0] = strrchr(SV_RUN_SH, '/')+1U;
	run.argv[4] = argv[1];

	/* setup argv and argc */
	argc -= 2, argv += 2;
	i = 5;
	for ( j = 0; j < argc; j++)
		run.argv[i++] = argv[j];
	run.argv[i] = (char *)0;
	sv_svcdeps_load(NULL);

	retval = svc_cmd(&run);
	if (access(SV_PIDFILE, F_OK))
		atexit(sv_cleanup);
	switch(retval) {
	case -EBUSY:
		return EXIT_SUCCESS;
	case -EINVAL:
		return 3;
	case -ENOENT:
		ERR("inexistant service -- %s\n", run.name);
		return 2;
	case -ECANCELED:
		return 4;
	case -EPERM:
		return 5;
	case SVC_WAITPID:
		run.cld = run.pid;
		retval = svc_waitpid(&run, 0);
		if (retval < 0) return EXIT_FAILURE;
		return retval;
	default:
		if (retval < 0) return EXIT_FAILURE;
		return retval;
	}
}

static void *thread_worker_handler(void *arg)
{
	SV_String_T *svc;
	size_t j = 0, n = 0;
	long int eagain = 0;
	int r;
	struct svcrun_list *p = arg;

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, arg);
#endif

	TAILQ_FOREACH(svc, p->list, entries) {
		p->run[n].name = svc->str;
		p->run[n].argc = p->argc;
		p->run[n].argv = p->argv;
		p->run[n].svc  = svc;
		p->run[n].rl_svc = p;
		p->run[n].dep = svc->data;

		if (!p->run[n].dep)
			p->run[n].dep = p->run[n].svc->data = sv_svcdeps_load(p->run[n].name);
		if (strcmp(p->argv[4], sv_svc_cmd[SV_SVC_CMD_START]) == 0)
			if (p->run[n].dep->status == SV_SVC_STAT_STAR)
				continue;
		if (strcmp(p->argv[4], sv_svc_cmd[SV_SVC_CMD_STOP]) == 0)
			if (p->run[n].dep->status == SV_SVC_MARK_STAR)
				continue;

		r = svc_cmd(&p->run[n]);
		reterr:
		switch(r) {
		case SVC_WAITPID:
			break;
		case -ENOENT:
		case -ECANCELED:
		case -EPERM:
			pthread_mutex_lock(&p->rl_mutex);
			p->rl_count++;
			p->retval++;
			pthread_mutex_unlock(&p->rl_mutex);
			continue;
		case -EBUSY:
		case -EINVAL:
			pthread_mutex_lock(&p->rl_mutex);
			p->rl_count++;
			pthread_mutex_unlock(&p->rl_mutex);
			continue;
		case -EAGAIN: /* fork(3) failure in svc_run() */
		case -ENOMEM:
			if (eagain == -n) { /* something went wrong */
				pthread_mutex_lock(&p->rl_mutex);
				p->retval++;
				pthread_mutex_unlock(&p->rl_mutex);
				goto retval;
			}
			else  eagain = 0;
			eagain += n;
			goto waitpid;
			break;
		default:
			LOG_ERR("%s:%d:service=%s: unhandled return value!!!\n", __func__,
					__LINE__, svc);
			continue;
		}

		if (sv_parallel) {
			/* add the job to queue here to be sure to have any child to wait for */
			if (!n) {
				pthread_rwlock_wrlock(&RL_SVC_LOCK);
				p->rl_next = RL_SVC;
				if (RL_SVC) RL_SVC->rl_prev = p;
				RL_SVC = p;
#ifdef SV_DEBUG
				if (sv_debug) DBG("[add]lid=%u prev=%p %p next=%p\n", p->rl_lid, p->rl_prev, p, p->rl_next);
#endif
				pthread_rwlock_unlock(&RL_SVC_LOCK);
			}
			pthread_rwlock_wrlock(&p->rl_lock);
			p->rl_len++;
			p->rl_job++;
			j = p->rl_job;
			pthread_rwlock_unlock(&p->rl_lock);
			n++;
		}
		else {
			r = svc_waitpid(&p->run[n], 0);
			if (r) p->retval++;
		}
	}

	if (!sv_parallel)
		goto retval;
	else
		goto waitpid;

waitpid:
	if (j) {
#ifdef SV_DEBUG
		if (sv_debug) DBG("lid=%u waiting %ld jobs\n", p->rl_lid, j);
#endif
		pthread_mutex_lock  (&p->rl_mutex);
		pthread_cond_wait(&p->rl_cond, &p->rl_mutex);
		pthread_mutex_unlock(&p->rl_mutex);
	}

	if (eagain > 0) {
		eagain = -eagain;
		r = svc_run(&p->run[n]);
		goto reterr;
	}

retval:
	/* have to remove job from queue */
	pthread_rwlock_rdlock(&p->rl_lock);
	if (p->rl_count != p->rl_siz) {
		pthread_rwlock_wrlock(&RL_SVC_LOCK);
#ifdef SV_DEBUG
		if (sv_debug) DBG("[del]lid=%u prev=%p %p next=%p\n", p->rl_lid, p->rl_prev, p, p->rl_next);
#endif
		if (RL_SVC == p) RL_SVC = p->rl_next;
		else {
			if (p->rl_prev) p->rl_prev->rl_next = p->rl_next;
			if (p->rl_next) p->rl_next->rl_prev = p->rl_prev;
		}
		pthread_rwlock_unlock(&RL_SVC_LOCK);
	}
	pthread_rwlock_unlock(&p->rl_lock);
	pthread_exit((void*)&p->retval);
}

static void thread_signal_action(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)))
{
	int i = -1;
	int serrno = errno;

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%d, %p, %p)\n", __func__, sig, si, ctx);
#endif

	switch(sig) {
	case SIGCHLD:
		switch(si->si_code) {
		case CLD_CONTINUED:
		case CLD_STOPPED:
		case CLD_TRAPPED:
			return;
		}
#ifdef SV_DEBUG
		if (sv_debug) DBG("Caught SIGCHLD from pid=%d\n", si->si_pid);
#endif
		pthread_cond_signal(&RL_PID_COND);
		break;
	case SIGINT:
		i = 1;
	case SIGTERM:
		if (i < 0) i = 3;
	case SIGQUIT:
		if (i < 0) i = 2;
		ERR("caught %s, aborting\n", signame[i]);
		exit(EXIT_FAILURE);
	case SIGUSR1:
		if (!sv_pid)
			break;
		fprintf(stderr, "%s: Aborting!\n", progname);
		exit(EXIT_FAILURE);
	default:
		ERR("caught unknown signal %d\n", sig);
	}
	errno = serrno;
}
__attribute__((__noreturn__)) static void *thread_sigchld_handler(void *arg __attribute__((__unused__)))
{
	int i, r, s;
	size_t len;
	struct svcrun_list *p;
	struct timespec ts = { 0L, 0L };
	pid_t pid;

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif

	if (pthread_sigmask(SIG_BLOCK, &ss_thread, NULL))
		ERROR("%s:%d: pthread_sigmask", __func__, __LINE__);

	for (;;) {
waitcond:
		if (pthread_mutex_lock(&RL_PID_MUTEX)) {
#ifdef SV_DEBUG
			if (sv_debug) DBG("Failed to lock pid mutex (RL_PID_MUTEX)\n", NULL);
#endif
			continue;
		}
		if (pthread_cond_wait(&RL_PID_COND, &RL_PID_MUTEX)) {
#ifdef SV_DEBUG
			if (sv_debug) DBG("Failed to wait pid condition (RL_PID_MUTEX)\n", NULL);
#endif
			continue;
		}
		pthread_mutex_unlock(&RL_PID_MUTEX);
waitpid:
		do {
			pid = waitpid(0, &s, WNOHANG|WUNTRACED);
			if (pid < 0 && errno != EINTR) goto waitcond;
			if (!pid) goto waitcond;
		} while(!WIFEXITED(s) && !WIFSIGNALED(s));
#ifdef SV_DEBUG
		if (sv_debug) DBG("Looking for pid=%d\n", pid);
#endif

rl_svc:
		/* read the first job which could have been changed */
		pthread_rwlock_rdlock(&RL_SVC_LOCK);
		for (p = RL_SVC; p; p = p->rl_next) {
			pthread_rwlock_rdlock(&p->rl_lock);
			len = p->rl_len;
			pthread_rwlock_unlock(&p->rl_lock);
			pthread_mutex_lock(&p->rl_pid);
			for (i = 0; i < len; i++) {
				if (p->run[i].pid != pid) continue;
				pthread_mutex_unlock(&p->rl_pid);
#ifdef SV_DEBUG
				if (sv_debug) DBG("Found pid=%d service=%s\n", pid, p->run[i].name);
#endif
				p->run[i].status = s;
				r = svc_waitpid(&p->run[i], 0);
				pthread_rwlock_wrlock(&p->rl_lock);
				p->rl_count++;
				p->rl_job--;
				len = p->rl_job;
				if (r) p->retval++;
				if (p->rl_count == p->rl_siz) {
					/* remove job from queue */
					pthread_rwlock_unlock(&RL_SVC_LOCK);
					pthread_rwlock_wrlock(&RL_SVC_LOCK);
					if (RL_SVC == p) RL_SVC = p->rl_next;
					else {
						if (p->rl_prev) p->rl_prev->rl_next = p->rl_next;
						if (p->rl_next) p->rl_next->rl_prev = p->rl_prev;
					}
				}
				pthread_rwlock_unlock(&RL_SVC_LOCK);
				pthread_rwlock_unlock(&p->rl_lock);
				if (!len) {
					pthread_mutex_lock(&p->rl_mutex);
					pthread_cond_signal(&p->rl_cond);
					pthread_mutex_unlock(&p->rl_mutex);
				}
				goto waitpid;
			}
			pthread_mutex_unlock(&p->rl_pid);
		}
#ifdef SV_DEBUG
		if (sv_debug) DBG("Failed to found pid=%d\n", pid);
#endif
		pthread_rwlock_unlock(&RL_SVC_LOCK);
		ts.tv_nsec = 10L;
		while (nanosleep(&ts, &ts)) ;
		goto rl_svc;
	}
}
static void thread_signal_setup(void)
{
	int r;
	int *sig = (int []){ SIGCHLD, SIGHUP, SIGINT, SIGTERM, SIGQUIT, SIGUSR1, 0 };
	struct sigaction sa;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif
	memset(&sa, 0, sizeof(sa));
	memcpy(&ss_thread, &ss_child, sizeof(sigset_t));

	sa.sa_sigaction = thread_signal_action;
	sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
	sigemptyset(&sa.sa_mask);

	for ( ; *sig; sig++) {
		do {
			r = sigaddset(&ss_thread, *sig);
			if (r) {
				if (errno == EINTR) continue;
				else ERROR("%s:%d: sigaddset", __func__, __LINE__);
			}
		} while (r);
		do {
			r = sigaction(*sig, &sa, NULL);
			if (r) {
				if (errno == EINTR) continue;
				else ERROR("%s:%d: sigaction", __func__, __LINE__);
			}
		} while (r);
	}
	sigprocmask(SIG_BLOCK, &ss_thread, NULL);
}

__attribute__((__noreturn__)) static void *thread_signal_worker(void *arg __attribute__((__unused__)))
{
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif
	if (pthread_sigmask(SIG_UNBLOCK, &ss_thread, NULL))
		ERROR("%s:%d: pthread_sigmask", __func__, __LINE__);

	for (;;) {
		if (sigsuspend(&ss_null) < 0 && errno != EINTR)
			ERR("%s:%d: sigsuspend: %s\n", __func__, __LINE__, strerror(errno));
	}
}

int svc_execl(SV_StringList_T *list, int argc, const char *argv[])
{
#define HANDLE_ERROR(func)                             \
	do {                                               \
		ERR("%s:%d: " #func "\n", __func__, __LINE__); \
		goto retval;                                   \
	} while(0)
	int r;
	static unsigned int count;
	struct svcrun_list *p, *k;

#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%p, %d, %p)\n", __func__, list, argc, argv);
#endif

	if (!list)
		return -ENOENT;
	if (!argc || !argv)
		return -EINVAL;

	if (RL_COUNT == 1U) {
		if ((r = pthread_attr_init(&RL_SVC_ATTR)))
			HANDLE_ERROR(pthread_attr_init);
		if ((r = pthread_mutexattr_init(&RL_PID_MUTEX_ATTR)))
			HANDLE_ERROR(pthread_mutexattr_init);
		if ((r = pthread_mutexattr_settype(&RL_PID_MUTEX_ATTR, PTHREAD_MUTEX_NORMAL)))
			HANDLE_ERROR(pthread_mutexattr_settype);
		if ((r = pthread_mutex_init(&RL_PID_MUTEX, &RL_PID_MUTEX_ATTR)))
			HANDLE_ERROR(pthread_mutex_init);
		if ((r = pthread_rwlock_init(&RL_SVC_LOCK, NULL)))
			HANDLE_ERROR(pthread_rwlock_init);
		if ((r = pthread_mutex_init(&RL_PID_MUTEX, &RL_PID_MUTEX_ATTR)))
			HANDLE_ERROR(pthread_mutex_init);
		thread_signal_setup();
		if ((r = pthread_create(&RL_SVC_SIGHANDLER_TID, &RL_SVC_ATTR,
						thread_signal_worker, NULL)))
			HANDLE_ERROR(pthread_create);
		if ((r = pthread_create(&RL_PID_TID, &RL_SVC_ATTR,
						thread_sigchld_handler, NULL)))
			HANDLE_ERROR(pthread_create);
	}

	p = err_malloc(sizeof(struct svcrun_list));
	memset(p, 0, sizeof(struct svcrun_list));
	p->rl_lid  = RL_COUNT;
	p->list = list;
	p->argc = argc;
	p->argv = argv;
	if (sv_parallel) {
		p->rl_siz = sv_stringlist_len(list);
		p->rl_siz += p->rl_siz % 4LU ? 4LU-(p->rl_siz % 4LU) : 4LU;
		p->run = err_calloc(sizeof(struct svcrun), p->rl_siz);
		memset(p->run, 0, sizeof(struct svcrun)*p->rl_siz);
		pthread_mutex_init(&p->rl_pid, &RL_PID_MUTEX_ATTR);

		if (count) {
			pthread_rwlock_rdlock(&RL_SVC_LOCK);
			if (RL_SVC) {
				for (k = RL_SVC; k; k = k->rl_next) {
					if (k->rl_lid == RL_COUNT) {
						if (RL_COUNT == UINT_MAX) {
							RL_COUNT = 1U;
							k = RL_SVC;
						}
						else RL_COUNT++;
						continue;
					}
					break;
				}
			}
			else count = 0U;
			pthread_rwlock_unlock(&RL_SVC_LOCK);
		}
		p->rl_lid = RL_COUNT;
	}
	else {
		p->run = err_malloc(sizeof(struct svcrun));
		memset(p->run, 0, sizeof(struct svcrun));
		p->rl_siz = 0LU;
		p->rl_lid= RL_COUNT;
	}
	if (RL_COUNT == UINT_MAX) {
		count++;
		RL_COUNT = 0U;
	}
	RL_COUNT++;

	if ((r = pthread_cond_init(&p->rl_cond, NULL)))
		HANDLE_ERROR(pthread_cond_init);
	if ((r = pthread_mutex_init(&p->rl_mutex, &RL_PID_MUTEX_ATTR)))
		HANDLE_ERROR(pthread_mutex_init);
	if ((r = pthread_rwlock_init(&p->rl_lock, NULL)))
		HANDLE_ERROR(pthread_rwlock_init);
	if ((r = pthread_create(&p->rl_tid, &RL_SVC_ATTR,
					thread_worker_handler, p)))
		HANDLE_ERROR(pthread_create);
	if ((r = pthread_join(p->rl_tid, (void*)NULL)))
		HANDLE_ERROR(pthread_join);
retval:
	if (r)
		r = -r;
	else
		r = p->retval;
	thread_worker_cleanup(p);
	return r;
#undef HANDLE_ERROR
}

static void thread_worker_cleanup(struct svcrun_list *p)
{
	int i;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, p);
#endif

	for (i = 0; i < p->rl_len; i++) {
		free((void*)p->run[i].ARGV);
		free((void*)p->run[i].path);
	}
	free((void*)p->run);

	pthread_cond_destroy(&p->rl_cond);
	pthread_mutex_destroy(&p->rl_mutex);
	pthread_rwlock_destroy(&p->rl_lock);
	pthread_mutex_destroy(&p->rl_pid);
	free((void*)p);
}
