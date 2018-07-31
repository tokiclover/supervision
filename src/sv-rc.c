/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-rc.c  0.14.0 2018/07/26
 */

#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __FreeBSD__
# include <sys/sysctl.h>
#endif
#include "sv-deps.h"

/* execute a service command (low level) */
extern int svc_cmd(struct svcrun *run);
/* execute a service command (high level) */
extern int svc_exec (int argc, const char *argv[]);
/* the same for a list of service */
extern int svc_execl(SV_StringList_T *list, int argc, const char *argv[]);
/* mark service status */
static int svc_mark_simple(char *svc, int status, const char *what);

/* signal handleer/setup */
sigset_t ss_child, ss_full, ss_null, ss_old;
static void sv_sighandler(int sig, siginfo_t *si __attribute__((__unused__)), void *ctx __attribute__((__unused__)));
static void sv_sigsetup(void);

static int sv_system_detect(void);

int sv_debug    =  0;
int sv_parallel =  0;
int sv_level    = -1;
int sv_init     = -1;
int sv_system = 0;
pid_t sv_pid  = 0;
int svc_deps  = 1;
int svc_quiet = 1;
SV_DepTree_T DEPTREE = { NULL, NULL, 0, 0 };

FILE *debugfp;
static int debugfd = STDERR_FILENO;
static FILE *logfp;
static int logfd;

/* list of service to start/stop before|after a init level */
static const char *const sv_run_level[][8] = {
	{ NULL },
	{ NULL },
	{ NULL },
	{ NULL },
	{ "clock", "hostname", NULL },
	{ NULL },
	{ NULL },
	{ NULL },
};

/* !!! order matter (defined constant/enumeration) !!! */
const char *const sv_init_level[] = { "shutdown", "single", "nonetwork",
	"default", "sysinit", "sysboot", "reboot", NULL
};

const char *progname;
static const char *applet;

static const char *shortopts = "0123456DdNSbdhlprsqvx";
static const struct option longopts[] = {
	{ "shutdown", 0, NULL, 'p' },
	{ "single",   0, NULL, 's' },
	{ "nonetwork",0, NULL, 'N' },
	{ "default",  0, NULL, 'l' },
	{ "sysinit",  0, NULL, 'S' },
	{ "sysboot",  0, NULL, 'b' },
	{ "reboot",   0, NULL, 'r' },
	{ "nodeps",   0, NULL, 'D' },
	{ "debug",    0, NULL, 'd' },
	{ "trace",    0, NULL, 'x' },
	{ "quiet",    0, NULL, 'q' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Select shutdown    run level",
	"Select single user run level",
	"Select nonetwork   run level",
	"Select multi users run level",
	"Select system init run level",
	"Select system boot run level",
	"Select reboot      run level",
	"Disable dependencies",
	"Enable debug mode",
	"Enable shell trace",
	"Enable quiet mode",
	"Show help and exit",
	"Show version and exit",
	NULL
};

__attribute__((__noreturn__)) static void help_message(int retval);

/*
 * bring system to a named init level or init stage
 * @cmd (start|stop|NULL); NULL is the default command
 */
static void svc_init(const char *cmd);

/*
 * execute a service list (called from svc_init())
 * to finish/start particular init levels or stages
 * @return 0 on success or number of failed services
 */
static int svc_init_command(int level, int argc, const char *argv[]);

/* simple rc compatible runlevel handler*/
static void svc_level(void);
static char *get_cmdline_option(const char *entry);

static int svc_mark_simple(char *svc, int status, const char *what)
{
	char path[PATH_MAX], *ptr;
	int fd;
	mode_t m;

	if (!svc)
		return -ENOENT;

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
		case SV_SVC_STAT_DOWN:
		case SV_SVC_STAT_FAIL:
		case SV_SVC_STAT_WAIT:
			snprintf(path, sizeof(path), "%s/%s", ptr, svc);
			m = umask(0);
			fd = open(path, O_CREAT|O_WRONLY|O_NONBLOCK, 0644);
			umask(m);
			if (fd > 0) {
				if (what)
					(void)err_write(fd, (const char*)what, (const char*)path);
				close(fd);
				return 0;
			}
		default:
			return -1;
	}
}
/* simple helper to set/get runlevel
 * @level: runlevel to set, or NULL to get the current runlevel;
 * @return: current runlevel;
 */
static const char *svc_run_level(const char *level);

__attribute__((__noreturn__)) static void help_message(int retval)
{
	int i;

	if (!strcmp(progname, "sv-rc") || !strcmp(progname, "rc"))
		printf("Usage: %s [OPTIONS] (RUNLEVEL)\n", progname);
	else
		printf("Usage: %s [OPTIONS] SERVICE COMMAND [ARGUMENTS]\n", progname);
	for ( i = 0; i < 7; i++)
		printf("    -%d, -%c, --%-12s %s\n", i, longopts[i].val, longopts[i].name,
				longopts_help[i]);
	for ( i = 7; longopts_help[i]; i++)
		printf("        -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);

	exit(retval);
}

static char *get_cmdline_option(const char *ent)
{
#ifdef __linux__
	FILE *fp;
	char *line = NULL, *ptr, path[] = "/proc/cmdline", *val = NULL;
	size_t len = 0;

	if (access(path, F_OK))
		return NULL;
	if (!(fp = fopen(path, "r")))
		return NULL;
	if (getline(&line, &len, fp) <= 0)
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

static const char *svc_run_level(const char *level)
{
	static char buf[16], path[] = SV_TMPDIR "/softlevel";
	const char *retval;
	int fd, flags = O_NONBLOCK|O_CREAT, len;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%s)\n", __func__, level);
#endif

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
		if (err_write(fd, (const char*)level, (const char*)path))
			retval = NULL;
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
	char *entry = NULL, *ptr, *ent;
	int i;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif

	entry = get_cmdline_option("softlevel");

	/* mark network services as started, so nothing will be started */
	if ((entry && strcmp(entry, sv_init_level[SV_NOWNETWORK_LEVEL]) == 0) ||
		(sv_level == SV_NOWNETWORK_LEVEL)) {
		sv_level = SV_NOWNETWORK_LEVEL;
		for (i = 0; i < SERVICES.virt_count; i++)
			if (strcmp(SERVICES.virt_svcdeps[i]->virt, "net") == 0)
				svc_mark_simple(SERVICES.virt_svcdeps[i]->svc, SV_SVC_STAT_STAR, NULL);
		svc_mark_simple("net", SV_SVC_STAT_STAR, NULL);
	}
	else if ((entry && strcmp(entry, sv_init_level[SV_SINGLE_LEVEL]) == 0)) {
		sv_level = SV_SINGLE_LEVEL;
	}

	if (entry) free(entry);
	entry = ent = get_cmdline_option("noinit");
	/* mark no started services as stopped */
	if (entry) {
		while ((ptr = strsep(&ent, ",")))
			svc_mark_simple(ptr, SV_SVC_STAT_STAR, NULL);
		free(entry);
	}
}

int svc_end(const char *svc, int status)
{
	static char ok[] = "ok", no[] = "no", *m;
	if (status)
		m = no;
	else
		m = ok;
	return svc_log("(%s) [%s]\n", svc, m);
}

__attribute__((format(printf,1,2))) int svc_log(const char *fmt, ...)
{
	static char logfile[] = "/var/log/sv.log", *logpath;
	int retval = 0;
	va_list ap;

	/* save logfile if necessary */
	if (!logpath && sv_conf_yesno("SV_LOGGER"))
		logpath = logfile;
	else
		logpath = SV_LOGFILE;
	if (!logfd) {
		logfd = open(logpath, O_NONBLOCK|O_CREAT|O_RDWR|O_CLOEXEC, 0644);
		if (logfd < 0)
			logfd = open(SV_LOGFILE, O_NONBLOCK|O_CREAT|O_RDWR|O_CLOEXEC, 0644);
		if (logfd > 0) {
			logfp = fdopen(logfd, "a+");
			if ((debugfd = dup(logfd)) > 0) {
				if (!(debugfp = fdopen(debugfd, "a+"))) debugfp = logfp;
			}
			else debugfd = STDERR_FILENO;
		}
	}

	va_start(ap, fmt);
	if (svc_quiet) {
		retval = vfprintf(stderr, fmt, ap);
		va_end(ap);
		va_start(ap, fmt);
	}
	if (logfp)
		retval = vfprintf(logfp , fmt, ap);
	va_end(ap);
	return retval;
}

void sv_cleanup(void)
{
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif
	if (!access(SV_ENVIRON, F_OK))
		unlink(SV_ENVIRON);
	if (!access(SV_PIDFILE, F_OK))
		unlink(SV_PIDFILE);
}

static void sv_sighandler(int sig, siginfo_t *si __attribute__((__unused__)), void *ctx __attribute__((__unused__)))
{
	int i = -1, serrno = errno;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%d, %p, %p)\n", __func__, sig, si, ctx);
#endif

	switch (sig) {
	case SIGINT:
		if (i < 0) i = 1;
	case SIGTERM:
		if (i < 0) i = 3;
	case SIGQUIT:
		if (i < 0) i = 2;
		ERR("caught %s, aborting\n", signame[i]);
		kill(0, sig);
		exit(EXIT_FAILURE);
	case SIGUSR1:
	case SIGUSR2:
		fprintf(stderr, "%s: Aborting!\n", progname);
		/* block child signals */
		sigprocmask(SIG_SETMASK, &ss_child, NULL);

		/* kill any worker process we have started */
		kill(0, SIGTERM);

		if (sig == SIGUSR2)
			exit(EXIT_SUCCESS);
		exit(EXIT_FAILURE);
	default:
		ERR("caught unknown signal %d\n", sig);
	}

	/* restore errno */
	errno = serrno;
}

static void sv_sigsetup(void)
{
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	int i;
	int sig[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGUSR1, SIGUSR2, 0 };

	sigfillset(&ss_full);
	sigemptyset(&ss_null);
	sigemptyset(&ss_old);
	sigemptyset(&ss_child);
	sigaddset(&ss_child, SIGCHLD);
	sigprocmask(SIG_SETMASK, &ss_child, &ss_old);

	sa.sa_sigaction = sv_sighandler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NOCLDSTOP;
	sigemptyset(&sa.sa_mask);
	for (i = 0; sig[i]; i++)
		if (sigaction(sig[i], &sa, NULL))
			ERROR("%s: sigaction(%s ...)", __func__, signame[i]);
}

static int sv_system_detect(void)
{
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif
#ifdef __FreeBSD__
	int jail;
	size_t len = sizeof(jail);
	if (sysctlbyname("security.jail.jailed", &jail, &len, NULL, 0) == 0)
		if (jail == 1)
			return SV_KEYWORD_JAIL;
#endif

#ifdef __NetBSD__
	if (!access("/kern/xen/privcmd", F_OK))
		return SV_KEYWORD_XEN0;
	if (!access("/kern/xen", F_OK))
		return SV_KEYWORD_XENU;
#endif

#ifdef __linux__
	char buf[32];
	int *cid = (int []){
		SV_KEYWORD_DOCKER,
		SV_KEYWORD_OPENVZ,
		SV_KEYWORD_LXC,
		SV_KEYWORD_SYSTEMD_NSPAWN,
		SV_KEYWORD_UML,
		SV_KEYWORD_VSERVER,
		0
	};
	do {
		snprintf(buf, sizeof(buf), "container=%s", sv_keywords[*cid]);
		if (!file_regex("/proc/1/environ", buf))
			return *cid;
	} while (*++cid);

	if (!access("/proc/xen", F_OK)) {
		if (!file_regex("/proc/xen/capabilities", "control_d"))
			return SV_KEYWORD_XEN0;
		return SV_KEYWORD_XENU;
	}
	else if (!file_regex("/proc/cpuinfo", "UML"))
		return SV_KEYWORD_UML;
	else if (!file_regex("/proc/self/status", "(s_context|VxID):[[:space:]]*[1-9]"))
		return SV_KEYWORD_VSERVER;
	else if (!access("/proc/vz/veinfo", F_OK) && access("/proc/vz/version", F_OK))
		return SV_KEYWORD_OPENVZ;
	else if (!file_regex("/proc/self/status", "envID:[[:space:]]*[1-9]"))
		return SV_KEYWORD_OPENVZ;
#endif

	return 0;
}

static int svc_init_command(int level, int argc, const char *argv[])
{
	int i, retval;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%d, %d, %p)\n", __func__, level, argc, argv);
#endif

	if (!*sv_run_level[level])
		return -ENOENT;

	DEPTREE.list = sv_stringlist_new();
	for (i = 0; sv_run_level[level][i]; i++)
		sv_stringlist_add(DEPTREE.list, sv_run_level[level][i]);

	retval = svc_execl(DEPTREE.list, argc, argv);
	sv_stringlist_free(&DEPTREE.list);
	return retval;
}

__attribute__((__noreturn__)) static void sv_init_status(void)
{
	const char *argv[8];
	struct svcrun run = { .argc = 8, .argv = argv };
	SV_String_T *svc;
	SV_StringList_T *list, *l;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif

	if (sv_init < 0) {
		list = sv_svclist_load(SV_TMPDIR_STAR);
		l = sv_svclist_load(SV_TMPDIR_FAIL);
		sv_stringlist_cat(&list, &l);
	}
	else
		list = sv_svclist_load(NULL);
	list = sv_stringlist_sort(&list);
	argv[0] = strrchr(SV_RUN_SH, '/')+1U;
	argv[4] = sv_svc_cmd[SV_SVC_CMD_STATUS];
	argv[5] = (char *)0;

	TAILQ_FOREACH(svc, list, entries) {
		run.svc = svc;
		run.name = svc->str;
		svc_cmd(&run);
	}
	exit(EXIT_SUCCESS);
}

static void svc_init(const char *cmd)
{
	const char *command = cmd;
	const char *argv[8];
	char buf[128];
	char b[32];
	int p, r;
	int svc_start = 1;
	int level = 0;
	const char *runlevel;
	time_t t;
#ifdef SV_DEBUG
	if (sv_debug) DBG("%s(%s)\n", __func__, cmd);
#endif

	runlevel  = svc_run_level(NULL);
	switch(sv_level) {
	case SV_SHUTDOWN_LEVEL:
		if (!runlevel) {
			ERR("There's nothing to shut down!!!\n", NULL);
			exit(EXIT_FAILURE);
		}
	case SV_SYSINIT_LEVEL:
		if (sv_system) {
			ERR("Invalid usage -- `%s' runlevel with `%s' subsystem\n",
					sv_init_level[sv_level], sv_keywords[sv_system]);
			exit(EXIT_FAILURE);
		}
		break;
	}

	/* set a few sane environment variables */
	sv_svcdeps_load(NULL);
	svc_deps  = 0;
	svc_quiet = 0;
	sv_pid    = getpid();
	setenv("SV_INITLEVEL" , sv_init_level[sv_init] , 1);
	setenv("SV_RUNLEVEL", sv_init_level[sv_level], 1);
	snprintf(buf, sizeof(buf), "%d", sv_pid);
	setenv("SV_PID", buf, 1);
	if ((r = open(SV_PIDFILE, O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC, 0644)) > 0) {
		if (flock(r, LOCK_EX|LOCK_NB))
			ERROR("Failed to lock %s", SV_PIDFILE);
	}
	else
		ERROR("Failed to open %s", SV_PIDFILE);
	(void)err_write(r, (const char*)buf, SV_PIDFILE);

	if (sv_init == SV_SYSINIT_LEVEL || command == NULL) /* force service command */
		command = sv_svc_cmd[SV_SVC_CMD_START];
	if (strcmp(command, sv_svc_cmd[SV_SVC_CMD_STOP]) == 0)
		svc_start = 0;

	argv[0] = strrchr(SV_RUN_SH, '/')+1U;
	argv[5] = (char *)0;
	chdir("/");

	t = time(NULL);
	ctime_r(&t, b);
	svc_log("logging: %s command\n", command);
	svc_log("%s %s init level started at %s\n", progname, sv_init_level[sv_init], b);

	/* do this extra loop to be able to stop sysboot with sv_init=SV_SHUTDOWN_LEVEL;
	 * so that, {local,network}fs services etc. can be safely stopped
	 */
	for (;;) { /* SHUTDOWN_LOOP */
		if (sv_init == SV_SHUTDOWN_LEVEL && !level) {
			level = SV_REBOOT_LEVEL;
			/* load the started services instead of only SV_{SYSBOOT,DEFAULT}_LEVEL
			 * to be abe to shutdown everything with sv_init=SV_SHUTDOWN_LEVEL
			 */
			DEPTREE.list = sv_svclist_load(SV_TMPDIR_STAR);
			command = sv_svc_cmd[SV_SVC_CMD_STOP];
			svc_start = 0;
		}
		else if (level == SV_REBOOT_LEVEL) {
			level = 0;
			/* close the logfile because rootfs will be mounted read-only */
			debugfp = stderr; debugfd = STDERR_FILENO;
			if (!fclose(logfp))
				logfd = 0;
			if (!close(logfd))
				logfp = NULL;
			/* and finaly start shutdown runlevel */
			command = sv_svc_cmd[SV_SVC_CMD_START];
			svc_start = 1;
			snprintf(buf, sizeof(buf), "%s/%s", SV_TMPDIR_DEPS, sv_init_level[sv_init]);
			unlink(buf);
		}
		else if (sv_init == SV_DEFAULT_LEVEL && !level) {
			/* start sysboot runlevel only when service command is NULL */
			if (!cmd && (!runlevel ||
						(strcmp(runlevel, sv_init_level[SV_SYSBOOT_LEVEL]) &&
						 strcmp(runlevel, sv_init_level[SV_DEFAULT_LEVEL])))) {
			level = sv_level;
			sv_level = sv_init = SV_SYSBOOT_LEVEL;
			setenv("SV_RUNLEVEL", sv_init_level[sv_level], 1);
			setenv("SV_INITLEVEL" , sv_init_level[sv_init] , 1);
			}
		}
		else if (level == SV_DEFAULT_LEVEL) {
			sv_level = sv_init = level;
			level = 0;
			svc_run_level(sv_init_level[SV_SYSBOOT_LEVEL]);
			setenv("SV_RUNLEVEL", sv_init_level[sv_level], 1);
			setenv("SV_INITLEVEL" , sv_init_level[sv_init] , 1);
			svc_environ_update(ENVIRON_OFF);
		}
		else if (sv_init == SV_SINGLE_LEVEL) {
			/* stop default runlevel only when service command is NULL */
			if (!cmd && !strcmp(runlevel, sv_init_level[SV_DEFAULT_LEVEL])) {
			level = sv_init;
			sv_init = SV_DEFAULT_LEVEL;
			setenv("SV_INITLEVEL" , sv_init_level[sv_init] , 1);
			command = sv_svc_cmd[SV_SVC_CMD_STOP];
			svc_start = 0;
			}
		}
		else if (level == SV_SINGLE_LEVEL) {
			sv_init = level;
			level = 0;
			setenv("SV_INITLEVEL" , sv_init_level[sv_init] ,1);
			svc_environ_update(ENVIRON_OFF);
			command = sv_svc_cmd[SV_SVC_CMD_START];
			svc_start = 1;
		}
		else if (sv_init == SV_SYSBOOT_LEVEL) {
			svc_level(); /* make SystemV compatible runlevel */
		}
		argv[4] = command;

		t = time(NULL);
		ctime_r(&t, b);
		svc_log( "\n\t%s runlevel (%s command) at %s\n", sv_init_level[sv_level],
				command, b);
		printf("\n\t%s%s%s %s(ing) init %s%s%s runlevel at %s%s%s\n",
				print_color(COLOR_MAG, COLOR_FG), progname, print_color(COLOR_RST, COLOR_RST),
				command, print_color(COLOR_BLU, COLOR_FG), sv_init_level[sv_level],
				print_color(COLOR_RST, COLOR_RST), print_color(COLOR_YLW, COLOR_RST),
				b, print_color(COLOR_RST, COLOR_RST));

		sv_deptree_load(&DEPTREE);
		if (svc_start)
			p = DEPTREE.size-1;
		else
			p = 0;
		while (p >= 0 && p < DEPTREE.size) { /* PRIORITY_LEVEL_LOOP */
			if (!TAILQ_EMPTY(DEPTREE.tree[p])) {
				t = time(NULL);
				ctime_r(&t, b);
				svc_log("\n\tpriority-level-%d started at %s\n", p,	b);
				r = svc_execl(DEPTREE.tree[p], 8, argv);
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
		sv_deptree_free(&DEPTREE);
		sv_stringlist_free(&DEPTREE.list);

		if (svc_start && *sv_run_level[sv_init])
			svc_init_command(sv_init, 8, argv);
		/* break shutdown loop */
		if (!level)
			break;
	} /* SHUTDOWN_LOOP */

	svc_run_level(sv_init_level[sv_level]);
	t = time(NULL);
	ctime_r(&t, b);
	svc_log("\n%s %s init run level stopped at %s\n", progname, sv_init_level[sv_init], b);
	atexit(sv_cleanup);
}

int main(int argc, char *argv[])
{
	int opt;
	char *ptr;
	char on[] = "1", off[] = "0";

	debugfp = stderr;
	progname = strrchr(argv[0], '/');
	if (!progname)
		progname = argv[0];
	else
		progname++;
	applet = (const char*)progname;

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
			case 'd':
				setenv("SVC_DEBUG", on, 1);
				break;
			case 'x':
				setenv("SVC_TRACE", on, 1);
				break;
			case '4':
			case 'S':
				sv_level = SV_SYSINIT_LEVEL;
				sv_init  = SV_SYSINIT_LEVEL;
				break;
			case '2':
			case 'N':
				sv_level = SV_NOWNETWORK_LEVEL;
				sv_init  = SV_SYSBOOT_LEVEL;
			case '5':
			case 'b':
				sv_level = SV_SYSBOOT_LEVEL;
				sv_init  = SV_SYSBOOT_LEVEL;
				break;
			case '1':
			case 's':
				sv_level = SV_SINGLE_LEVEL;
				sv_init  = SV_SINGLE_LEVEL;
				break;
			case '3':
			case 'l':
				sv_level = SV_DEFAULT_LEVEL;
				sv_init  = SV_DEFAULT_LEVEL;
				break;
			case '6':
			case 'r':
				sv_level = SV_REBOOT_LEVEL;
				sv_init  = SV_SHUTDOWN_LEVEL;
				break;
			case '0':
			case 'p':
				sv_level = SV_SHUTDOWN_LEVEL;
				sv_init  = SV_SHUTDOWN_LEVEL;
				break;
			case 'q':
				svc_quiet = 0;
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

	/* set this to avoid double waiting for a lockfile for supervision */
	setenv("__SVC_WAIT__", off, 1);
	setenv("SVC_DEPS", off, 1);
	setenv("SV_SYSTEM", "", 1);
	setenv("SV_PREFIX", "", 1);
	if ((ptr = (char*)sv_getconf("SV_SYSTEM"))) {
		for (opt = SV_KEYWORD_SUPERVISION; sv_keywords[opt]; opt++)
			if (strcmp(ptr, sv_keywords[opt]) == 0) {
				setenv("SV_SYSTEM", ptr, 1);
				sv_system = opt;
				break;
			}
	}
	else if ((opt = sv_system_detect())) {
		sv_system = opt;
		setenv("SV_SYSTEM", sv_keywords[opt], 1);
	}
	if ((ptr = (char*)sv_getconf("SV_PREFIX")))
		setenv("SV_PREFIX", ptr, 1);
	setenv("SV_LIBDIR", SV_LIBDIR, 1);
	setenv("SV_RUNDIR", SV_RUNDIR, 1);
	setenv("SV_SVCDIR", SV_SVCDIR, 1);
	setenv("SV_VERSION", SV_VERSION, 1);
	setenv("SV_SYSBOOT_LEVEL" , sv_init_level[SV_SYSBOOT_LEVEL] , 1);
	setenv("SV_SHUTDOWN_LEVEL", sv_init_level[SV_SHUTDOWN_LEVEL], 1);
	sv_debug    = sv_conf_yesno("SV_DEBUG");
	sv_parallel = sv_conf_yesno("SV_PARALLEL");
	sv_sigsetup();

	if (!strcmp(progname, "sv-run")) {
		if (!argc) {
sv_run_help:
			fprintf(stderr, "Usage: %s [OPTIONS] SERVICE COMMAND [ARGUMENTS]\n", progname);
			exit(EXIT_FAILURE);
		}

		if (!strcmp(*argv, sv_svc_cmd[SV_SVC_CMD_STATUS]))
			sv_init_status();
		else if (!strcmp(*argv, "scan")) {
			argc--; argv++;
			svc_sigsetup();
			setenv("SVCDEPS_UPDATE", on, 1);
			execv(SV_DEPGEN, argv);
			ERROR("Failed to execv(%s, argv)", SV_DEPGEN);
		}
		else if (strcmp(*argv, "init") == 0) {
			/* compatibility with <v0.13.0 */
			argc--, argv++;
			if (argc) {
				if (!strcmp(*argv, sv_svc_cmd[SV_SVC_CMD_STATUS]))
					sv_init_status();
				if(!strcmp(*argv, sv_svc_cmd[SV_SVC_CMD_START]) ||
				   !strcmp(*argv, sv_svc_cmd[SV_SVC_CMD_STOP] ) )
					if (sv_init >= 0)
						goto sv_rc_init;
sv_run_init_help:
				fprintf(stderr, "Usage: %s [OPTIONS] init [start|stop|status]\n", progname);
				exit(EXIT_FAILURE);
			}
			else goto sv_run_init_help;
		}
		else {
sv_run:
			/* handle service command or
			 * support SystemV or BSD like system rc command
			 */
			if (argc < 2) goto sv_run_help;
			unsetenv("SV_INITLEVEL");
			unsetenv("SV_RUNLEVEL");
			return svc_exec(argc, argv);
		}
	}
	else if (!strcmp(progname, "service")) goto sv_run;
	else if (!strcmp(progname, "rc")) goto sv_rc;
	else if (!strcmp(progname, "sv-rc")) {
		if (sv_init >= 0) {
sv_rc_init:
			setenv("SVC_TRACE", off, 1);
			if (sv_init >= 0) {
				svc_init(*argv);
				exit(EXIT_SUCCESS);
			}
			else {
				fprintf(stderr, "Usage: %s -(0|1|2|3|4|5|6)"
						"(runlevel argument required)\n", progname);
				exit(EXIT_FAILURE);
			}
		}

sv_rc:
		if (argc != 1) {
sv_rc_help:
			fprintf(stderr, "Usage: %s (nonetwork|single|sysinit|sysboot|default|"
					"shutdown|reboot)\n", progname);
			exit(EXIT_FAILURE);
		}

		/* support SystemV and BSD like system rc runlevel */
		for (sv_level = 0; sv_init_level[sv_level]; sv_level++) {
			if (!strcmp(*argv, sv_init_level[sv_level])) {
				switch(sv_level) {
				case SV_REBOOT_LEVEL:
				case SV_SHUTDOWN_LEVEL:
					sv_init = SV_SHUTDOWN_LEVEL;
					break;
				case SV_NOWNETWORK_LEVEL:
				case SV_SYSBOOT_LEVEL:
					sv_init = SV_SYSBOOT_LEVEL;
					break;
				case SV_DEFAULT_LEVEL:
				case SV_SINGLE_LEVEL:
				case SV_SYSINIT_LEVEL:
					sv_init = sv_level;
					break;
				}
				argc--; argv++; *argv = NULL;
				goto sv_rc_init;
			}
		}

		ERR("invalid argument -- `%s'\n", *argv);
		goto sv_rc_help;
	}

	ERR("nothing to do -- invalid usage!!!\n", NULL);
	exit(EXIT_FAILURE);
}