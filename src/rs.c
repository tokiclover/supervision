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
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>

#define VERSION "0.12.0"
#define RS_RUNSCRIPT SV_LIBDIR "/sh/runscript"

#define SV_TMPDIR_DOWN SV_TMPDIR "/down"
#define SV_TMPDIR_FAIL SV_TMPDIR "/fail"
#define SV_TMPDIR_PIDS SV_TMPDIR "/pids"
#define SV_TMPDIR_STAR SV_TMPDIR "/star"
#define SV_TMPDIR_WAIT SV_TMPDIR "/wait"

struct svcrun {
	char *name;
	char *path;
	pid_t pid;
	int lock;
};

struct slock {
	int f_fd;
	struct flock *f_lock;
};

static int svc_deps  = 0;
static int svc_quiet = 1;

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
	"COLUMNS", "LINES",	"RS_STAGE", "RS_STRICT_DEP", "RS_TYPE",
	"SVC_DEBUG", "SVC_DEPS", "SVC_WAIT", NULL
};

__NORETURN__ static void help_message(int exit_val);
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
static int svc_stage_init(int stage, const char *argv[], const char *envp[]);

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
static int svc_wait(const char *svc, int timeout, struct slock *lock);
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
	struct flock f_lock;
	struct slock s_lock;

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
					if (svc_wait(svc, timeout, NULL))
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

		f_lock.l_type   = F_WRLCK;
		f_lock.l_whence = SEEK_SET;
		f_lock.l_start  = 0;
		f_lock.l_len    = 0;
		s_lock.f_fd   = fd;
		s_lock.f_lock = &f_lock;
		if (fcntl(fd, F_GETLK, &f_lock) == -1) {
			ERR("%s: Failed to fcntl(%d, F_GETLK...): %s\n", svc, fd,
				strerror(errno));
			close(fd);
			return -errno;
		}
		if (f_lock.l_type == F_UNLCK)
			f_lock.l_type = F_WRLCK;
		else if (svc_wait(svc, timeout, &s_lock)) {
			close(fd);
			return -1;
		}
		if (fcntl(fd, F_SETLK, &f_lock) == -1) {
			ERR("%s: Failed to fcntl(%d, F_SETLK...): %s\n", svc, fd,
				strerror(errno));
			close(fd);
			return -errno;
		}
		return fd;
	}
	else if (lock_fd > 0)
		close(lock_fd);
	if (file_test(f_path, 0))
		unlink(f_path);
	return 0;
}

static int svc_wait(const char *svc, int timeout, struct slock *lock)
{
	int i, j;
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
			if (lock) {
				if (fcntl(lock->f_fd, F_GETLK, lock->f_lock) == -1)
					return -1;
				if (lock->f_lock->l_type == F_UNLCK)
					return 0;
			}
			/* use poll(3p) as a milliseconds timer (sleep(3) replacement) */
			if (poll(0, 0, SVC_WAIT_POLL) < 0)
				return -1;
		}
		WARN("waiting for %s (%d seconds)\n", svc, i+nsec);
	}
	return svc_state(svc, 'w');
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

static struct sigaction sa_sigint, sa_sigquit;
static sigset_t ss_savemask;

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
	const char *ptr;
	const char **envp;
	const char *argv[argc+3], *cmd = args[1], *svc;
	int i = 0, j, state = 0, status;
	int lock;
	pid_t pid;
	argv[i++] = "runscript";

	if (args[0][0] == '/') {
		ptr = args[0];
		svc = strrchr(args[0], '/')+1;
	}
	else {
		ptr = svc_find(args[0]);
		svc = args[0];
		if (ptr == NULL) {
			if (svc_quiet)
				WARN("%s: Inexistant service\n", args[0]);
			exit(EXIT_SUCCESS);
		}
	}

	/* do this before anything else */
	if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_ZAP]) == 0) {
		svc_zap(svc);
		exit(EXIT_SUCCESS);
	}

	/* setup argv and envp */
	argc--, args++;
	argv[i++] = ptr;
	for ( j = 0; j < argc; j++)
		argv[i++] = args[j];
	argv[i] = (char *)0;
	envp = svc_env();

	/* get service status before doing anything */
	if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_START]) == 0)
		state = 's';
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_STOP]) == 0)
		state = 'S';

	if (state == 's' || state == 'S') {
		if (strcmp(ptr, rs_stage_type[RS_STAGE_SUPERVISION]) == 0)
			status = svc_state(svc, 'p');
		else
			status = svc_state(svc, 's');

		if (status) {
			if (state == 's') {
				if (svc_quiet) {
					WARN("%s: Service is already started\n", svc);
					WARN("%s: Try zap command beforehand to force start up\n", svc);
				}
				exit(EXIT_SUCCESS);
			}
		}
		else if (state == 'S') {
			if (svc_quiet)
				WARN("%s: Service is not started\n", svc);
			exit(EXIT_SUCCESS);
		}

		if ((lock = svc_lock(svc, SVC_LOCK, SVC_WAIT_SECS)) < 0) {
			if (svc_quiet)
				ERR("%s: Failed to setup lockfile for service\n", svc);
			exit(EXIT_FAILURE);
		}
	}

	svc_sigsetup();
	pid = fork();

	if (pid > 0) {
		waitpid(pid, &status, 0);
		svc_lock(svc, lock, 0);
		if (WEXITSTATUS(status) == 0 && state)
			svc_mark(svc, state);
		else if (state == 's')
			svc_mark(svc, 'f');
		exit(WEXITSTATUS(status));
	}
	else if (pid == 0) {
		/* restore previous signal actions and mask */
		sigaction(SIGINT, &sa_sigint, NULL);
		sigaction(SIGQUIT, &sa_sigquit, NULL);
		sigprocmask(SIG_SETMASK, &ss_savemask, NULL);

		execve(RS_RUNSCRIPT, (char *const*)argv, (char *const*)envp);
		_exit(127);
	}

	ERROR("%s: Failed to fork()", __func__);
}

static int svc_exec_list(RS_StringList_T *list, const char *argv[], const char *envp[])
{
	RS_String_T *svc;
	pid_t pid;
	int count = 0, status, retval = 0, i;
	static int parallel, type;
	struct svcrun **svclist = NULL;
	int state;
	static char type_rs[8], type_sv[8];
	static size_t len;

	if (list == NULL) {
		errno = ENOENT;
		return -1;
	}
	if (argv == NULL || envp == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (len == 0) {
		parallel = rs_conf_yesno("RS_PARALLEL");
		len = strlen(RS_SVCDIR);
		snprintf(type_rs, sizeof(type_rs), "--%s", rs_stage_type[RS_STAGE_RUNSCRIPT]);
		snprintf(type_sv, sizeof(type_sv), "--%s", rs_stage_type[RS_STAGE_SUPERVISION]);
	}
	svc_sigsetup();

	if (strcmp(argv[3], rs_svc_cmd[RS_SVC_CMD_START]) == 0)
		state = 's';
	else
		state = 'S';

	SLIST_FOREACH(svc, list, entries) {
		argv[2] = svc_find(svc->str);
		if (argv[2] == NULL)
			continue;
		if (strncmp(RS_SVCDIR, argv[2], len) == 0) {
			type = 0;
			argv[1] = type_rs;
		}
		else {
			type = 1;
			argv[1] = type_sv;
		}

		if (type)
			status = svc_state(svc->str, 'p');
		else
			status = svc_state(svc->str, 's');
		if (status) {
			if (state == 's') {
				DBG("%s: Service is already started\n", svc->str);
				free((void*)argv[2]);
				continue;
			}
		}
		else if (state == 'S') {
			DBG("%s: Service is not started\n", svc->str);
			free((void*)argv[2]);
			continue;
		}

		pid = fork();
		if (pid > 0) { /* parent */
			if (parallel) {
				svclist = err_realloc(svclist, sizeof(void*) * (count+1));
				svclist[count] = err_malloc(sizeof(struct svcrun));
				svclist[count]->name = svc->str;
				svclist[count]->path = (char*)argv[2];
				svclist[count]->pid = pid;
				count++;
			}
			else {
				waitpid(pid, &status, 0);
				svc_mark(svc->str, 'W');
				if (WIFEXITED(status))
					svc_mark(svc->str, state);
				else
					retval++;
				free((void*)argv[2]);
			}
		}
		else if (pid == 0) { /* child */
			/* restore previous signal actions and mask */
			sigaction(SIGINT, &sa_sigint, NULL);
			sigaction(SIGQUIT, &sa_sigquit, NULL);
			sigprocmask(SIG_SETMASK, &ss_savemask, NULL);

			int lock;
			char *svc_name = strrchr(argv[2], '/')+1;
			if ((lock = svc_lock(svc_name, SVC_LOCK, SVC_WAIT_SECS)) < 0) {
				DBG("%s: Failed to setup lockfile for service\n", svc_name);
				_exit(EXIT_FAILURE);
			}

			execve(RS_RUNSCRIPT, (char *const*)argv, (char *const*)envp);
			_exit(127);
		}
		else
			ERROR("%s: fork()", __func__);
	}

	for (i = 0; i < count; i++) {
		waitpid(svclist[i]->pid, &status, 0);
		svc_mark(svclist[i]->name, 'W');
		if (WEXITSTATUS(status)) {
			retval++;
			if (state == 's')
				svc_mark(svclist[i]->name, 'f');
		}
		else
			svc_mark(svclist[i]->name, state);
		free(svclist[i]->path);
		free(svclist[i]);
	}
	free(svclist);

	return retval;
}

static int svc_stage_init(int stage, const char *argv[], const char *envp[])
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

	if (RS_STAGE.level == 0 || RS_STAGE.level == 3) { /* force stage type */
		setenv("RS_TYPE", rs_stage_type[RS_STAGE_RUNSCRIPT], 1);
		RS_STAGE.type = rs_stage_type[RS_STAGE_RUNSCRIPT];
		command = rs_svc_cmd[RS_SVC_CMD_START];
	}
	if (RS_STAGE.type == NULL) /* -r|-v passed ? */
		type = 0;
	if (command == NULL) /* start|stop passed ? */
		command = rs_svc_cmd[RS_SVC_CMD_START];
	if (strcmp(command, rs_svc_cmd[RS_SVC_CMD_START]) == 0)
		j = RS_DEPTREE_PRIO-1;
	else
		j = 0, svc_start = 0;

	envp = svc_env();
	argv[4] = (char *)0, argv[3] = command;

	/* initialize boot */
	if (RS_STAGE.level == 1 )
		svc_stage_init(1, argv, envp);
	/* fix a race condition for sysinit */
	if (RS_STAGE.level == 0 )
		svc_stage_init(3, argv, envp);

	for (k = 0; k < ARRAY_SIZE(rs_stage_type); k++) {
		if (!type) {
			RS_STAGE.type = rs_stage_type[k];
			setenv("RS_TYPE", rs_stage_type[k], 1);
		}
		deptree = rs_deptree_load();

		while (j >= 0 && j < RS_DEPTREE_PRIO) {
			svc_exec_list(deptree[j], argv, envp);
			if (svc_start)
				--j;
			else
				++j;
		}
		rs_deptree_free(deptree);

		/* skip irrelevant cases or because -[rv] passed */
		if (type)
			break;
	}

	/* finish sysinit */
	if (RS_STAGE.level == 0 )
		svc_stage_init(0, argv, envp);
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
		setenv("SVC_DEPS", on, 1);
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
