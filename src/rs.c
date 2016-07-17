/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision (Scripts Framework).
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 */

#include "error.h"
#include "helper.h"
#include "rs.h"
#include <dirent.h>
#include <getopt.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <poll.h>

#define VERSION "0.12.0"
#define RS_RUNSCRIPT SV_LIBDIR "/sh/runscript"

#define SV_TMPDIR_DOWN SV_TMPDIR "/down"
#define SV_TMPDIR_FAIL SV_TMPDIR "/fail"
#define SV_TMPDIR_PIDS SV_TMPDIR "/pids"
#define SV_TMPDIR_STAR SV_TMPDIR "/star"
#define SV_TMPDIR_WAIT SV_TMPDIR "/wait"

struct svcrun {
	const char *name;
	const char *path;
	pid_t pid;
	int lock;
};

static int svc_deps  = 0;
static int svc_quiet = 1;
static RS_SvcDepsList_T *svcdeps;

/* list of service to start/stop before|after a stage */
static const char *const rs_init_stage[][4] = {
	{ "clock", "hostname", NULL },
	{ "sysctl", "dmcrypt", NULL },
	{                      NULL },
	{ "devfs",  "sysfs",   NULL },
};

/* !!! order matter (defined constant/enumeration) !!! */
const char *const rs_stage_type[] = { "rs", "sv" };
const char *const rs_stage_name[] = { "sysinit", "boot", "default", "shutdown" };
const char *const rs_deps_type[] = { "before", "after", "use", "need" };
const char *prgname;

enum {
	RS_SVC_CMD_STOP,
	RS_SVC_CMD_START,
	RS_SVC_CMD_ADD,
	RS_SVC_CMD_DEL,
	RS_SVC_CMD_DESC,
	RS_SVC_CMD_REMOVE,
	RS_SVC_CMD_RESTART,
	RS_SVC_CMD_STATUS,
	RS_SVC_CMD_ZAP
};
/* !!! likewise (service command) !!! */
static const char *const rs_svc_cmd[] = { "stop", "start",
	"add", "del", "desc", "remove", "restart", "status", "zap"
};

static const char *shortopts = "Dg0123qrVvh";
static const struct option longopts[] = {
	{ "nodeps",   0, NULL, 'D' },
	{ "debug",    0, NULL, 'g' },
	{ "sysinit",  0, NULL, '0' },
	{ "boot",     0, NULL, '1' },
	{ "default",  0, NULL, '2' },
	{ "shutdown", 0, NULL, '3' },
	{ "rs",       0, NULL, 'r' },
	{ "sv",       0, NULL, 'v' },
	{ "quiet",    0, NULL, 'q' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'V' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Disable dependencies",
	"Enable debug mode",
	"Select stage-0 run level",
	"Select stage-1 run level",
	"Select stage-2 run level",
	"Select stage-3 run level",
	"Select runscript backend",
	"Select supervision backend",
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
	"COLUMNS", "LINES",	"RS_STRICT_DEP",
	"SVC_DEBUG", "SVC_DEPS", "SVC_WAIT", NULL
};

__NORETURN__ static void help_message(int exit_val);

#define SVC_CMD_FIND 1
#define SVC_CMD_WAIT 2
#define SVC_RET_WAIT 256
/* execute a service command;
 * @argv: argument vector to pass to execve(2);
 * @envp: environment vector to pass to execve(2);
 * @run: an info structure to update;
 * @return: -errno on errors,
 *   child return value if SVC_CMD_WAIT is or-ed to the flags,
 *   SVC_RET_WAIT otherwise;
 */
static int svc_cmd(const char *argv[], const char *envp[], struct svcrun *run, int flags);

/*
 * setup service dependencies
 * @svc: service name;
 * @argv and @envp are passed verbatim to svc_exec_list();
 * @return: 0 on succsess,
 *        > 0 for non fatals errors
 *        < 0 for fatals errors;
 */
static int svc_depend(const char *svc, const char *argv[], const char *envp[]);

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
static int rs_stage_start(int stage, const char *argv[], const char *envp[]);

/*
 * stop remaining list;
 * @return: 0 on success;
 */
static int rs_stage_stop(const char *argv[], const char *envp[]);

/*
 * set service status
 * @svc: service name;
 * @status: int value [dfrs]
 * @return: 0 on success;
 */
static int svc_mark(const char *svc, int status);

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
#define SVC_WAIT_MSEC 10000 /* interval for displaying warning */
#define SVC_WAIT_POLL 100   /* poll interval */

/*
 * execute a service with the appended arguments
 */
__NORETURN__ static int svc_exec(int argc, char *argv[]);

/*
 * execute a service list (called from svc_stage())
 * @return 0 on success or number of failed services
 */
static int svc_exec_list(RS_StringList_T *list, const char *argv[], const char *envp[]);

/*
 * remove service temporary files
 */
static void svc_zap(const char *svc);

/* handle SIGCHLD/INT setup */
static struct sigaction sa_sigint, sa_sigquit;
static sigset_t ss_savemask;
static void svc_sigsetup(void);

/*
 * find a service
 * @return NULL if nothing found or service path (dir/file)
 */
static char *svc_find(const char *svc);

/*
 * generate a default environment for service
 */
static const char **svc_env(void);

__NORETURN__ static void help_message(int exit_val)
{
	int i;

	printf("usage: rs [OPTIONS] SERVICE COMMAND\n");
	printf("  COMMAND: add|del|remove|restart|start|stop|status|zap\n");
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

static int svc_cmd(const char *argv[], const char *envp[], struct svcrun *run, int flags)
{
	static char nodeps[16] = "--nodeps";
	static char type_rs[8] = "--rs", type_sv[8] = "--sv";
	static size_t len;
	int status, command, retval = 1;
	int find = flags & SVC_CMD_FIND, type;
	pid_t pid;
	int deps = 0, i, size = 4;
	const char **ARGV = NULL;

	if (!len) {
		svc_sigsetup();
		len = strlen(RS_SVCDIR);
	}

	if (strcmp(argv[3], rs_svc_cmd[RS_SVC_CMD_START]) == 0)
		command = 's', deps = 1;
	else if (strcmp(argv[3], rs_svc_cmd[RS_SVC_CMD_STOP]) == 0)
		command = 'S';
	else if (strcmp(argv[3], rs_svc_cmd[RS_SVC_CMD_ZAP]) == 0) {
		svc_zap(run->name);
		return 0;
	}
	else if (strcmp(argv[3], rs_svc_cmd[RS_SVC_CMD_STATUS]) == 0) {
		if (svc_state(run->name, 's')) {
			printf("%s: service is started\n", run->name);
			return 0;
		}
		else {
			printf("%s: service is stopped\n", run->name);
			return 3;
		}
	}
	else
		command = 0;

	while (argv[size])
		size++;
	ARGV = err_calloc(size+2, sizeof(void*));
	for (i = 0; i <= size; i++)
		ARGV[i] = argv[i];
	/* get service path */
	if (find) {
		run->path = ARGV[2] = svc_find(run->name);
		if (ARGV[2] == NULL)
			return -ENOENT;
	}
	if (strncmp(RS_SVCDIR, ARGV[2], len) == 0)
		type = 0, ARGV[1] = type_rs;
	else
		type = 1, ARGV[1] = type_sv;

	/* get service status */
	switch(command) {
	case 's':
	case 'S':
		if (type)
			status = svc_state(run->name, 'p');
		else
			status = svc_state(run->name, 's');

		if (status) {
			if (command == 's') {
				if (svc_quiet)
					WARN("%s: Service is already started\n", run->name);
				return -EBUSY;
			}
		}
		else if (command == 'S') {
			if (svc_quiet)
				WARN("%s: Service is not started\n", run->name);
			return -EINVAL;
		}

		break;
	}

	/* setup dependencies */
	if (deps && svc_deps) {
		retval = svc_depend(run->name, argv, envp);
		if (retval == -ENOENT)
			;
		else if (retval < 0) {
			if (svc_quiet)
				ERR("%s: Failed to set up service dependencies\n", run->name);
			retval = -ECANCELED;
			goto reterr;
		}
		else {
			for (i = size; i > 2; i--)
				ARGV[i] = ARGV[i-1];
			ARGV[2] = nodeps;
		}
	}

	if ((run->lock = svc_lock(run->name, SVC_LOCK, SVC_WAIT_SECS)) < 0) {
		if (svc_quiet)
			ERR("%s: Failed to setup lockfile for service\n", run->name);
		retval = -ENOLCK;
		goto reterr;
	}

	pid = fork();
	if (pid > 0) { /* parent */
		close(run->lock);
		run->pid = pid;

		if (flags & SVC_CMD_WAIT) {
			waitpid(pid, &status, 0);
			if (command)
				svc_mark(run->name, 'W');

			retval = WEXITSTATUS(status);
			if (!retval && command)
				svc_mark(run->name, command);
			goto reterr;
		}
		else
			return SVC_RET_WAIT;
	}
	else if (pid == 0) { /* child */
		/* restore previous signal actions and mask */
		sigaction(SIGINT, &sa_sigint, NULL);
		sigaction(SIGQUIT, &sa_sigquit, NULL);
		sigprocmask(SIG_SETMASK, &ss_savemask, NULL);

		execve(RS_RUNSCRIPT, (char *const*)ARGV, (char *const*)envp);
		_exit(255);
	}
	else
		ERROR("%s: Failed to fork(): %s\n", __func__, strerror(errno));

reterr:
	if (find)
		free((void *)run->path);
	free(ARGV);
	return retval;
}

static int svc_depend(const char *svc, const char *argv[], const char *envp[])
{
	RS_SvcDeps_T *depend;
	int type, val, retval = 0;
	static int strict_dep, setup = 1;

	if (setup) {
		strict_dep = rs_yesno(getenv("RS_STRICT_DEP"));
		setup = 0;
	}
	if (svc_deps == 0)
		return 0;
	if (svcdeps)
		depend = rs_svcdeps_find(svcdeps, svc);
	else
		return -ENOENT;
	if (!depend)
		return 0;

	/* skip before deps type */
	for (type = RS_DEPS_AFTER; type < RS_DEPS_TYPE; type++) {
		val = svc_exec_list(depend->deps[type], argv, envp);
		if (val > 0) {
			switch(type) {
			case RS_DEPS_AFTER:
				if (!strict_dep)
					break;
			case RS_DEPS_NEED:
				retval -= val;
				break;
			}
		}
	}
	return retval;
}

static const char **svc_env(void)
{
	const char **envp;
	size_t size = 1024;
	char *env = err_malloc(8);
	int i = 0, j;
	envp = err_calloc(ARRAY_SIZE(env_list), sizeof(void *));

	if (!getenv("COLUMNS")) {
		sprintf(env, "%d", get_term_cols());
		setenv("COLUMNS", env, 1);
	}
	free(env);

	for (j = 0; env_list[j]; j++)
		if (getenv(env_list[j])) {
			env = err_malloc(size);
			snprintf(env, size, "%s=%s", env_list[j], getenv(env_list[j]));
			envp[i++] = err_realloc(env, strlen(env)+1);
		}

	envp[i] = (char *)0;
	return envp;
}

static char *svc_find(const char *svc)
{
	char *buf = err_malloc(BUFSIZ);
	int i;
	int err = errno;

	if (!svc)
		return NULL;

	snprintf(buf, BUFSIZ, "%s/%s", RS_SVCDIR, svc);
	if (file_test(buf, 0))
		return err_realloc(buf, strlen(buf)+1);
	errno = 0;
	for (i = 0; i <= 3; i++) {
		snprintf(buf, BUFSIZ, "%s/stage-%d/%s", RS_SVCDIR, i, svc);
		if (file_test(buf, 0))
			return err_realloc(buf, strlen(buf)+1);
		errno = 0;
	}

	snprintf(buf, BUFSIZ, "%s/%s", SV_SVCDIR, svc);
	if (file_test(buf, 0))
		return err_realloc(buf, strlen(buf)+1);
	errno = 0;
	snprintf(buf, BUFSIZ, "%s/%s", SV_SERVICE, svc);
	if (file_test(buf, 0))
		return err_realloc(buf, strlen(buf)+1);
	errno = err;

	free(buf);
	return NULL;
}

static int svc_lock(const char *svc, int lock_fd, int timeout)
{
	char f_path[BUFSIZ];
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
		w = file_test(f_path, 0);
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
			if (file_test(f_path, 0))
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
				if (svc_quiet)
					ERR("%s: Failed to flock(%d, LOCK_EX...): %s\n", svc, fd,
							strerror(errno));
				close(fd);
				return -1;
			}
		return fd;
	}
	else if (lock_fd > 0)
		close(lock_fd);
	if (file_test(f_path, 0))
		unlink(f_path);
	return 0;
}

static int svc_wait(const char *svc, int timeout, int lock_fd)
{
	int i, j;
	int err;
	int msec = SVC_WAIT_MSEC, nsec;
	if (timeout < 10) {
		nsec = timeout;
		msec = 1000*timeout;
	}
	else
		nsec = timeout % 10;
	nsec = nsec ? nsec : 10;

	for (i = 0; i < timeout; i += 10) {
		for (j = SVC_WAIT_POLL; j <= msec; j += SVC_WAIT_POLL) {
			if (svc_state(svc, 'w') <= 0)
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
		WARN("waiting for %s (%d seconds)\n", svc, i+nsec);
	}
	return svc_state(svc, 'w') ? -1 : 0;
}

static void svc_zap(const char *svc)
{
	int i;
	char path[BUFSIZ];
	char *dirs[] = { SV_TMPDIR_DOWN, SV_TMPDIR_FAIL,
		SV_TMPDIR_PIDS, SV_TMPDIR_STAR,
		SV_TMPDIR_WAIT, NULL };

	for (i = 0; dirs[i]; i++) {
		snprintf(path, sizeof(path), "%s/%s", dirs[i], svc);
		if (file_test(path, 0))
			unlink(path);
	}

	snprintf(path, sizeof(path), "%s/%s_OPTIONS", SV_TMPDIR, svc);
	if (file_test(path, 0))
		unlink(path);
}

static int svc_mark(const char *svc, int status)
{
	char path[BUFSIZ], *ptr;
	int fd;
	mode_t m;

	if (!svc) {
		errno = ENOENT;
		return -1;
	}

	switch(status) {
		case 'f':
		case 'F':
			ptr = SV_TMPDIR_FAIL;
			break;
		case 'd':
		case 'D':
			ptr = SV_TMPDIR_DOWN;
			break;
		case 's':
		case 'S':
			ptr = SV_TMPDIR_STAR;
			break;
		case 'w':
		case 'W':
			ptr = SV_TMPDIR_WAIT;
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	snprintf(path, sizeof(path), "%s/%s", ptr, svc);
	switch (status) {
		case 'd':
		case 'f':
		case 's':
		case 'w':
			m = umask(0);
			fd = open(path, O_CREAT|O_WRONLY|O_NONBLOCK, 0644);
			umask(m);
			if (fd >= 0) {
				close(fd);
				return 0;
			}
			return -1;
		default:
			if (file_test(path, 0))
				return unlink(path);
			else
				return 0;
	}
}

static int svc_state(const char *svc, int status)
{
	char path[BUFSIZ], *ptr = NULL;
	int retval;

	if (!svc) {
		errno = ENOENT;
		return 0;
	}

	switch(status) {
		case 'e':
		case 'E':
			ptr = svc_find(svc);
			if (ptr)
				retval = 1;
			else
				retval = 0;
			free(ptr);
			return retval;
		case 'f':
		case 'F':
			ptr = SV_TMPDIR_FAIL;
			break;
		case 'd':
		case 'D':
			ptr = SV_TMPDIR_DOWN;
			break;
		case 'p':
		case 'P':
			ptr = SV_TMPDIR_PIDS;
			break;
		case 's':
		case 'S':
			ptr = SV_TMPDIR_STAR;
			break;
		case 'w':
		case 'W':
			ptr = SV_TMPDIR_WAIT;
			break;
		default:
			errno = EINVAL;
			return 0;
	}
	snprintf(path, sizeof(path), "%s/%s", ptr, svc);
	return file_test(path, 0);
}

static void svc_sigsetup(void)
{
	static int sigsetup = 0;
	static struct sigaction sa_ign;
	static sigset_t ss_child;

	if (sigsetup)
		return;

	/* ignore SIGINT and SIGQUIT */
	sa_ign.sa_handler = SIG_IGN;
	sa_ign.sa_flags = 0;
	sigemptyset(&sa_ign.sa_mask);

	if (sigaction(SIGINT, &sa_ign, &sa_sigint) < 0)
		ERROR("%s: sigaction(SIGINT)", __func__);
	if (sigaction(SIGQUIT, &sa_ign, &sa_sigquit) < 0)
		ERROR("%s: sigaction(SIGQUIT)", __func__);
	sigemptyset(&ss_child);
	sigaddset(&ss_child, SIGCHLD);
	sigsetup = 1;

	/* block SIGCHLD */
	if (sigprocmask(SIG_BLOCK, &ss_child, &ss_savemask) < 0)
		ERROR("%s: sigprocmask(SIG_BLOCK)", __func__);
}

__NORETURN__ static int svc_exec(int argc, char *args[]) {
	const char **envp;
	const char *argv[argc+4];
	int i = 0, j, retval;
	int cmd_flags = SVC_CMD_WAIT;
	struct svcrun run;

	if (args[0][0] == '/') {
		argv[2] = args[0];
		run.name = strrchr(args[0], '/')+1;
	}
	else {
		cmd_flags |= SVC_CMD_FIND;
		run.name = args[0];
	}
	argv[0] = "runscript", argv[3] = args[1];

	/* setup argv and envp */
	argc -= 2, args += 2, i = 4;
	for ( j = 0; j < argc; j++)
		argv[i++] = args[j];
	argv[i] = (char *)0;
	envp = svc_env();

	retval = svc_cmd(argv, envp, &run, cmd_flags);
	switch(retval) {
	case -EBUSY:
	case -EINVAL:
	case -ENOENT:
		exit(EXIT_SUCCESS);
	case -ENOLCK:
	case -ECANCELED:
		exit(EXIT_FAILURE);
	default:
		exit(retval);
	}
}

static int svc_exec_list(RS_StringList_T *list, const char *argv[], const char *envp[])
{
	RS_String_T *svc;
	size_t n = 0, size = 8;
	int retval = 0;
	int i, r, state, status;
	static int parallel, setup, cmd_flags = SVC_CMD_FIND;
	struct svcrun *run = NULL;

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
	}
	if (strcmp(argv[3], rs_svc_cmd[RS_SVC_CMD_START]) == 0)
		state = 's';
	else
		state = 'S';

	run = err_calloc(size, sizeof(struct svcrun));
	SLIST_FOREACH(svc, list, entries) {
		run[n].name = svc->str;

		r = svc_cmd(argv, envp, &run[n], cmd_flags);
		switch(r) {
		case SVC_RET_WAIT:
			break;
		case -EBUSY:
		case -EINVAL:
		case -ENOENT:
		case -ENOLCK:
		case -ECANCELED:
			continue;
			break;
		}

		if (parallel) {
			close(run[n].lock);
			if (n == size) {
				size += 8;
				run = err_realloc(run, sizeof(struct svcrun)*size);
			}
			n++;
		}
		else if (r)
			retval++;
	}

	for (i = 0; i < n; i++) {
		waitpid(run[i].pid, &status, 0);
		svc_mark(run[i].name, 'W');
		if (WEXITSTATUS(status)) {
			retval++;
			if (state == 's')
				svc_mark(run[i].name, 'f');
		}
		else
			svc_mark(run[i].name, state);
	}
	free(run);

	return retval;
}

static int rs_stage_start(int stage, const char *argv[], const char *envp[])
{
	int i, retval;
	RS_StringList_T *init_stage_list;

	init_stage_list = rs_stringlist_new();
	for (i = 0; rs_init_stage[stage][i]; i++)
		rs_stringlist_add(init_stage_list, rs_init_stage[stage][i]);

	retval = svc_exec_list(init_stage_list, argv, envp);
	rs_stringlist_free(init_stage_list);

	return retval;
}

static int rs_stage_stop(const char *argv[], const char *envp[])
{
	int retval;
	RS_StringList_T *svclist;
	DIR *d_ptr;
	struct dirent *d_ent;

	d_ptr = opendir(SV_TMPDIR_STAR);
	if (d_ptr == NULL) {
		DBG("Failed to open %s directory: `%s'\n", SV_TMPDIR_STAR, strerror(errno));
		return -1;
	}
	svclist = rs_stringlist_new();

	while ((d_ent = readdir(d_ptr)))
		if (d_ent->d_name[0] != '.')
			rs_stringlist_add(svclist, d_ent->d_name);
	closedir(d_ptr);

	retval = svc_exec_list(svclist, argv, envp);
	rs_stringlist_free(svclist);
	return retval;
}

static void svc_stage(const char *cmd)
{
	RS_STAGE.level = atoi(getenv("RS_STAGE"));
	RS_STAGE.type  = getenv("RS_TYPE");
	RS_StringList_T **deptree;
	const char *command = cmd;
	const char **envp;
	const char *argv[8] = { "runscript" };
	int j, k, type = 1;
	int svc_start = 1;
	int level = 0;

	if (RS_STAGE.level == 0 || RS_STAGE.level == 3) { /* force stage type */
		setenv("RS_TYPE", rs_stage_type[RS_STAGE_RUNSCRIPT], 1);
		RS_STAGE.type = rs_stage_type[RS_STAGE_RUNSCRIPT];
		command = rs_svc_cmd[RS_SVC_CMD_START];
	}
	if (RS_STAGE.type == NULL) /* -r|-v passed ? */
		type = 0;
	if (command == NULL) /* start|stop passed ? */
		command = rs_svc_cmd[RS_SVC_CMD_START];
	envp = svc_env();
	argv[4] = (char *)0, argv[3] = command;

	svcdeps = rs_svcdeps_load();
	/* initialize boot */
	if (RS_STAGE.level == 1 )
		rs_stage_start(1, argv, envp);
	/* fix a race condition for sysinit */
	if (RS_STAGE.level == 0 )
		rs_stage_start(3, argv, envp);

	/* do this extra loop to be able to stop stage-1 with RS_STAGE=3; so that,
	 * {local,network}fs services etc. can be safely stopped
	 */
	for (;;) { /* SHUTDOWN_LOOP */
		if (RS_STAGE.level == 3) {
			level = 3;
			RS_STAGE.level = 2;
			command = rs_svc_cmd[RS_SVC_CMD_STOP];
		}
		else if (level) {
			RS_STAGE.level = level;
			level = 0;
			command = rs_svc_cmd[RS_SVC_CMD_START];
		}
		argv[3] = command;

		for (k = 0; k < ARRAY_SIZE(rs_stage_type); k++) { /* STAGE_TYPE_LOOP */
			if (strcmp(command, rs_svc_cmd[RS_SVC_CMD_START]) == 0)
				j = RS_DEPTREE_PRIO-1;
			else
				j = 0, svc_start = 0;

			if (!type)
				RS_STAGE.type = rs_stage_type[k];
			deptree = rs_deptree_load();
			while (j >= 0 && j < RS_DEPTREE_PRIO) {
				svc_exec_list(deptree[j], argv, envp);
				if (svc_start)
					--j;
				else
					++j;
			} /* PRIORITY_LEVEL_LOOP */
			rs_deptree_free(deptree);

			/* skip irrelevant cases or because -[rv] passed */
			if (type)
				break;
		} /* STAGE_TYPE_LOOP */

		/* terminate remaining services before stage-3 */
		if (level)
			rs_stage_stop(argv, envp);
		else
			break;
	} /* SHUTDOWN_LOOP */

	/* finish sysinit */
	if (RS_STAGE.level == 0 )
		rs_stage_start(0, argv, envp);
	rs_svcdeps_free(svcdeps);
}

int main(int argc, char *argv[])
{
	prgname = strrchr(argv[0], '/');
	if (!prgname)
		prgname = argv[0];
	else
		prgname++;

	int opt;
	char on[8] = "1", off[8] = "0";

	/* Show help if insufficient args */
	if (argc < 2) {
		help_message(1);
	}

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
			case '1':
			case '2':
			case '3':
				setenv("RS_STAGE", argv[optind-1]+1, 1);
				break;
			case 'q':
				svc_quiet = 0;
				break;
			case 'r':
				setenv("RS_TYPE", rs_stage_type[RS_STAGE_RUNSCRIPT], 1);
				break;
			case 'V':
				printf("%s version %s\n\n", prgname, VERSION);
				puts(RS_COPYRIGHT);
				exit(EXIT_SUCCESS);
			case 'v':
				setenv("RS_TYPE", rs_stage_type[RS_STAGE_SUPERVISION], 1);
				break;
			case '?':
			case 'h':
				help_message(0);
				break;
			default:
				help_message(1);
				break;
		}
	}

	/* set this to avoid double waiting for a lockfile for supervision */
	setenv("SVC_WAIT", off, 1);

	if (strcmp(argv[optind], "stage") == 0) {
		/* set a few sane environment variables */
		svc_deps  = 1;
		svc_quiet = 0;
		unsetenv("SVC_DEPS");
		setenv("SVC_DEBUG", off, 1);
		setenv("RS_STRICT_DEP", off, 1);

		if (getenv("RS_STAGE"))
			svc_stage(argv[optind+1]);
		else {
			fprintf(stderr, "Usage: %s -(0|1|2|3) [-r|-v] stage [start|stop]"
					"(level argument required)\n", prgname);
			exit(EXIT_FAILURE);
		}
	}
	else {
		if ((argc-optind) < 2) {
			fprintf(stderr, "Usage: %s [OPTIONS] SERVICE COMMAND [ARGS]\n",
					prgname);
			exit(EXIT_FAILURE);
		}
		/* likewise, set a few sane environment variables */
		unsetenv("RS_STAGE");

		svc_exec(argc-optind, argv+optind);
	}

	exit(EXIT_SUCCESS);
}
