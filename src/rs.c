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
#include <sys/wait.h>
#include <poll.h>
#include <time.h>

#define VERSION "0.12.0"
#define RS_RUNSCRIPT SV_LIBDIR "/sh/runscript"

#define SV_TMPDIR_DOWN SV_TMPDIR "/down"
#define SV_TMPDIR_FAIL SV_TMPDIR "/fail"
#define SV_TMPDIR_PIDS SV_TMPDIR "/pids"
#define SV_TMPDIR_STAR SV_TMPDIR "/star"
#define SV_TMPDIR_WAIT SV_TMPDIR "/wait"

struct svcrun {
	RS_SvcDeps_T *depends;
	const char *name;
	const char *path;
	pid_t pid;
	int lock;
	int argc;
	const char **argv;
	const char **envp;
};

int rs_stage = -1;
static int rs_runlevel = -1;
static int svc_deps  = 1;
static int svc_quiet = 1;
static RS_StringList_T *svclist;

/* list of service to start/stop before|after a stage */
static const char *const rs_init_stage[][4] = {
	{ "clock", "hostname", NULL },
};

/* !!! order matter (defined constant/enumeration) !!! */
const char *const rs_stage_name[] = { "sysinit", "boot", "default", "shutdown",
	"reboot", "single", "nonetwork", NULL
};
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

static const char *shortopts = "Dg0123qvh";
static const struct option longopts[] = {
	{ "nodeps",   0, NULL, 'D' },
	{ "debug",    0, NULL, 'g' },
	{ "sysinit",  0, NULL, '0' },
	{ "boot",     0, NULL, '1' },
	{ "default",  0, NULL, '2' },
	{ "shutdown", 0, NULL, '3' },
	{ "quiet",    0, NULL, 'q' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Disable dependencies",
	"Enable debug mode",
	"Select stage-0 run level",
	"Select stage-1 run level",
	"Select stage-2 run level",
	"Select stage-3 run level",
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
	"RS_RUNLEVEL", "RS_STAGE", NULL
};

__NORETURN__ static void help_message(int exit_val);

#define SVC_CMD_FIND 1
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
#define LOG_ERR(fmt, ...)  svc_log("ERROR: %s: " fmt, PRGNAME, __VA_ARGS__)
#define LOG_WARN(fmt, ...) svc_log( "WARN: %s: " fmt, PRGNAME, __VA_ARGS__)
#define LOG_INFO(fmt, ...) svc_log( "INFO: %s: " fmt, PRGNAME, __VA_ARGS__)

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
 * @svc: service name;
 * @status: int value [dfrs]
 * @return: 0 on success;
 */
static int svc_mark(const char *svc, int status);

/* simple rc compatible runlevel handler*/
static void svc_level(void);
static char *get_cmdline_entry(const char *entry);

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
static int svc_exec_list(RS_StringList_T *list, int argc, const char *argv[],
		const char *envp[]);

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

static int svc_cmd(struct svcrun *run, int flags)
{
	static char nodeps_arg[16] = "--nodeps", deps_arg[8] = "--deps";
	static char type_rs[8] = "--rs", type_sv[8] = "--sv";
	static int setup = 1;
	int status, command, retval = 1;
	int find = flags & SVC_CMD_FIND;
	pid_t pid;
	int deps = 0, i;
	const char **argv = NULL;
	const char *cmd = run->argv[4];
	char *path;

	if (setup) {
		svc_sigsetup();
		setup = 0;
	}

	if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_START]) == 0)
		command = 's', deps = 1;
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_STOP]) == 0)
		command = 'S';
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_ADD]) == 0)
		command = 'a';
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_DEL]) == 0)
		command = 'd';
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_ZAP]) == 0) {
		svc_zap(run->name);
		return 0;
	}
	else if (strcmp(cmd, rs_svc_cmd[RS_SVC_CMD_STATUS]) == 0) {
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

	/* this is done before because of a possible virtual provider */
	if (command == 's' || command == 'S' ) {
		status = svc_state(run->name, 's');
		if (status) {
			if (command == 's') {
				LOG_WARN("%s: Service is already started\n", run->name);
				return -EBUSY;
			}
		}
		else if (command == 'S') {
			LOG_WARN("%s: Service is not started\n", run->name);
			return -EINVAL;
		}
	}

	argv = err_calloc(run->argc, sizeof(void*));
	for (i = 0; i <= run->argc; i++)
		argv[i] = run->argv[i];
	/* get service path */
	if (find) {
		/* find a real service instead of a virtual */
		if (rs_stage < 0) {
			if (run->depends && run->depends->virt)
				run->name = run->depends->svc;
		}
		run->path = argv[3] = svc_find(run->name);
		if (argv[3] == NULL)
			return -ENOENT;
	}
	if (file_test(argv[3], 'd'))
		argv[1] = type_sv;
	else
		argv[1] = type_rs;
	argv[2] = deps_arg;

	/* get service status */
	switch(command) {
	case 'a':
	case 'd':
		if (rs_stage < 0) {
			fprintf(stderr, "%s: stage level argument is required\n", prgname);
			fprintf(stderr, "Usage: %s -(0|1|2|3) %s COMMAND\n", prgname,
					run->name);
			retval = 1;
			goto reterr;
		}

		path = err_malloc(256*sizeof(char));
		snprintf(path, 256, "%s/.stage-%d/%s", SV_SVCDIR, rs_stage,
				run->name);
		if (!access(path, F_OK)) {
			if (command == 'd')
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

	/* setup dependencies */
	if (deps) {
		retval = svc_depend(run);
		if (retval == -ENOENT)
			;
		else if (retval) {
			LOG_ERR("%s: Failed to set up service dependencies\n", run->name);
			retval = -ECANCELED;
			goto reterr;
		}
		else
			argv[2] = nodeps_arg;
	}

	if (!svc_quiet)
		svc_log("[%s] service %s...\n", run->name, cmd);

	pid = fork();
	if (pid > 0) { /* parent */
		close(run->lock);
		run->pid = pid;

		if (flags & SVC_CMD_WAIT) {
			waitpid(pid, &status, 0);
			if (command)
				svc_mark(run->name, 'W');

			retval = WEXITSTATUS(status);
			if (!svc_quiet)
				svc_end(run->name, retval);
			if (!retval && command) {
				svc_mark(run->name, command);
				if (run->depends && run->depends->virt)
					svc_mark(run->depends->virt, command);
			}
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

		switch (command) {
		case 's':
		case 'S':
			if ((run->lock = svc_lock(run->name, SVC_LOCK, SVC_WAIT_SECS)) < 0) {
				LOG_ERR("%s: Failed to setup lockfile for service\n", run->name);
				_exit(ENOLCK);
			}
			/* close the lockfile to be able to mount rootfs read-only */
			if (rs_stage == 3 && command == 's')
				close(run->lock);
			break;
		}

		execve(RS_RUNSCRIPT, (char *const*)argv, (char *const*)run->envp);
		_exit(255);
	}
	else
		ERROR("%s: Failed to fork(): %s\n", __func__, strerror(errno));

reterr:
	if (find)
		free((void *)run->path);
	free(argv);
	return retval;
}

static int svc_depend(struct svcrun *run)
{
	int type, val, retval = 0;
	int p = 0;
	RS_DepTree_T deptree;
	memset(&deptree, 0, sizeof(RS_DepTree_T));

	if (svc_deps == 0)
		return 0;
	else
		svc_deps = 0; /* override this for svc_exec dependencies */
	if (!run->depends)
		run->depends = rs_svcdeps_find(service_deplist, run->name);
	if (!run->depends)
		return -ENOENT;

	/* skip before deps type */
	for (type = RS_DEPS_USE; type < RS_DEPS_TYPE; type++) {
		if (SLIST_EMPTY(run->depends->deps[type]))
			continue;
		/* build a deptree to avoid segfault because cyclical dependencies */
		svclist = deptree.list = run->depends->deps[type];
		svc_deptree_load(&deptree);
		while (p >= 0 && p < deptree.size) { /* PRIORITY_LEVEL_LOOP */
			if (!SLIST_EMPTY(deptree.tree[p]))
				val = svc_exec_list(deptree.tree[p], run->argc, run->argv, run->envp);
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
	const char **envp;
	size_t size = 1024;
	char *env = err_malloc(size);
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

	envp[i++] = (char *)0;
	envp[i++] = (char *)0;
	return envp;
}

static char *svc_find(const char *svc)
{
	char *buf = err_malloc(BUFSIZ), *ptr = NULL;
	int i;
	int err = errno;

	if (!svc)
		return NULL;

	for (i = 0; i <= 3; i++) {
		snprintf(buf, BUFSIZ, "%s/.stage-%d/%s", SV_SVCDIR, i, svc);
		if (!access(buf, F_OK)) {
			ptr = buf;
			break;
		}
	}
	if (!ptr) {
		snprintf(buf, BUFSIZ, "%s/%s", SV_SVCDIR, svc);
		if (!access(buf, F_OK))
			ptr = buf;
	}
	if (ptr)
		ptr = err_realloc(buf, strlen(buf)+1);
	else
		free(buf);

	errno = err;
	return ptr;
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

static char *get_cmdline_entry(const char *ent)
{
#ifdef __linux__
	FILE *fp;
	char *line, *ptr, path[] = "/proc/cmdline", *val = NULL;
	size_t size, len;

	if (!access(path, F_OK))
		return NULL;
	if (!(fp = fopen(path, "r")))
		return NULL;
	if (!rs_getline(fp, &line, &size))
		return NULL;

	len = strlen(ent);
	ptr = line;
	while ((ptr = strsep(&ptr, " "))) {
		if (strncmp(ent, ptr, len) == 0) {
			ptr += len;
			switch(ptr[len]) {
			case '=':
				ptr++;
			case ' ':
			case '\0':
				val = err_strdup(ptr);
				break;
			default:
				continue;
			}
		}
	}
	fclose(fp);
	free(line);
	return val;
#else
	return NULL;
#endif /* __linux__ */
}

static void svc_level(void)
{
	char *entry = NULL, path[128], *ptr;
	int i;

	entry = get_cmdline_entry("softlevel");
	if (rs_stage == 1) {
		/* mark network services as started, so nothing will be started */
		if ((entry && strcmp(entry, rs_stage_name[RS_STAGE_NONETWORK]) == 0) ||
		    (rs_runlevel == RS_STAGE_NONETWORK)) {
			for (i = 0; i < rs_virtual_count; i++)
				if (strcmp(virtual_deplist[i]->virt, "net") == 0)
					svc_mark(virtual_deplist[i]->svc, 's');
			svc_mark("net", 's');
		}
		else if ((entry && strcmp(entry, rs_stage_name[RS_STAGE_SINGLE]) == 0) ||
		    (rs_runlevel == RS_STAGE_SINGLE)) {
			snprintf(path, sizeof(path), "%s/.%s", SV_SVCDIR,
					rs_stage_name[RS_STAGE_SINGLE]);
			rs_svclist_load(path);
		}
		goto noinit;
	}
	else if (rs_stage == 3) {
		rs_svclist_load(SV_TMPDIR_STAR);
		goto noinit;
	}

	free(entry);

noinit:
	entry = get_cmdline_entry("noinit");
	if (!entry)
		return;
	if (rs_stage == 1)
		i = 's';
	else if (rs_stage == 3)
		i = 'S';
	/* mark no started services as stopped */
	while ((ptr = strsep(&entry, ",")))
		svc_mark(ptr, i);
	free(entry);
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
	char path[BUFSIZ];
	char *dirs[] = { SV_TMPDIR_DOWN, SV_TMPDIR_FAIL,
		SV_TMPDIR_PIDS, SV_TMPDIR_STAR,
		SV_TMPDIR_WAIT, NULL };
	const char *files[] = { "ENV", "OPTIONS", NULL };

	for (i = 0; dirs[i]; i++) {
		snprintf(path, sizeof(path), "%s/%s", dirs[i], svc);
		if (!access(path, F_OK))
			unlink(path);
	}

	for (i = 0; files[i]; i++) {
		snprintf(path, sizeof(path), "%s/%s_%s", SV_TMPDIR, svc, files[i]);
		if (!access(path, F_OK))
			unlink(path);
	}
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
			if (!access(path, F_OK))
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
	return !access(path, F_OK);
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

__NORETURN__ static int svc_exec(int argc, char *argv[]) {
	int i = 0, j, retval;
	int cmd_flags = SVC_CMD_WAIT;
	struct svcrun run;

	run.argc = 8*(argc/8)+8+ (argc%8) > 4 ? 8 : 0;
	run.argv = err_malloc(run.argc*sizeof(void*));

	if (!service_deplist)
		rs_svcdeps_load();
	if (argv[0][0] == '/') {
		run.argv[3] = argv[0];
		run.name = strrchr(argv[0], '/')+1;
	}
	else {
		cmd_flags |= SVC_CMD_FIND;
		run.name = argv[0];
	}
	run.argv[0] = "runscript";
	run.argv[4] = argv[1];

	/* setup argv and envp */
	argc -= 2, argv += 2;
	i = 5;
	for ( j = 0; j < argc; j++)
		run.argv[i++] = argv[j];
	run.argv[i] = (char *)0;
	run.envp = svc_env();
	run.depends = NULL;

	retval = svc_cmd(&run, cmd_flags);
	switch(retval) {
	case -EBUSY:
	case -EINVAL:
		exit(EXIT_SUCCESS);
	case -ENOENT:
		exit(2);
	case -ECANCELED:
		exit(4);
	default:
		exit(retval);
	}
}

static int svc_exec_list(RS_StringList_T *list, int argc, const char *argv[],
		const char *envp[])
{
	RS_String_T *svc;
	size_t n = 0, size = 8;
	int retval = 0;
	int i, r, state, status;
	static int parallel, setup, cmd_flags = SVC_CMD_FIND;
	struct svcrun **run;

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
	if (strcmp(argv[4], rs_svc_cmd[RS_SVC_CMD_START]) == 0)
		state = 's';
	else
		state = 'S';

	run = err_malloc(size*sizeof(void*));
	SLIST_FOREACH(svc, list, entries) {
		run[n]  = err_malloc(sizeof(struct svcrun));
		run[n]->name = svc->str;
		run[n]->depends = rs_virtual_find(svc->str, svclist);
		run[n]->argc = argc;
		run[n]->argv = argv;
		run[n]->envp = envp;

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
		else if (r)
			retval++;
	}

	for (i = 0; i < n; i++) {
		waitpid(run[i]->pid, &status, 0);
		svc_mark(run[i]->name, 'W');
		if (WEXITSTATUS(status)) {
			retval++;
			if (state == 's')
				svc_mark(run[i]->name, 'f');
		}
		else {
			svc_mark(run[i]->name, state);
			if (run[i]->depends && run[i]->depends->virt)
				svc_mark(run[i]->depends->virt, state);
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

	retval = svc_exec_list(init_stage_list, argc, argv, envp);
	rs_stringlist_free(&init_stage_list);

	return retval;
}

static void svc_stage(const char *cmd)
{
	RS_DepTree_T deptree;
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
	memset(&deptree, 0, sizeof(RS_DepTree_T));
	svc_deps  = 1;
	svc_quiet = 0;
	unsetenv("SVC_DEPS");
	unsetenv("SVC_DEBUG");
	snprintf(buf, sizeof(buf), "%d", rs_stage);
	setenv("RS_STAGE", buf, 1);
	snprintf(buf, sizeof(buf), "%s/%d_deptree", SV_TMPDIR_DEPS, rs_stage);

	if (rs_stage == 0) /* force service command */
		command = rs_svc_cmd[RS_SVC_CMD_START];
	if (command == NULL) /* start|stop passed ? */
		command = rs_svc_cmd[RS_SVC_CMD_START];
	if (strcmp(command, rs_svc_cmd[RS_SVC_CMD_STOP]) == 0)
		svc_start = 0;

	envp = svc_env();
	argv[5] = (char *)0;
	svc_level();
	chdir("/");

	t = time(NULL);
	rs_debug = 1;
	svc_log("logging: %s command\n", command);
	svc_log("rs init stage-%d started at %s\n", rs_stage, ctime(&t));

	/* do this extra loop to be able to stop stage-1 with RS_STAGE=3; so that,
	 * {local,network}fs services etc. can be safely stopped
	 */
	for (;;) { /* SHUTDOWN_LOOP */
		if (rs_stage == 3 && !level) {
			level = rs_stage;
			/* load the started services instead of only stage-[12]
			 * to be abe to shutdown everything with RS_STAGE=3
			 */
			deptree.list = rs_svclist_load(SV_TMPDIR_STAR);
			command = rs_svc_cmd[RS_SVC_CMD_STOP];
			svc_start = 0;
		}
		else if (level == 3) {
			level = 0;
			/* close the logfile because rootfs will be mounted read-only */
			if (!fclose(logfp) || !close(logfd))
				logfd = 0, logfp = NULL;
			/* and finaly start stage-3 */
			command = rs_svc_cmd[RS_SVC_CMD_START];
			svc_start = 1;
			/* avoid starting everything after stopping */
			rs_stringlist_free(&deptree.list);
			deptree.list = NULL;
			unlink(buf);
		}
		argv[4] = command;

		t = time(NULL);
		svc_log( "\n\tstage-%d (%s) at %s\n", rs_stage, command, ctime(&t));

		rs_deptree_load(&deptree);
		svclist = deptree.list;
		if (svc_start)
			p = deptree.size-1;
		else
			p = 0;
		while (p >= 0 && p < deptree.size) { /* PRIORITY_LEVEL_LOOP */
			if (!SLIST_EMPTY(deptree.tree[p])) {
				t = time(NULL);
				svc_log("\n\tpriority-level-%d started at %s\n", p,	ctime(&t));
				r = svc_exec_list(deptree.tree[p], argc, argv, envp);
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
		rs_deptree_free(&deptree);

		/* break shutdown loop */
		if (!level)
			break;
	} /* SHUTDOWN_LOOP */

	/* finish sysinit */
	if (rs_stage == 0 )
		svc_stage_command(0, argc, argv, envp);

	t = time(NULL);
	svc_log("\nrs init stage-%d stopped at %s\n", rs_stage, ctime(&t));
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
				rs_stage = atoi(argv[optind-1]+1);
				break;
			case 'q':
				svc_quiet = 0;
				break;
			case 'v':
				printf("%s version %s\n\n", prgname, VERSION);
				puts(RS_COPYRIGHT);
				exit(EXIT_SUCCESS);
			case '?':
			case 'h':
				help_message(0);
				break;
			default:
				help_message(1);
				break;
		}
	}
	if ((argc-optind) < 1)
		help_message(1);

	unsetenv("RS_STAGE");
	unsetenv("RS_RUNLEVEL");
	/* set this to avoid double waiting for a lockfile for supervision */
	setenv("SVC_WAIT", off, 1);

	if (strcmp(argv[optind], "stage") == 0) {
		if (rs_stage >= 0)
			svc_stage(argv[optind+1]);
		else {
			fprintf(stderr, "Usage: %s -(0|1|2|3) stage [start|stop]"
					"(level argument required)\n", prgname);
			exit(EXIT_FAILURE);
		}
	}
	else if (argc-optind == 1) {
		/* enable sysvinit compatiblity command line */
		for (opt = 0; rs_stage_name[opt]; opt++) {
			if (strcmp(argv[optind], rs_stage_name[opt]) == 0) {
				switch(opt) {
				case RS_STAGE_REBOOT:
				case RS_STAGE_SHUTDOWN:
					rs_stage = 3;
					break;
				case RS_STAGE_DEFAULT:
				case RS_STAGE_SINGLE:
					rs_stage = 2;
					break;
				case RS_STAGE_NONETWORK:
				case RS_STAGE_BOOT:
					rs_stage = 1;
					break;
				case RS_STAGE_SYSINIT:
					rs_stage = 0;
					break;
				}
				rs_runlevel = opt;
				setenv("RS_RUNLEVEL", rs_stage_name[opt], 1);
				svc_stage(NULL);
			}
		}
		if (rs_runlevel == -1) {
			fprintf(stderr, "Usage: %s [OPTIONS] SERVICE COMMAND [ARGUMENTS] "
					"(service command)\n", prgname);
			fprintf(stderr, "       %s -{0|1|2|3} stage "
					"(init-stage)\n", prgname);
			fprintf(stderr, "       %s {sysinit|boot|default|shutdown|reboot} "
					"(run levels)\n", prgname);
			exit(EXIT_FAILURE);
		}
	}
	else {
		/* handle service command */
		if ((argc-optind) < 2) {
			fprintf(stderr, "Usage: %s [OPTIONS] SERVICE COMMAND [ARGS]\n",
					prgname);
			exit(EXIT_FAILURE);
		}
		svc_exec(argc-optind, argv+optind);
	}

	exit(EXIT_SUCCESS);
}
