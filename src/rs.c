/*
 * Copyright (c) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs.c  0.13.0 2016/12/30
 */

#include "sv.h"
#include "sv-deps.h"
#include <stdio.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>

static struct svcrun *RUN;

const char *progname;
static const char *applet = "rs";

/* !!! likewise (service command) !!! */
const char *const sv_svc_cmd[] = { "stop", "start",
	"add", "del", "desc", "remove", "restart", "status", "zap"
};

static const char *const environ_whitelist[] = {
	"PATH", "SHELL", "SHLVL", "USER", "HOME", "TERM", "TMP", "TMPDIR",
	"LANG", "LC_ALL", "LC_ADDRESS", "LC_COLLATE", "LC_CTYPE", "LC_NUMERIC",
	"LC_MEASUREMENT", "LC_MONETARY", "LC_MESSAGES", "LC_NAME", "LC_PAPER",
	"LC_IDENTIFICATION", "LC_TELEPHONE", "LC_TIME", "PWD", "OLDPWD", "LOGNAME",
	"COLUMNS", "LINES", "UID", "GID", "EUID", "EGID", NULL
};
static const char *environ_list[] = {
	"SVC_DEBUG", "SVC_WAIT", "SV_RUNLEVEL", "SV_STAGE",
	"SV_SYSBOOT_LEVEL", "SV_SHUTDOWN_LEVEL", "SV_VERSION", NULL
};

/* execute a service command;
 * @run: an svcrun structure;
 * @return: -errno on errors,
 */
int svc_cmd(struct svcrun *run);
static int svc_run(struct svcrun *run);
static int svc_waitpid(struct svcrun *run, int flags);
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
int svc_mark(const char *svc, int status, const char *what);

/*
 * lock file for service to start/stop
 * @svc: service name;
 * @lock_fd: SVC_LOCK to lock, lock_fd >= 0 to unlock;
 * @timeout: timeout to use to poll the lockfile (SVC_WAIT_SECS);
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
static int svc_wait(const char *svc, int timeout, int lock_fd);
#define SVC_WAIT_SECS 60    /* default delay */
#define SVC_WAIT_MSEC 1000  /* interval for displaying warning */
#define SVC_WAIT_POLL 100   /* poll interval */

/*
 * execute a service with the appended arguments
 */
int svc_exec(int argc, const char *argv[]);

/*
 * execute a service list (called from svc_stage())
 * @return 0 on success or number of failed services
 */
int svc_execl(SV_StringList_T *list, int argc, const char *argv[]);

/*
 * remove service temporary files
 */
static void svc_zap(const char *svc);

/* signal handler/setup */
static void sv_sighandler(int sig);
static void sv_sigsetup(void);
void svc_sigsetup(void);
extern sigset_t ss_child, ss_full, ss_old;

/*
 * generate a default environment for service
 */
static const char **svc_environ;
static void svc_env(void);

int svc_cmd(struct svcrun *run)
{
	static char *deps[2] = { "--deps", "--nodeps" };
	static char *type[2] = { "--rs", "--sv" };
	static struct stat st_dep = { .st_mtime = 0 };
	struct stat st_buf;
	int retval;
	int i;
	size_t len = 0;
	const char *cmd = run->argv[4];
	char *path, buf[512];

	if (!st_dep.st_mtime) {
		stat(SV_SVCDEPS_FILE, &st_dep);
		svc_env();
	}
	run->cmd = -1;
	run->envp = svc_environ;
	run->dep = NULL;
	run->mark = 0;

	if (strcmp(cmd, sv_svc_cmd[SV_SVC_CMD_START]) == 0) {
		run->mark = SV_SVC_STAT_STAR;
		if (svc_state(run->name, SV_SVC_STAT_STAR) ||
			svc_state(run->name, SV_SVC_STAT_PIDS)) {
			LOG_WARN("%s: service started\n", run->name);
			return -EBUSY;
		}
		run->cmd  = SV_SVC_CMD_START;
	}
	else if (strcmp(cmd, sv_svc_cmd[SV_SVC_CMD_STOP]) == 0) {
		run->mark = SV_SVC_MARK_STAR;
		if (!(svc_state(run->name, SV_SVC_STAT_STAR) ||
			  svc_state(run->name, SV_SVC_STAT_PIDS))) {
			LOG_WARN("%s: service stopped\n", run->name);
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
		if (svc_state(run->name, SV_SVC_STAT_DOWN)) {
			printf("%-32s: %s: down\n", run->name, cmd);
			return 8;
		}
		else if (svc_state(run->name, SV_SVC_STAT_STAR) ||
			svc_state(run->name, SV_SVC_STAT_PIDS)) {
			printf("%-32s: %s: started\n", run->name, cmd);
			return 0;
		}
		else if (svc_state(run->name, SV_SVC_STAT_FAIL)) {
			snprintf(buf, sizeof(buf), "%s/%s", SV_TMPDIR_FAIL, run->name);
			stat(buf, &st_buf);
			if ((i = open(buf, O_RDONLY)) > 0) {
				if (read(i, buf, sizeof(buf)) > 0) {
					printf("%-32s: %s: (%s command) failed at %s", run->name,
							cmd, buf, ctime(&st_buf.st_mtime));
				}
				close(i);
			}
			else
				printf("%-32s: %s: failed at %s", run->name, cmd,
					ctime(&st_buf.st_mtime));
			return 16;
		}
		else if (svc_state(run->name, SV_SVC_STAT_WAIT)) {
			snprintf(buf, sizeof(buf), "%s/%s", SV_TMPDIR_FAIL, run->name);
			stat(buf, &st_buf);
			if ((i = open(buf, O_RDONLY)) > 0) {
				if (read(i, buf, sizeof(buf)) > 0) {
					printf("%-32s: %s: waiting (%s command) since %s", run->name,
							cmd, buf, ctime(&st_buf.st_ctime));
				}
				close(i);
			}
			else
				printf("%-32s: %s: waiting since %s", run->name, cmd,
					ctime(&st_buf.st_ctime));
			return 32;
		}
		else {
			printf("%-32s: %s: stopped\n", run->name, cmd);
			return 3;
		}
	}

	run->ARGV = err_calloc(run->argc, sizeof(void*));
	for (i = 0; i <= run->argc; i++)
		run->ARGV[i] = run->argv[i];
	/* get service path */
	if (!run->path) {
		snprintf(buf, sizeof(buf), "%s/%s", SV_SVCDIR, run->name);
		len = strlen(buf)+1;
		run->path = memcpy(err_malloc(len), buf, len);
	}
	if (stat(run->path, &st_buf) < 0) {
		retval = -ENOENT;
		goto reterr;
	}

	/* get service status */
	switch(run->cmd) {
	case SV_SVC_CMD_ADD:
	case SV_SVC_CMD_DEL:
		if (sv_stage < 0) {
			fprintf(stderr, "%s: runlevel argument is required\n", progname);
			fprintf(stderr, "Usage: %s -(0|1|3|4|5) %s %s\n", progname,
					run->name, sv_svc_cmd[run->cmd]);
			retval = 1;
			goto reterr;
		}

		path = err_malloc(512);
		snprintf(path, 512, "%s/.%s/%s", SV_SVCDIR, sv_runlevel[sv_stage],
				run->name);
		if (!access(path, F_OK)) {
			if (run->cmd == SV_SVC_CMD_DEL)
				unlink(path);
			retval = 0;
			goto reterr;
		}

		if (symlink(run->path, path)) {
			ERR("%s: Failed to add service: %s\n", run->name, strerror(errno));
			retval = 1;
		}
		else
			retval = 0;
		goto reterr;
		break;
	}

	/* get service type */
	if (S_ISDIR(st_buf.st_mode))
		run->ARGV[1] = type[1];
	else
		run->ARGV[1] = type[0];
	run->ARGV[2] = deps[0];
	run->ARGV[3] = run->path;
	if (!run->svc->data)
		run->svc->data = sv_svcdeps_load(run->name);
	run->dep = run->svc->data;

	/* check service mtime */
	if (st_buf.st_mtime > st_dep.st_mtime)
		LOG_WARN("%s was updated -- `scan' command might be necessary?\n",
				run->name);

	/* setup dependencies */
	if (run->cmd == SV_SVC_CMD_START && svc_deps && run->dep) {
		retval = svc_depend(run);
		if (retval == -ENOENT)
			;
		else if (retval) {
			LOG_ERR("%s: Failed to set up service dependencies\n", run->name);
			svc_mark(run->name, SV_SVC_STAT_FAIL, cmd);
			retval = -ECANCELED;
			goto reterr;
		}
		else
			run->ARGV[2] = deps[1];
	}

	if (!svc_quiet)
		svc_log("[%s] service %s...\n", run->name, cmd);

	return svc_run(run);
reterr:
	free(run->ARGV);
	return retval;
}

static int svc_run(struct svcrun *run)
{
	if (sv_stringlist_find(run->dep->deps[SV_DEPS_KWD], "timeout"))
		run->dep->timeout = -1;
	else
		run->dep->timeout = SVC_WAIT_SECS;
	run->status = -1;
	run->sig = 0;

	/* block signal before fork() */
	sigprocmask(SIG_SETMASK, &ss_full, NULL);
	run->pid = fork();
	if (run->pid > 0) { /* parent */
		/* restore signal mask */
		sigprocmask(SIG_SETMASK, &ss_child, NULL);
		return SVC_WAITPID;
	}
	else if (run->pid == 0) /* child */
		goto runsvc;
	else {
		ERR("%s:%d: Failed to fork(): %s\n", __func__, __LINE__, strerror(errno));
		return -errno;
	}

runsvc:
	/* restore signal mask */
	sigprocmask(SIG_SETMASK, &ss_child, NULL);

	/* run a chid process to exec to the service; failure mean _exit(VALUE)! */
	if ((run->lock = svc_lock(run->name, SVC_LOCK, SVC_WAIT_SECS)) < 0) {
		LOG_ERR("%s: Failed to setup lockfile for service\n", run->name);
		_exit(ENOLCK);
	}
	write(run->lock, run->argv[4], strlen(run->argv[4])+1);

	/* close the lockfile to be able to mount rootfs read-only */
	if (sv_stage == SV_SHUTDOWN_LEVEL && run->cmd == SV_SVC_CMD_START)
		close(run->lock);

	/* supervise the service */
	if (sv_nohang) {
		/* block signal before fork() */
		sigprocmask(SIG_SETMASK, &ss_full, NULL);

		if ((run->cld = fork()) > 0)
			goto supervise;
		else if (run->cld < 0) {
			ERR("%s:%d: Failed to fork(): %s\n", __func__, __LINE__,
					strerror(errno));
			_exit(errno);
		}
		else
			/* restore signal mask */
			sigprocmask(SIG_SETMASK, &ss_old, NULL);
	}

	/* restore previous default signal handling */
	svc_sigsetup();

	execve(SV_RUNSCRIPT, (char *const*)run->ARGV, (char *const*)run->envp);
	_exit(255);
supervise:
	RUN = run;
	/* restore signal mask */
	sigprocmask(SIG_SETMASK, &ss_child, NULL);
	/* setup SIGCHILD,SIGALRM and unblock SIGCHILD */
	sv_sigsetup();

	/* setup a timeout and wait for the child */
	if (run->dep->timeout > 0)
		alarm(run->dep->timeout);
	while (run->cld)
		sigsuspend(&ss_old);
	_exit(run->status);
}

static int svc_waitpid(struct svcrun *run, int flags)
{
	int status = 0;
	pid_t pid;

	/* do this hack to only mark children status */
	if (run->status != -1) {
		pid = run->pid;
		status = run->status;
		goto status;
	}
	do {
		pid = waitpid(run->cld, &status, flags);
		if (pid < 0) {
			if (errno != EINTR)
				return -1;
		}
		if (pid == 0)
			return SVC_WAITPID;
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));

status:
	if (run->lock)
		close(run->lock);
	if (pid > 0 && WIFEXITED(status))
		run->status =  WEXITSTATUS(status);
	run->cld = 0;
	svc_mark(run->name, SV_SVC_MARK_WAIT, NULL);
	if (!svc_quiet)
		svc_end(run->name, run->status);
	if (run->status)
		svc_mark(run->name, SV_SVC_STAT_FAIL, run->argv[4]);
	else if (run->mark) {
		svc_mark(run->name, run->mark, NULL);
		if (run->dep && run->dep->virt)
			svc_mark(run->dep->virt, run->mark, NULL);
	}

	return run->status;
}

static int svc_depend(struct svcrun *run)
{
	int type, val, retval = 0;
	int p = 0;
	SV_DepTree_T deptree = { NULL, NULL, 0, 0 };

	if (!run->dep)
		return -ENOENT;

	/* skip before deps type */
	for (type = SV_DEPS_USE; type <= SV_DEPS_NEED; type++) {
		if (TAILQ_EMPTY(run->dep->deps[type]))
			continue;
		/* build a deptree to avoid segfault because cyclical dependencies */
		deptree.list = run->dep->deps[type];
		svc_deptree_load(&deptree);
		while (p >= 0 && p < deptree.size) { /* PRIORITY_LEVEL_LOOP */
			if (!TAILQ_EMPTY(deptree.tree[p]))
				val = svc_execl(deptree.tree[p], run->argc, run->argv);
				--p;
		} /* PRIORITY_LEVEL_LOOP */
		sv_deptree_free(&deptree);
		if (val > 0 && type == SV_DEPS_NEED)
			retval = val;
	}
	return retval;
}

static void svc_env(void)
{
	size_t len = 8, size = 1024;
	char *env, *ptr;
	FILE *fp;
	int i = 0, j;

	if (svc_environ)
		return;
	env = err_malloc(size);
	svc_environ = err_calloc(len, sizeof(void *));

	if (!getenv("COLUMNS")) {
		sprintf(env, "%d", get_term_cols());
		setenv("COLUMNS", env, 1);
	}
	free(env);

	for (j = 0; environ_whitelist[j]; j++) {
		ptr = getenv(environ_whitelist[j]);
		if (ptr) {
			env = err_malloc(size);
			snprintf(env, size, "%s=%s", environ_whitelist[j], ptr);
			svc_environ[i++] = err_realloc(env, strlen(env)+1);
			if (i == len) {
				len += 8;
				svc_environ = err_realloc(svc_environ, len*sizeof(void*));
			}
		}
	}
	if (i == len) {
		len += 8;
		svc_environ = err_realloc(svc_environ, len*sizeof(void*));
	}
	svc_environ[i++] = (char *)0;

	if (!access(SV_ENVIRON, F_OK))
		return;
	if (!(fp = fopen(SV_ENVIRON, "w"))) {
		ERR("Failed to open %s: %s\n", SV_ENVIRON, strerror(errno));
		return;
	}
	env = err_malloc(size);
	for (j = 0; environ_list[j]; j++)
		if ((ptr = getenv(environ_list[j])))
			fprintf(fp, "%s=%s\n", environ_list[j], ptr);
	free(env);
}

static int svc_lock(const char *svc, int lock_fd, int timeout)
{
	char f_path[512];
	int fd;
	int w;
	mode_t m;
	static int f_flags = O_NONBLOCK | O_CREAT | O_WRONLY;
	static mode_t f_mode = 0644;

	if (svc == NULL) {
		errno = ENOENT;
		return -ENOENT;
	}
	snprintf(f_path, sizeof(f_path), "%s/%s", SV_TMPDIR_WAIT, svc);

	if (lock_fd == SVC_LOCK) {
		w = !access(f_path, F_OK);
		m = umask(0);
		fd = open(f_path, f_flags, f_mode);
		/* got different behaviours of open(3p)/O_CREAT when the file is locked
		 * (try that mode | S_ISGID hack of SVR3 to enable mandatory lock?)
		 */
		if (w && fd < 0) {
			switch (errno) {
				case EWOULDBLOCK:
				case EEXIST:
					if (svc_wait(svc, timeout, 0) < 0)
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
				if (svc_wait(svc, timeout, lock_fd) > 0)
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

static int svc_wait(const char *svc, int timeout, int lock_fd)
{
	int i, j;
	int err;
	int msec = SVC_WAIT_MSEC, nsec, ssec = 10;
	if (timeout < ssec) {
		nsec = timeout;
		msec = 1000*timeout;
	}
	else
		nsec = timeout % ssec;
	nsec = nsec ? nsec : ssec;

	for (i = 0; i < timeout; ) {
		for (j = SVC_WAIT_POLL; j <= msec; j += SVC_WAIT_POLL) {
			if (svc_state(svc, SV_SVC_STAT_WAIT) < 1)
				return 0;
			/* add some insurence for failed services */
			if (lock_fd) {
				err = errno;
				if (flock(lock_fd, LOCK_EX|LOCK_NB) == 0)
					return lock_fd;
				errno = err;
			}
			/* use poll(3p) as a milliseconds timer (sleep(3) replacement) */
			if (poll(0, 0, SVC_WAIT_POLL) < 0)
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
	char path[512];
	char *dirs[] = { SV_TMPDIR_DOWN, SV_TMPDIR_FAIL, SV_TMPDIR_STAR,
		SV_TMPDIR_WAIT, SV_TMPDIR "/ENVS", SV_TMPDIR "/OPTS", NULL };

	for (i = 0; dirs[i]; i++) {
		snprintf(path, sizeof(path), "%s/%s", dirs[i], svc);
		if (!access(path, F_OK))
			unlink(path);
	}
}

int svc_mark(const char *svc, int status, const char *what)
{
	char path[512], *ptr;
	int fd;
	mode_t m;

	if (!svc) {
		errno = ENOENT;
		return -1;
	}

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
			errno = EINVAL;
			return -1;
	}

	snprintf(path, sizeof(path), "%s/%s", ptr, svc);
	switch (status) {
		case SV_SVC_STAT_DOWN:
		case SV_SVC_STAT_FAIL:
		case SV_SVC_STAT_STAR:
		case SV_SVC_STAT_WAIT:
			m = umask(0);
			fd = open(path, O_CREAT|O_WRONLY|O_NONBLOCK, 0644);
			umask(m);
			if (fd > 0) {
				if (what)
					write(fd, what, strlen(what)+1);
				close(fd);
				return 0;
			}
			return -1;
		default:
			if (!access(path, F_OK))
				return unlink(path);
			else
				return 0;
	}
}

static int svc_state(const char *svc, int status)
{
	char path[512], *ptr = NULL;

	if (!svc) {
		errno = ENOENT;
		return 0;
	}

	switch(status) {
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
			errno = EINVAL;
			return 0;
	}
	snprintf(path, sizeof(path), "%s/%s", ptr, svc);
	if (access(path, F_OK)) return 0;
	return 1;
}

static void sv_sighandler(int sig)
{
	int i = -1, serrno = errno;
	static const char signame[][8] = { "SIGINT", "SIGQUIT", "SIGKILL",
		"SIGTERM" };

	switch (sig) {
	case SIGALRM:
		if (RUN) {
			if (!RUN->sig)
				i = 3, RUN->sig = SIGTERM;
			else if (RUN->sig == SIGTERM)
				i = 1, RUN->sig = SIGQUIT;
			else
				i = 2, RUN->sig = SIGKILL;
			alarm(RUN->dep->timeout);
			LOG_WARN("sending %s to process PID=%d (service %s)!!!\n", signame[i],
					RUN->cld, RUN->name);
			kill(RUN->cld, RUN->sig);
		}
		break;
	case SIGCHLD:
		if (RUN)
			svc_waitpid(RUN, WNOHANG);
		break;
	default:
		ERR("caught unknown signal %d\n", sig);
	}

	/* restore errno */
	errno = serrno;
}

static void sv_sigsetup(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));

	sa.sa_handler = sv_sighandler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	sigprocmask(SIG_UNBLOCK, &ss_child, NULL);
}

void svc_sigsetup(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));

	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGHUP , &sa, NULL);
	sigaction(SIGINT , &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
	sigprocmask(SIG_SETMASK, &ss_old, NULL);
}

int svc_exec(int argc, const char *argv[]) {
	int i = 0, j, retval;
	struct svcrun run;
	SV_String_T svc = { NULL, NULL, NULL, NULL };

	run.argc = 8*(argc/8)+8+ (argc%8) > 4 ? 8 : 0;
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
	run.argv[0] = "runscript";
	run.argv[4] = argv[1];

	/* setup argv and argc */
	argc -= 2, argv += 2;
	i = 5;
	for ( j = 0; j < argc; j++)
		run.argv[i++] = argv[j];
	run.argv[i] = (char *)0;
	sv_svcdeps_load(NULL);
	/* clean up on if necessary */
	if (access(SV_ENVIRON, F_OK))
		atexit(sv_cleanup);

	retval = svc_cmd(&run);
	switch(retval) {
	case -EBUSY:
	case -EINVAL:
		return EXIT_SUCCESS;
	case -ENOENT:
		ERR("inexistant service -- %s\n", run.name);
		return 2;
	case -ECANCELED:
		return 4;
	case SVC_WAITPID:
		run.cld = run.pid;
		return svc_waitpid(&run, 0);
	default:
		return retval;
	}
}

int svc_execl(SV_StringList_T *list, int argc, const char *argv[])
{
	SV_String_T *svc;
	size_t c, len, n = 0;
	int eagain = 0;
	int i, r, retval = 0, status;
	struct svcrun **run;

	if (list == NULL)
		return -ENOENT;
	if (argv == NULL)
		return -EINVAL;

	if (sv_parallel)
		len = sv_stringlist_len(list);
	else
		len = 1;
	run = err_malloc(len*sizeof(void*));
	memset(run, 0, len*sizeof(void*));
	*run = err_malloc(sizeof(struct svcrun));

	TAILQ_FOREACH(svc, list, entries) {
		run[n]->name = svc->str;
		run[n]->argc = argc;
		run[n]->argv = argv;
		run[n]->svc  = svc;
		run[n]->path = NULL;

		r = svc_cmd(run[n]);
		reterr:
		switch(r) {
		case SVC_WAITPID:
			break;
		case -ENOENT:
		case -ECANCELED:
			retval++;
			continue;
		case -EBUSY:
		case -EINVAL:
			continue;
		case -EAGAIN: /* fork(3) failure in svc_run() */
		case -ENOMEM:
			if (eagain == n) { /* something went wrong */
				free(run[n]->ARGV);
				free(run[n]);
				retval++;
				goto retval;
			}
			else  eagain = 0;
			eagain += n;
			goto waitpid;
		eagain:
			r = svc_run(run[n]);
			goto reterr;
			break;
		}

		if (sv_parallel)
			n++;
		else {
			r = svc_waitpid(*run, 0);
			if (r) retval++;
		}
		if (n < len)
			run[n] = err_malloc(sizeof(struct svcrun));
	}

	if (!sv_parallel)
		goto retval;
	else
		goto waitpid;

waitpid:
	c = n;
	while (c) {
		/* multiple calls of this function may have children to wait for;
		 * and avoid wasting cpu time in repeatedly calling waipid(3) */
		r = waitpid(0, &status, 0);
		if (r < 0) {
			if (errno == ECHILD) {
				LOG_ERR("%s: waitpid(): no child to wait for!!!\n", __func__);
				goto retval;
			}
			continue;
		}
		for (i = 0; i < n; i++)
			if (run[i] && run[i]->pid == r)
				break;
		if (i > n)
			continue;

		if (sv_nohang) {
			if (WIFEXITED(status) && !WEXITSTATUS(status))
				;
			else
				retval++;
		}
		else {
			run[i]->status = status;
			r = svc_waitpid(run[i], WNOHANG);
			if (r == SVC_WAITPID)
				continue;
			if (r == -1 && errno == ECHILD)
				LOG_ERR("no child for %s service!!!\n", run[i]->name);
			if (r) retval++;
		}
		free((void*)run[i]->ARGV);
		free((void*)run[i]->path);
		free(run[i]);
		run[i] = NULL;
		c--;
	}
	if (eagain)
		goto eagain;

retval:
	if (*run)
		free(*run);
	free(run);
	return retval;
}
