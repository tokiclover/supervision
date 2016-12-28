/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-stage.c  0.13.0 2016/12/26
 */

#include "sv.h"
#include "rs-deps.h"
#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern int svc_exec (int argc, const char *argv[]);
extern int svc_execl(RS_StringList_T *list, int argc, const char *argv[]);

/* signal handleer/setup */
sigset_t ss_child, ss_full, ss_old;
static void sv_sighandler(int sig);
static void sv_sigsetup(void);

int sv_nohang   =  0;
int sv_parallel =  0;
int sv_runlevel = -1;
int sv_stage    = -1;
pid_t sv_pid;
int svc_deps  = 1;
int svc_quiet = 1;
static RS_DepTree_T DEPTREE = { NULL, NULL, 0, 0 };

static FILE *logfp;
static int logfd, rs_debug;

/* list of service to start/stop before|after a stage */
static const char *const sv_init_stage[][4] = {
	{ "clock", "hostname", NULL },
};

/* !!! order matter (defined constant/enumeration) !!! */
const char *const sv_runlevel_name[] = { "shutdown", "single", "nonetwork",
	"default", "sysinit", "boot", "reboot", NULL
};

const char *progname;
static const char *applet;

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

__NORETURN__ static void help_message(int retval);

/*
 * bring system to a named level or stage
 * @cmd (start|stop|NULL); NULL is the default command
 */
static void svc_stage(const char *cmd);

/*
 * execute a service list (called from svc_stage())
 * to finish/start particular stages
 * @return 0 on success or number of failed services
 */
static int svc_stage_command(int stage, int argc, const char *argv[]);

/* simple rc compatible runlevel handler*/
static void svc_level(void);
static char *get_cmdline_entry(const char *entry);

/* simple helper to set/get runlevel
 * @level: runlevel to set, or NULL to get the current runlevel;
 * @return: current runlevel;
 */
static const char *svc_runlevel(const char *level);

__NORETURN__ static void help_message(int retval)
{
	int i;

	printf("Usage: %s [OPTIONS] (ARGUMENTS)\n", progname);
	for ( i = 0; i < 2; i++)
		printf("    -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);
	for ( i = 2; longopts_help[i]; i++)
		printf("    -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);

	exit(retval);
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
			svc_mark(ptr, SV_SVC_STAT_STAR, NULL);
		free(entry);
	}
	entry = old;
nonetwork:
	/* mark network services as started, so nothing will be started */
	if ((entry && strcmp(entry, sv_runlevel_name[SV_RUNLEVEL_NONETWORK]) == 0) ||
		(sv_runlevel == SV_RUNLEVEL_NONETWORK)) {
		for (i = 0; i < SERVICES.virt_count; i++)
			if (strcmp(SERVICES.virt_svcdeps[i]->virt, "net") == 0)
				svc_mark(SERVICES.virt_svcdeps[i]->svc, SV_SVC_STAT_STAR, NULL);
		svc_mark("net", SV_SVC_STAT_STAR, NULL);
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

int svc_end(const char *svc, int status)
{
	static char ok[] = "ok", no[] = "no", *m;
	if (status)
		m = no;
	else
		m = ok;
	return svc_log("(%s) [%s]\n", svc, m);
}

int svc_log(const char *fmt, ...)
{
	static char logfile[] = "/var/log/rs.log", *logpath;
	int retval = 0;
	va_list ap;


	/* save logfile if necessary */
	if (rs_conf_yesno("SV_DEBUG"))
		logpath = logfile;
	else
		logpath = SV_LOGFILE;
	if (!logfd && rs_debug) {
		logfd = open(logpath, O_NONBLOCK|O_CREAT|O_RDWR|O_CLOEXEC, 0644);
		if (logfd < 0)
			logfd = open(SV_LOGFILE, O_NONBLOCK|O_CREAT|O_RDWR|O_CLOEXEC, 0644);
		if (logfd > 0) {
			rs_debug = 1;
			logfp = fdopen(logfd, "a+");
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

static int svc_stage_command(int stage, int argc, const char *argv[])
{
	int i, retval;
	RS_StringList_T *list = rs_stringlist_new();

	for (i = 0; sv_init_stage[stage][i]; i++)
		rs_stringlist_add(list, sv_init_stage[stage][i]);

	retval = svc_execl(list, argc, argv);
	rs_stringlist_free(&list);

	return retval;
}

static void svc_stage(const char *cmd)
{
	const char *command = cmd;
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
	if ((r = open(SV_PIDFILE, O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC, 0644)) > 0) {
		if (flock(r, LOCK_EX|LOCK_NB))
			ERROR("Failed to lock %s", SV_PIDFILE);
	}
	else
		ERROR("Failed to open %s\n", SV_PIDFILE);
	write(r, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "%s/%d_deptree", SV_TMPDIR_DEPS, sv_stage);

	if (sv_stage == 0 || command == NULL) /* force service command */
		command = sv_svc_cmd[SV_SVC_CMD_START];
	if (strcmp(command, sv_svc_cmd[SV_SVC_CMD_STOP]) == 0)
		svc_start = 0;

	argv[5] = (char *)0;
	chdir("/");

	t = time(NULL);
	rs_debug = 1;
	svc_log("logging: %s command\n", command);
	svc_log("%s init stage-%d started at %s\n", progname, sv_stage, ctime(&t));

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
			command = sv_svc_cmd[SV_SVC_CMD_STOP];
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
			command = sv_svc_cmd[SV_SVC_CMD_START];
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
				r = svc_execl(DEPTREE.tree[p], argc, argv);
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
		svc_stage_command(0, argc, argv);

	svc_runlevel(sv_runlevel_name[sv_runlevel]);
	t = time(NULL);
	svc_log("\n%s init stage-%d stopped at %s\n", progname, sv_stage, ctime(&t));
}

int main(int argc, char *argv[])
{
	int opt;
	char *ptr;
	char on[] = "1", off[] = "0";

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
	setenv("SVC_WAIT", off, 1);
	setenv("SVC_DEPS", off, 1);
	setenv("SV_VERSION", SV_VERSION, 1);
	sv_nohang = rs_conf_yesno("SV_NOHANG");
	sv_parallel = rs_conf_yesno("SV_PARALLEL");
	sv_sigsetup();

	if (strcmp(progname, "rs") == 0 || strcmp(progname, "service") == 0)
		goto rs;
	else if (strcmp(progname, "rc") == 0) {
		setenv("SVC_DEBUG", off, 1);
		goto rc;
	}
	else if (strcmp(progname, "sv-stage") == 0) {
init_stage:
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
					svc_stage(sv_svc_cmd[SV_SVC_CMD_STOP]);
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

	/* compatibility with <v0.13.0 */
	if (argc && strcmp(*argv, "stage") == 0) {
		argc--, argv++;
		goto init_stage;
	}

	/* handle service command or
	 * support systemV compatiblity rc command
	 */
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [OPTIONS] SERVICE COMMAND [ARGUMENTS]\n",
				progname);
		exit(EXIT_FAILURE);
	}
	return svc_exec(argc, argv);
}
