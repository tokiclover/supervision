/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs.c
 */

#include "rs.h"
#include "rs-deps.h"
#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>

#define VERSION "0.12.6.4"
#define RS_RUNSCRIPT SV_LIBDIR "/sh/runscript"

#define SV_TMPDIR_DOWN SV_TMPDIR "/down"
#define SV_TMPDIR_FAIL SV_TMPDIR "/fail"
#define SV_TMPDIR_PIDS SV_TMPDIR "/pids"
#define SV_TMPDIR_STAR SV_TMPDIR "/star"
#define SV_TMPDIR_WAIT SV_TMPDIR "/wait"

#define SV_PIDFILE SV_TMPDIR "/sv.pid"

struct svcrun {
	RS_String_T *svc;
	const char *name;
	const char *path;
	pid_t pid;
	int lock;
	int argc;
	const char **argv;
	const char **envp;
};

int sv_stage    = -1;
int sv_runlevel = -1;
pid_t sv_pid;
static int svc_deps  = 1;
static int svc_quiet = 1;
static RS_DepTree_T DEPTREE = { NULL, NULL, 0, 0 };

/* list of service to start/stop before|after a stage */
static const char *const rs_init_stage[][4] = {
	{ "clock", "hostname", NULL },
};

/* !!! order matter (defined constant/enumeration) !!! */
const char *const sv_runlevel_name[] = { "shutdown", "single", "nonetwork", "default",
	"sysinit", "boot", "reboot", NULL
};
const char *progname;

/* status command to issue to svc_{mark,state} when the STAT querry the status
 * and MARK command remove the status; so the command is only valid with svc_mark
 */
#define RS_SVC_STAT_FAIL 'f'
#define RS_SVC_MARK_FAIL 'F'
#define RS_SVC_STAT_DOWN 'd'
#define RS_SVC_MARK_DOWN 'D'
#define RS_SVC_STAT_PIDS 'p'
#define RS_SVC_STAT_STAR 's'
#define RS_SVC_MARK_STAR 'S'
#define RS_SVC_STAT_WAIT 'w'
#define RS_SVC_MARK_WAIT 'W'

enum {
	RS_SVC_CMD_STOP,
#define RS_SVC_CMD_STOP    RS_SVC_CMD_STOP
	RS_SVC_CMD_START,
#define RS_SVC_CMD_START   RS_SVC_CMD_START
	RS_SVC_CMD_ADD,
#define RS_SVC_CMD_ADD     RS_SVC_CMD_ADD
	RS_SVC_CMD_DEL,
#define RS_SVC_CMD_DEL     RS_SVC_CMD_DEL
	RS_SVC_CMD_DESC,
#define RS_SVC_CMD_DESC    RS_SVC_CMD_DESC
	RS_SVC_CMD_REMOVE,
#define RS_SVC_CMD_REMOVE  RS_SVC_CMD_REMOVE
	RS_SVC_CMD_RESTART,
#define RS_SVC_CMD_RESTART RS_SVC_CMD_RESTART
	RS_SVC_CMD_STATUS,
#define RS_SVC_CMD_STATUS  RS_SVC_CMD_STATUS
	RS_SVC_CMD_ZAP
#define RS_SVC_CMD_ZAP     RS_SVC_CMD_ZAP
};
/* !!! likewise (service command) !!! */
static const char *const rs_svc_cmd[] = { "stop", "start",
	"add", "del", "desc", "remove", "restart", "status", "zap"
};

static const char *shortopts = "Dg0S1N23Rqvh";
static const struct option longopts[] = {
	{ "nodeps",   0, NULL, 'D' },
	{ "debug",    0, NULL, 'g' },
	{ "sysinit",  0, NULL, '0' },
	{ "single",   0, NULL, 'S' },
	{ "boot",     0, NULL, '1' },
	{ "nonetwork",0, NULL, 'N' },
	{ "default",  0, NULL, '2' },
	{ "shutdown", 0, NULL, '3' },
	{ "reboot",   0, NULL, 'R' },
	{ "quiet",    0, NULL, 'q' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Disable dependencies",
	"Enable debug mode",
	"Select sysinit     run level",
	"Select single user run level",
	"Select boot        run level",
	"Select nonetwork   run level",
	"Select default     run level",
	"Select shutdown    run level",
	"Select reboot      run level",
	"Enable quiet mode",
	"Show help and exit",
	"Show version and exit",
	NULL
};

static const char *const env_list[] = {
	"PATH", "SHELL", "SHLVL", "USER", "HOME", "TERM", "TMP", "TMPDIR",
	"LANG", "LC_ALL", "LC_ADDRESS", "LC_COLLATE", "LC_CTYPE", "LC_NUMERIC",
	"LC_MEASUREMENT", "LC_MONETARY", "LC_MESSAGES", "LC_NAME", "LC_PAPER",
	"LC_IDENTIFICATION", "LC_TELEPHONE", "LC_TIME", "PWD", "OLDPWD", "LOGNAME",
	"COLUMNS", "LINES", "SVC_DEBUG", "SVC_DEPS", "SVC_WAIT",
	"UID", "GID", "EUID", "EGID",
	"SV_RUNLEVEL", "SV_STAGE", NULL
};

__NORETURN__ static void help_message(int exit_val);

#define SVC_CMD_WAIT 2
#define SVC_RET_WAIT 256
/* execute a service command;
 * @run: an svcrun structure;
 * @return: -errno on errors,
 *   child return value if SVC_CMD_WAIT is or-ed to the flags,
 *   SVC_RET_WAIT otherwise;
 */
static int svc_cmd(struct svcrun *run, int flags);

/* tiny function to print end string like the shell end() counterpart */
static int svc_end(const char *svc, int status);

/*
 * setup service dependencies
 * @run: svcrun structure;
 * @return: 0 on succsess,
 *        > 0 for non fatals errors
 *        < 0 for fatals errors;
 */
static int svc_depend(struct svcrun *run);

/* simple function to help debug info in a file (vfprintf(3) clone) */
static FILE *logfp;
static int logfd, rs_debug;

static int svc_log(const char *fmt, ...);
#define LOG_ERR(fmt, ...)  svc_log("ERROR: %s: " fmt, progname, __VA_ARGS__)
#define LOG_WARN(fmt, ...) svc_log( "WARN: %s: " fmt, progname, __VA_ARGS__)
#define LOG_INFO(fmt, ...) svc_log( "INFO: %s: " fmt, progname, __VA_ARGS__)

#define RS_LOGFILE SV_TMPDIR "/rs.log"

/*
 * bring system to a named level or stage
 * @cmd (start|stop|NULL); NULL is the default command
 */
static void svc_stage(const char *cmd);

/*
 * querry service status
 * @svc: service name;
 * @status: int value [defrs];
 * @return: true/false;
 */
static int svc_state(const char *svc, int status);

/*
 * execute a service list (called from svc_stage())
 * to finish/start particular stages
 * @return 0 on success or number of failed services
 */
static int svc_stage_command(int stage, int argc, const char *argv[], const char *envp[]);

/*
 * set service status
 * @svc: service name; or @run instead;
 * @status: int value [dfrs]
 * @return: 0 on success;
 */
static int svc_mark(const char *svc, int status, const char *what);

/* simple rc compatible runlevel handler*/
static void svc_level(void);
static char *get_cmdline_entry(const char *entry);

/* simple helper to set/get runlevel
 * @level: runlevel to set, or NULL to get the current runlevel;
 * @return: current runlevel;
 */
static const char *svc_runlevel(const char *level);

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
__NORETURN__ static int svc_exec(int argc, char *argv[]);

/*
 * execute a service list (called from svc_stage())
 * @return 0 on success or number of failed services
 */
static int svc_execl(RS_StringList_T *list, int argc, const char *argv[],
		const char *envp[]);

/*
 * remove service temporary files
 */
static void svc_zap(const char *svc);

/* signal handler/setup */
static void sv_sighandler(int sig);
static void sv_sigsetup(void);
static void svc_sigsetup(void);
static sigset_t ss_child, ss_full, ss_old;

/*
 * generate a default environment for service
 */
static const char **svc_environ;
static const char **svc_env(void);

__NORETURN__ static void help_message(int exit_val)
{
	int i;

	printf("usage: rs [OPTIONS] SERVICE COMMAND\n");
	printf("  COMMAND: add|del|remove|restart|start|stop|status|zap|...\n");
	printf("  OPTIONS: [OPTS] SERVICE COMMAND\n");
	for ( i = 0; i < 2; i++)
		printf("    -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);
	printf("  OPTIONS: [OPTS] stage [start|stop]\n");
	for ( i = 2; longopts_help[i]; i++)
		printf("    -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);

	exit(exit_val);
}

static int svc_cmd(struct svcrun *run, int flags)
{
	static char *deps[2] = { "--deps", "--nodeps" };
	static char *type[2] = { "--rs", "--sv" };
	static int setup = 1;
	static struct stat st_dep;
	struct stat st_buf;
	int state = 0, status, command = -1, retval = 1;
	pid_t pid;
	int i;
	const char **argv = NULL;
	const char *cmd = run->argv[4];
	char *path, buf[512];
	RS_SvcDeps_T *d = run->svc->data;

	if (setup) {
		setup = 0;
		stat(SV_SVCDEPS_FILE, &st_dep);
	}

	if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_START]) == 0)
		state   = RS_SVC_STAT_STAR,
		status  = svc_state(run->name, RS_SVC_STAT_STAR) ||
			svc_state(run->name, RS_SVC_STAT_PIDS),
		command = RS_SVC_CMD_START;
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_STOP]) == 0)
		state   = RS_SVC_MARK_STAR,
		status  = svc_state(run->name, RS_SVC_STAT_STAR) ||
			svc_state(run->name, RS_SVC_STAT_PIDS),
		command = RS_SVC_CMD_STOP;
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_ADD]) == 0)
		command = RS_SVC_CMD_ADD;
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_DEL]) == 0)
		command = RS_SVC_CMD_DEL;
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_ZAP]) == 0) {
		svc_zap(run->name);
		return 0;
	}
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_STATUS]) == 0) {
		if (svc_state(run->name, RS_SVC_STAT_DOWN)) {
			printf("%s: %s: down\n", run->name, cmd);
			return 8;
		}
		else if (svc_state(run->name, RS_SVC_STAT_STAR) ||
			svc_state(run->name, RS_SVC_STAT_PIDS)) {
			printf("%s: %s: started\n", run->name, cmd);
			return 0;
		}
		else if (svc_state(run->name, RS_SVC_STAT_FAIL)) {
			snprintf(buf, sizeof(buf), "%s/%s", SV_TMPDIR_FAIL, run->name);
			stat(buf, &st_buf);
			if ((i = open(buf, O_RDONLY)) > 0) {
				if (read(i, buf, sizeof(buf)) > 0) {
					printf("%s: %s: (%s command) failed at %s", run->name,
							cmd, buf, ctime(&st_buf.st_mtim.tv_sec));
				}
			}
			else
				printf("%s: %s: failed at %s", run->name, cmd,
					ctime(&st_buf.st_mtim.tv_sec));
			return 16;
		}
		else if (svc_state(run->name, RS_SVC_STAT_WAIT)) {
			snprintf(buf, sizeof(buf), "%s/%s", SV_TMPDIR_FAIL, run->name);
			stat(buf, &st_buf);
			if ((i = open(buf, O_RDONLY)) > 0) {
				if (read(i, buf, sizeof(buf)) > 0) {
					printf("%s: %s: waiting (%s command) since %s", run->name,
							cmd, buf, ctime(&st_buf.st_ctim.tv_sec));
				}
			}
			else
				printf("%s: %s: waiting since %s", run->name, cmd,
					ctime(&st_buf.st_ctim.tv_sec));
			return 32;
		}
		else {
			printf("%s: %s: stopped\n", run->name, cmd);
			return 3;
		}
	}

	/* this is done before because of a possible virtual provider */
	if ((command == RS_SVC_CMD_START) && status) {
		LOG_WARN("%s: service started\n", run->name);
		return -EBUSY;
	}
	else if ((command == RS_SVC_CMD_STOP) && !status) {
		LOG_WARN("%s: service stopped\n", run->name);
		return -EINVAL;
	}

	argv = err_calloc(run->argc, sizeof(void*));
	for (i = 0; i <= run->argc; i++)
		argv[i] = run->argv[i];
	/* get service path */
	if (!run->path) {
		argv[3] = run->path = buf;
		snprintf(buf, sizeof(buf), "%s/%s", SV_SVCDIR, run->name);
	}
	if (stat(run->path, &st_buf) < 0)
		return -ENOENT;

	/* get service type */
	if (S_ISDIR(st_buf.st_mode))
		argv[1] = type[1];
	else
		argv[1] = type[0];
	argv[2] = deps[0];

	/* check service mtime */
	if (st_buf.st_mtime > st_dep.st_mtime)
		LOG_WARN("%s was updated -- `scan' command might be necessary?\n",
				run->name);

	/* get service status */
	switch(command) {
	case RS_SVC_CMD_ADD:
	case RS_SVC_CMD_DEL:
		if (sv_stage < 0) {
			fprintf(stderr, "%s: stage level argument is required\n", progname);
			fprintf(stderr, "Usage: %s -(0|1|2|3) %s %s\n", progname,
					run->name, rs_svc_cmd[command]);
			retval = 1;
			goto reterr;
		}

		path = err_malloc(512);
		snprintf(path, 512, "%s/.stage-%d/%s", SV_SVCDIR, sv_stage,
				run->name);
		if (!access(path, F_OK)) {
			if (command == RS_SVC_CMD_DEL)
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
	case RS_SVC_CMD_START:
		if (svc_deps && !d)
			d = run->svc->data = rs_svcdeps_load(run->name);
		break;
	}

	/* setup dependencies */
	if (d) {
		retval = svc_depend(run);
		if (retval == -ENOENT)
			;
		else if (retval) {
			LOG_ERR("%s: Failed to set up service dependencies\n", run->name);
			svc_mark(run->name, RS_SVC_STAT_FAIL, cmd);
			retval = -ECANCELED;
			goto reterr;
		}
		else
			argv[2] = deps[1];
	}

	if (!svc_quiet)
		svc_log("[%s] service %s...\n", run->name, cmd);

	/* block signal before fork() */
	sigprocmask(SIG_SETMASK, &ss_full, NULL);

	pid = fork();
	if (pid > 0) { /* parent */
		/* restore signal mask */
		sigprocmask(SIG_SETMASK, &ss_child, NULL);
		close(run->lock);
		run->pid = pid;

		if (flags & SVC_CMD_WAIT) {
			waitpid(pid, &status, 0);
			svc_mark(run->name, RS_SVC_MARK_WAIT, NULL);

			retval = WEXITSTATUS(status);
			if (!svc_quiet)
				svc_end(run->name, retval);
			if (retval)
				svc_mark(run->name, RS_SVC_STAT_FAIL, cmd);
			else if (state) {
				svc_mark(run->name, state, NULL);
				if (d && d->virt)
					svc_mark(d->virt, state, NULL);
			}
			goto reterr;
		}
		else
			return SVC_RET_WAIT;
	}
	else if (pid == 0) { /* child */
		/* restore previous signal actions and mask */
		sigprocmask(SIG_SETMASK, &ss_child, NULL);

		if ((run->lock = svc_lock(run->name, SVC_LOCK, SVC_WAIT_SECS)) < 0) {
			LOG_ERR("%s: Failed to setup lockfile for service\n", run->name);
			_exit(ENOLCK);
		}
		write(run->lock, cmd, strlen(cmd)+1);
		/* close the lockfile to be able to mount rootfs read-only */
		if (sv_stage == 3 && command == RS_SVC_CMD_START)
			close(run->lock);

		/* restore previous default signal handling */
		svc_sigsetup();

		execve(RS_RUNSCRIPT, (char *const*)argv, (char *const*)run->envp);
		_exit(255);
	}
	else
		ERROR("%s: Failed to fork(): %s\n", __func__, strerror(errno));

reterr:
	free(argv);
	return retval;
}

static int svc_depend(struct svcrun *run)
{
	int type, val, retval = 0;
	int p = 0;
	RS_DepTree_T deptree = { NULL, NULL, 0, 0 };
	RS_SvcDeps_T *d = run->svc->data;

	if (!d)
		return -ENOENT;

	/* skip before deps type */
	for (type = RS_DEPS_USE; type < RS_DEPS_TYPE; type++) {
		if (TAILQ_EMPTY(d->deps[type]))
			continue;
		/* build a deptree to avoid segfault because cyclical dependencies */
		deptree.list = d->deps[type];
		svc_deptree_load(&deptree);
		while (p >= 0 && p < deptree.size) { /* PRIORITY_LEVEL_LOOP */
			if (!TAILQ_EMPTY(deptree.tree[p]))
				val = svc_execl(deptree.tree[p], run->argc, run->argv, run->envp);
				--p;
		} /* PRIORITY_LEVEL_LOOP */
		rs_deptree_free(&deptree);
		if (val > 0 && type == RS_DEPS_NEED)
			retval = val;
	}
	return retval;
}

static int svc_end(const char *svc, int status)
{
	static char ok[] = "ok", no[] = "no", *m;
	if (status)
		m = no;
	else
		m = ok;
	return svc_log("(%s) [%s]\n", svc, m);
}

static const char **svc_env(void)
{
	size_t size = 1024;
	char *env, *ptr;
	int i = 0, j;

	if (svc_environ)
		return svc_environ;
	env = err_malloc(size);
	svc_environ = err_calloc(ARRAY_SIZE(env_list), sizeof(void *));

	if (!getenv("COLUMNS")) {
		sprintf(env, "%d", get_term_cols());
		setenv("COLUMNS", env, 1);
	}
	free(env);

	for (j = 0; env_list[j]; j++) {
		ptr = getenv(env_list[j]);
		if (ptr) {
			env = err_malloc(size);
			snprintf(env, size, "%s=%s", env_list[j], ptr);
			svc_environ[i++] = err_realloc(env, strlen(env)+1);
		}
	}
	svc_environ[i++] = (char *)0;
	svc_environ[i++] = (char *)0;
	return svc_environ;
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
			if (svc_state(svc, RS_SVC_STAT_WAIT) < 1)
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
	return svc_state(svc, RS_SVC_STAT_WAIT) ? -1 : 0;
}

static char *get_cmdline_entry(const char *ent)
{
#ifdef __linux__
	FILE *fp;
	char *line = NULL, *ptr, path[] = "/proc/cmdline", *val;
	size_t len;

	if (access(path, F_OK))
		return NULL;
	if (!(fp = fopen(path, "r")))
		return NULL;
	if (!rs_getline(fp, &line, &len))
		return NULL;

	len = strlen(ent);
	ptr = line;
	while ((val = strsep(&ptr, " "))) {
		if (strncmp(ent, val, len) == 0) {
			val += len;
			switch(val[0]) {
			case '=':
				val++;
			case ' ':
			case '\0':
				val = err_strdup(val);
				break;
			default:
				continue;
			}
			break;
		}
	}
	fclose(fp);
	free(line);
	return val;
#else
	return NULL;
#endif /* __linux__ */
}

static const char *svc_runlevel(const char *level)
{
	static char buf[16], path[] = SV_TMPDIR "/softlevel";
	const char *retval;
	int fd, flags = O_NONBLOCK|O_CREAT, len;

	if (level)
		flags |= O_TRUNC|O_WRONLY;
	else if (access(path, F_OK))
		return NULL;
	else
		flags |= O_RDONLY;
	if ((fd = open(path, flags, 0644)) < 0) {
		WARN("Failed to open `%s': %s\n", path, strerror(errno));
		return NULL;
	}

	if (level) {
		len = strlen(level);
		if (write(fd, level, len) < len) {
			WARN("Failed to write to `%s': %s\n", path, strerror(errno));
			retval = NULL;
		}
		else
			retval = level;
	}
	else {
		if (( len = read(fd, buf, sizeof(buf))) < 0) {
			WARN("Failed to read from `%s': %s\n", path, strerror(errno));
			retval = NULL;
		}
		else
			buf[len++] = '\0', retval = buf;
	}

	close(fd);
	return retval;
}

static void svc_level(void)
{
	char *entry = NULL, path[512], *ptr, *old, *ent;
	int i;

	entry = get_cmdline_entry("softlevel");
	if (sv_stage == 1) {
		goto noinit;
	}
	else if (sv_stage == 2) {
		ptr = (char*)svc_runlevel(NULL);
		if (ptr && strcmp(ptr, sv_runlevel_name[SV_RUNLEVEL_SINGLE]) == 0)
			goto single;
	}

noinit:
	old = entry;
	entry = ent = get_cmdline_entry("noinit");
	/* mark no started services as stopped */
	if (entry) {
		while ((ptr = strsep(&ent, ",")))
			svc_mark(ptr, RS_SVC_STAT_STAR, NULL);
		free(entry);
	}
	entry = old;
nonetwork:
	/* mark network services as started, so nothing will be started */
	if ((entry && strcmp(entry, sv_runlevel_name[SV_RUNLEVEL_NONETWORK]) == 0) ||
		(sv_runlevel == SV_RUNLEVEL_NONETWORK)) {
		for (i = 0; i < SERVICES.virt_count; i++)
			if (strcmp(SERVICES.virt_svcdeps[i]->virt, "net") == 0)
				svc_mark(SERVICES.virt_svcdeps[i]->svc, RS_SVC_STAT_STAR, NULL);
		svc_mark("net", RS_SVC_STAT_STAR, NULL);
	}
single:
	if ((entry && strcmp(entry, sv_runlevel_name[SV_RUNLEVEL_SINGLE]) == 0) ||
	    (sv_runlevel == SV_RUNLEVEL_SINGLE)) {
		snprintf(path, sizeof(path), "%s/.%s", SV_SVCDIR,
				sv_runlevel_name[SV_RUNLEVEL_SINGLE]);
		DEPTREE.list = rs_svclist_load(path);
	}

	if (entry) free(entry);
}

static int svc_log(const char *fmt, ...)
{
	static char logfile[] = "/var/log/rs.log", *logpath;
	int retval = 0;
	va_list ap;


	/* save logfile if necessary */
	if (rs_conf_yesno("RS_DEBUG"))
		logpath = logfile;
	else
		logpath = RS_LOGFILE;
	if (!logfd && rs_debug) {
		logfd = open(logpath, O_NONBLOCK|O_CREAT|O_RDWR|O_CLOEXEC, 0644);
		if (logfd < 0)
			logfd = open(RS_LOGFILE, O_NONBLOCK|O_CREAT|O_RDWR|O_CLOEXEC, 0644);
		if (logfd > 0) {
			rs_debug = 1;
			logfp = fdopen(logfd, "a+");
		}
	}

	va_start(ap, fmt);
	if (svc_quiet)
		retval = vfprintf(stderr, fmt, ap);
	else if (logfp)
		retval = vfprintf(logfp , fmt, ap);
	va_end(ap);
	return retval;
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

static int svc_mark(const char *svc, int status, const char *what)
{
	char path[512], *ptr;
	int fd;
	mode_t m;

	if (!svc) {
		errno = ENOENT;
		return -1;
	}

	switch(status) {
		case RS_SVC_STAT_FAIL:
		case RS_SVC_MARK_FAIL:
			ptr = SV_TMPDIR_FAIL;
			break;
		case RS_SVC_STAT_DOWN:
		case RS_SVC_MARK_DOWN:
			ptr = SV_TMPDIR_DOWN;
			break;
		case RS_SVC_STAT_STAR:
		case RS_SVC_MARK_STAR:
			ptr = SV_TMPDIR_STAR;
			break;
		case RS_SVC_STAT_WAIT:
		case RS_SVC_MARK_WAIT:
			ptr = SV_TMPDIR_WAIT;
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	snprintf(path, sizeof(path), "%s/%s", ptr, svc);
	switch (status) {
		case RS_SVC_STAT_DOWN:
		case RS_SVC_STAT_FAIL:
		case RS_SVC_STAT_STAR:
		case RS_SVC_STAT_WAIT:
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
		case RS_SVC_STAT_FAIL:
		case RS_SVC_MARK_FAIL:
			ptr = SV_TMPDIR_FAIL;
			break;
		case RS_SVC_STAT_DOWN:
		case RS_SVC_MARK_DOWN:
			ptr = SV_TMPDIR_DOWN;
			break;
		case RS_SVC_STAT_PIDS:
			ptr = SV_TMPDIR_PIDS;
			break;
		case RS_SVC_STAT_STAR:
		case RS_SVC_MARK_STAR:
			ptr = SV_TMPDIR_STAR;
			break;
		case RS_SVC_STAT_WAIT:
		case RS_SVC_MARK_WAIT:
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
	case SIGINT:
		if (i < 0) i = 0;
	case SIGTERM:
		if (i < 0) i = 3;
	case SIGQUIT:
		if (i < 0) i = 1;
		ERR("caught %s, aborting\n", signame[i]);
		kill(0, sig);
		exit(EXIT_FAILURE);
	case SIGUSR1:
		fprintf(stderr, "%s: Aborting!\n", progname);
		/* block child signals */
		sigprocmask(SIG_SETMASK, &ss_child, NULL);

		/* kill any worker process we have started */
		kill(0, SIGTERM);

		exit(EXIT_FAILURE);
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

	sigfillset(&ss_full);
	sigemptyset(&ss_old);
	sigemptyset(&ss_child);
	sigaddset(&ss_child, SIGCHLD);
	sigprocmask(SIG_SETMASK, &ss_child, &ss_old);

	sa.sa_handler = sv_sighandler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGHUP , &sa, NULL);
	sigaction(SIGINT , &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
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

__NORETURN__ static int svc_exec(int argc, char *argv[]) {
	int i = 0, j, retval;
	int cmd_flags = SVC_CMD_WAIT;
	struct svcrun run;
	RS_String_T svc = { NULL, NULL, NULL, NULL, };

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

	/* setup argv and envp */
	argc -= 2, argv += 2;
	i = 5;
	for ( j = 0; j < argc; j++)
		run.argv[i++] = argv[j];
	run.argv[i] = (char *)0;
	run.envp = svc_env();
	rs_svcdeps_load(NULL);

	retval = svc_cmd(&run, cmd_flags);
	switch(retval) {
	case -EBUSY:
	case -EINVAL:
		exit(EXIT_SUCCESS);
	case -ENOENT:
		ERR("inexistant service -- %s\n", run.name);
		exit(2);
	case -ECANCELED:
		exit(4);
	default:
		exit(retval);
	}
}

static int svc_execl(RS_StringList_T *list, int argc, const char *argv[],
		const char *envp[])
{
	RS_String_T *svc;
	size_t n = 0, size = 8;
	int retval = 0;
	int i, r, state, status;
	static int parallel, setup, cmd_flags;
	struct svcrun **run;
	RS_SvcDeps_T *d;

	if (list == NULL) {
		errno = ENOENT;
		return -1;
	}
	if (argv == NULL || envp == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (!setup) {
		parallel = rs_conf_yesno("RS_PARALLEL");
		if (!parallel)
			cmd_flags |= SVC_CMD_WAIT;
		setup = 1;
	}
	if (strcmp(argv[4], rs_svc_cmd[RS_SVC_CMD_START]) == 0)
		state = RS_SVC_STAT_STAR;
	else
		state = RS_SVC_MARK_STAR;

	run = err_malloc(size*sizeof(void*));
	TAILQ_FOREACH(svc, list, entries) {
		run[n] = err_malloc(sizeof(struct svcrun));
		run[n]->name = svc->str;
		run[n]->argc = argc;
		run[n]->argv = argv;
		run[n]->envp = envp;
		run[n]->path = NULL;
		run[n]->svc  = svc;

		r = svc_cmd(run[n], cmd_flags);
		switch(r) {
		case SVC_RET_WAIT:
			break;
		case -ENOENT:
		case -ECANCELED:
			retval++;
			continue;
			break;
		case -EBUSY:
		case -EINVAL:
			continue;
			break;
		}

		if (parallel) {
			if (n++ == size) {
				size += 8;
				run = err_realloc(run, sizeof(void*)*size);
			}
		}
		else {
			free(run[n]);
			if (r) retval++;
		}
	}

	for (i = 0; i < n; i++) {
		waitpid(run[i]->pid, &status, 0);
		svc_mark(run[i]->name, RS_SVC_MARK_WAIT, NULL);
		if (WEXITSTATUS(status)) {
			retval++;
			svc_mark(run[i]->name, RS_SVC_STAT_FAIL, argv[4]);
		}
		else {
			svc_mark(run[i]->name, state, NULL);
			d = run[n]->svc->data;
			if (d && d->virt)
				svc_mark(d->virt, state, NULL);
		}
		if (!svc_quiet)
			svc_end(run[i]->name, status);
		free(run[i]);
	}
	free(run);

	return retval;
}

static int svc_stage_command(int stage, int argc, const char *argv[], const char *envp[])
{
	int i, retval;
	RS_StringList_T *init_stage_list;

	init_stage_list = rs_stringlist_new();
	for (i = 0; rs_init_stage[stage][i]; i++)
		rs_stringlist_add(init_stage_list, rs_init_stage[stage][i]);

	retval = svc_execl(init_stage_list, argc, argv, envp);
	rs_stringlist_free(&init_stage_list);

	return retval;
}

static void svc_stage(const char *cmd)
{
	const char *command = cmd;
	const char **envp;
	const char *argv[8] = { "runscript" };
	char buf[128];
	int p, r;
	int svc_start = 1;
	int level = 0;
	int argc = 8;
	time_t t;

	/* set a few sane environment variables */
	svc_deps  = 0;
	svc_quiet = 0;
	sv_pid    = getpid();
	snprintf(buf, sizeof(buf), "%d", sv_stage);
	setenv("SV_STAGE", buf, 1);
	setenv("SV_RUNLEVEL", sv_runlevel_name[sv_runlevel], 1);
	snprintf(buf, sizeof(buf), "%d", sv_pid);
	setenv("SV_PID", buf, 1);
	if ((r = open(SV_PIDFILE, O_CREAT|O_WRONLY|O_TRUNC, 0644)) > 0) {
		write(r, buf, strlen(buf));
		close(r);
	}
	else
		WARN("Failed to open %s\n", SV_PIDFILE);
	snprintf(buf, sizeof(buf), "%s/%d_deptree", SV_TMPDIR_DEPS, sv_stage);

	if (sv_stage == 0 || command == NULL) /* force service command */
		command = rs_svc_cmd[RS_SVC_CMD_START];
	if (strcmp(command, rs_svc_cmd[RS_SVC_CMD_STOP]) == 0)
		svc_start = 0;

	envp = svc_env();
	argv[5] = (char *)0;
	chdir("/");

	t = time(NULL);
	rs_debug = 1;
	svc_log("logging: %s command\n", command);
	svc_log("rs init stage-%d started at %s\n", sv_stage, ctime(&t));

	/* do this extra loop to be able to stop stage-1 with SV_STAGE=3; so that,
	 * {local,network}fs services etc. can be safely stopped
	 */
	for (;;) { /* SHUTDOWN_LOOP */
		if (sv_stage == 3 && !level) {
			level = sv_stage;
			/* load the started services instead of only stage-[12]
			 * to be abe to shutdown everything with SV_STAGE=3
			 */
			DEPTREE.list = rs_svclist_load(SV_TMPDIR_STAR);
			command = rs_svc_cmd[RS_SVC_CMD_STOP];
			svc_start = 0;
		}
		else if (level == 3) {
			level = 0;
			/* close the logfile because rootfs will be mounted read-only */
			if (!fclose(logfp))
				logfd = 0;
			if (!close(logfd))
				logfp = NULL;
			/* and finaly start stage-3 */
			command = rs_svc_cmd[RS_SVC_CMD_START];
			svc_start = 1;
			unlink(buf);
		}
		else if (sv_stage == 1 || sv_stage == 2)
			svc_level(); /* make SysVinit compatible runlevel */
		argv[4] = command;

		t = time(NULL);
		svc_log( "\n\tstage-%d (%s) at %s\n", sv_stage, command, ctime(&t));

		rs_deptree_load(&DEPTREE);
		if (svc_start)
			p = DEPTREE.size-1;
		else
			p = 0;
		while (p >= 0 && p < DEPTREE.size) { /* PRIORITY_LEVEL_LOOP */
			if (!TAILQ_EMPTY(DEPTREE.tree[p])) {
				t = time(NULL);
				svc_log("\n\tpriority-level-%d started at %s\n", p,	ctime(&t));
				r = svc_execl(DEPTREE.tree[p], argc, argv, envp);
				/* enable dependency tracking only if needed */
				if (r)
					svc_deps = 1;
				else
					svc_deps = 0;
			}
			if (svc_start)
				--p;
			else
				++p;
		} /* PRIORITY_LEVEL_LOOP */
		rs_deptree_free(&DEPTREE);

		/* break shutdown loop */
		if (!level)
			break;
	} /* SHUTDOWN_LOOP */

	/* finish sysinit */
	if (sv_stage == 0 )
		svc_stage_command(0, argc, argv, envp);

	svc_runlevel(sv_runlevel_name[sv_runlevel]);
	t = time(NULL);
	svc_log("\nrs init stage-%d stopped at %s\n", sv_stage, ctime(&t));
}

int main(int argc, char *argv[])
{
	int opt;
	char *ptr;
	char on[8] = "1", off[8] = "0";

	progname = strrchr(argv[0], '/');
	if (!progname)
		progname = argv[0];
	else
		progname++;

	/* Show help if insufficient args */
	if (argc < 2)
		help_message(1);

	/* Parse options */
	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
	{
		switch (opt) {
			case 'D':
				svc_deps = 0;
				setenv("SVC_DEPS", off, 1);
				break;
			case 'g':
				setenv("SVC_DEBUG", on, 1);
				break;
			case '0':
					sv_runlevel = SV_RUNLEVEL_SYSINIT;
					sv_stage = 0;
					break;
			case 'N':
				sv_runlevel = SV_RUNLEVEL_NONETWORK;
			case '1':
				if (sv_runlevel < 0)
					sv_runlevel = SV_RUNLEVEL_BOOT;
			case 'S':
				if (sv_runlevel < 0)
					sv_runlevel = SV_RUNLEVEL_SINGLE;
				sv_stage = 1;
				break;
			case '2':
				sv_runlevel = SV_RUNLEVEL_DEFAULT;
				sv_stage = 2;
				break;
			case 'R':
				sv_runlevel = SV_RUNLEVEL_REBOOT;
			case '3':
				if (sv_runlevel < 0)
					sv_runlevel = SV_RUNLEVEL_SHUTDOWN;
				sv_stage = 3;
				break;
			case 'q':
				svc_quiet = 0;
				break;
			case 'v':
				printf("%s version %s\n\n", progname, VERSION);
				puts(RS_COPYRIGHT);
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
	sv_sigsetup();

	/* set this to avoid double waiting for a lockfile for supervision */
	setenv("SVC_WAIT", off, 1);
	setenv("SVC_DEPS", off, 1);

	if (strcmp(progname, "rs") == 0 || strcmp(progname, "service") == 0)
		goto rs;
	else if (strcmp(progname, "rc") == 0) {
		setenv("SVC_DEBUG", off, 1);
		goto rc;
	}
	else if (strcmp(progname, "sv-stage") == 0 || strcmp(*argv, "stage") == 0) {
		if (strcmp(*argv, "stage") == 0)
			argv++;
		setenv("SVC_DEBUG", off, 1);
		if (sv_stage >= 0)
			svc_stage(*argv);
		else {
			fprintf(stderr, "Usage: %s -(0|1|2|3) [start|stop]"
					"(level argument required)\n", progname);
			exit(EXIT_FAILURE);
		}
	}
	else if (strcmp(*argv, "scan") == 0)
		goto scan;
	else if (argc == 1)
		goto rc;
	else
		goto rs;

	exit(EXIT_SUCCESS);

rc:
	if (argc != 1) {
rc_help:
		fprintf(stderr, "Usage: %s {nonetwork|single|sysinit|boot|default|shutdown|reboot} "
				"(run level)\n", progname);
		exit(EXIT_FAILURE);
	}

	/* support SystemV compatiblity rc command */
	for (sv_runlevel = 0; sv_runlevel_name[sv_runlevel]; sv_runlevel++) {
		if (strcmp(*argv, sv_runlevel_name[sv_runlevel]) == 0) {
			switch(sv_runlevel) {
			case SV_RUNLEVEL_REBOOT:
			case SV_RUNLEVEL_SHUTDOWN:
				sv_stage = 3;
				break;
			case SV_RUNLEVEL_DEFAULT:
				sv_stage = 2;
				break;
			case SV_RUNLEVEL_SINGLE:
				/* stop stage-2 */
				ptr = (char *)svc_runlevel(NULL);
				if (ptr && strcmp(ptr, sv_runlevel_name[SV_RUNLEVEL_DEFAULT]) == 0) {
					sv_stage = 2;
					DEPTREE.list = rs_svclist_load(NULL);
					svc_stage(rs_svc_cmd[RS_SVC_CMD_STOP]);
				}
			case SV_RUNLEVEL_NONETWORK:
			case SV_RUNLEVEL_BOOT:
				sv_stage = 1;
				break;
			case SV_RUNLEVEL_SYSINIT:
				sv_stage = 0;
				break;
			}
			svc_stage(NULL);
			exit(EXIT_SUCCESS);
		}
	}

	if (strcmp(progname, "rc") == 0)
		goto rc_help;
	ERR("invalid argument -- `%s'\n", *argv);
	fprintf(stderr, "Usage: %s -{0|1|2|3|R|S|N} [OPTIONS] (init-stage)\n", progname);
	exit(EXIT_FAILURE);

scan:
	svc_sigsetup();
	setenv("SVCDEPS_UPDATE", on, 1);
	execv(SV_DEPGEN, argv);
	ERROR("Failed to execv(%s, argv)", SV_DEPGEN);

rs:
	unsetenv("SV_STAGE");
	unsetenv("SV_RUNLEVEL");
	/* handle service command or
	 * support systemV compatiblity rc command
	 */
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [OPTIONS] SERVICE COMMAND [ARGUMENTS]\n",
				progname);
		exit(EXIT_FAILURE);
	}
	svc_exec(argc, argv);
}
