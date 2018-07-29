/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-config.c  0.14.0 2018/07/18
 */

#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <paths.h>
#include "config.h"
#include "error.h"

#define VERSION "0.14.0"
#define SV_SVCDIR SYSCONFDIR "/sv"
#define SV_LIBDIR LIBDIR "/sv"
#define SV_RUNDIR RUNDIR "/sv"
#define SV_CONFIG SV_LIBDIR "/sh/SV-CONFIG"

__attribute__((__unused__)) int file_test(const char *pathname, int mode);

const char *progname;

#define SV_SYSINIT_LEVEL 2
#define SV_SYSBOOT_LEVEL 3
#define SV_DEFAULT_LEVEL 1
#define SV_SHUTDOWN_LEVEL 0
static const char *const sv_init_level[] = {
	"sysinit", "sysboot", "default", "shutdown", "single", NULL
};

static const char *shortopts = "Dc:dhluvx";
static const struct option longopts[] = {
	{ "nodeps",   0, NULL, 'D' },
	{ "log",      0, NULL, 'l' },
	{ "debug",    0, NULL, 'd' },
	{ "trace",    0, NULL, 'x' },
	{ "config",   0, NULL, 'c' },
	{ "update",   0, NULL, 'u' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Disable service dependency",
	"Add supervision log directory",
	"Enable debug mode",
	"Enable shell trace",
	"Configure supervision",
	"Update supervision",
	"Print help message",
	"Print version string",
	NULL
};

static char SVENT[64] = "runit|s6|daemontools|daemontools-encore";
static char *svent;
static int log;
static size_t len;
static char const*const cmd[2] = { "run", "finish" };

struct envent {
	char *var;
	char *val;
	char *arg;
	void *env;
};
static struct envent ENVENT[32] = {
	{ "__SIGSTRT__" , "-u" },
	{ "__SIGSTOP__" , "-d" },
	{ "__SIGONCE__" , "-o" },
	{ "__SIGPAUS__" , "-p" },
	{ "__SIGCONT__" , "-c" },
	{ "__SIGHUP__"  , "-h" },
	{ "__SIGALRM__" , "-a" },
	{ "__SIGINT__"  , "-i" },
	{ "__SIGTERM__" , "-t" },
	{ "__SIGKILL__" , "-k" },
	{ "__SIGEXIT__" , "-x" },
	{ NULL, NULL }
};
static struct envent CMDENT[8] = {
	{ "__SV_CMD__", "svscan", NULL },
	{ "__SVCCMD__", "svc"   , NULL },
	{ "__CHKCMD__", "svok"  , NULL },
	{ "__STACMD__", "svstat", NULL },
	{ "__LOGCMD__", "multilog", NULL },
	{ NULL, NULL, NULL }
};

static char const*const CMDSYM[8] = {
	"envdir", "envuidgid", "fghack", "pgrphack", "setlock", "setuidgid",
	"softlimit", NULL
};

static int svconfig(void)
{
	const char *bin, *prefix;
	char buf[256], *path[16], *ptr;
	int i = 0, j;
	FILE *fp;
	bin = ptr = _PATH_STDPATH;
#ifdef SV_DEBUG
	DBG("%s(void)\n", __func__);
#endif

	while ((ptr = strchr(ptr, ':'))) {
		len = ptr - bin;
		path[i] = err_malloc(len);
		memcpy(path[i], bin, len);
		i++;
		ptr++;
		bin = ptr;
	}
	path[i++] = (char*)0;
	bin = prefix = "";

	if (!strcmp(svent, "daemontools-encore")) {
		ENVENT[11].var = "__SIGQUIT__"; ENVENT[11].val = "-q";
		ENVENT[12].var = "__SIGUSR1__"; ENVENT[12].val = "-1";
		ENVENT[13].var = "__SIGUSR2__"; ENVENT[13].val = "-2";
		ENVENT[14].var = ENVENT[14].val = NULL;
	}
	else if (!strcmp(svent, "runit")) {
		bin = "chpst";
		ENVENT[0].val = "up";
		ENVENT[1].val = "down";
		ENVENT[2].val = "once";
		ENVENT[3].val = "pause";
		ENVENT[4].val = "cont";
		ENVENT[5].val = "hup";
		ENVENT[6].val = "alarm";
		ENVENT[7].val = "interrupt";
		ENVENT[8].val = "term";
		ENVENT[9].val = "kill";
		ENVENT[10].val = "exit";
		ENVENT[11].val = "quit";
		ENVENT[12].var = "__SIGUSR1__"; ENVENT[12].val = "1";
		ENVENT[13].var = "__SIGUSR2__"; ENVENT[13].val = "2";
		ENVENT[14].var = "__SIGWTUP__";
		ENVENT[14].val = "-w -T${SVC_TIMEOUT_UP:-${SV_TIMEOUT_UP:-10}} up";
		ENVENT[15].var = "__SIGWDWN__";
		ENVENT[15].val = "-w -T${SVC_TIMEOUT_DOWN:-${SV_TIMEOUT_DOWN:-30}} down";
		ENVENT[16].var = "__SIGRELD__"; ENVENT[16].val = "reload";
		ENVENT[17].var = ENVENT[17].val = NULL;
		CMDENT[0].val = "runsvdir";
		CMDENT[1].val = "sv";
		CMDENT[2].val = "sv"; CMDENT[2].arg = "check";
		CMDENT[3].val = "sv";
		CMDENT[4].val = "svlogd";
		CMDENT[5].var = "__SCANCMD__"; CMDENT[5].val = "pkill";
		CMDENT[5].arg = "-CONT -F ${SV_TMPDIR}/runsvdir.pid";
		CMDENT[6].var = "__PRECMD__"; CMDENT[6].val = "chpst";
		CMDENT[7].var = CMDENT[7].val = CMDENT[7].arg = NULL;
	}
	else if (!strcmp(svent, "s6")) {
		prefix = "s6-";
		ENVENT[11].var = "__SIGQUIT__"; ENVENT[11].val = "-q";
		ENVENT[12].var = "__SIGUSR1__"; ENVENT[12].val = "-1";
		ENVENT[13].var = "__SIGUSR2__"; ENVENT[13].val = "-2";
		ENVENT[14].var = "__SIGWTUP__";
		ENVENT[14].val = "-wU -T${SVC_TIMEOUT_UP:-${SV_TIMEOUT_UP:-10000}}";
		ENVENT[15].var = "__SIGWDWN__";
		ENVENT[15].val = "-wD -T${SVC_TIMEOUT_DOWN:-${SV_TIMEOUT_DOWN:-30000}}";
		ENVENT[14].var = ENVENT[14].val = NULL;
        CMDENT[4].val = "log";
        CMDENT[5].var = "__SCANCMD__"; CMDENT[5].val = "svscanctl";
		CMDENT[5].arg = "-an ${SV_RUNDIR}";
		CMDENT[6] .var= CMDENT[6].val = NULL;
	}

	if (!(fp = fopen(SV_CONFIG, "w")))
		ERROR("Failed to open `%s'", SV_CONFIG);
	fprintf(fp, "__SV_NAM__='%s'\n", svent);
	/* define the command macros */
	for (i = 0; CMDENT[i].var; i++) {
		for (j = 0; path[j]; j++) {
			snprintf(buf, sizeof(buf), "%s/%s%s", path[j], prefix, CMDENT[i].val);
			if (!access(buf, R_OK|X_OK)) break;
		}
		if (!path[j]) {
			errno = ENOENT;
			ERROR("Failed to find `%s%s' binary", prefix, CMDENT[i].val);
		}
		if (CMDENT[i].arg) {
			len = strlen(buf);
			snprintf(buf+len, sizeof(buf)-len, " %s", CMDENT[i].arg);
		}
		fprintf(fp, "%s='%s'\n", CMDENT[i].var, buf);
	}

	/* define the environment variables */
	for (i = 0; ENVENT[i].var; i++)
		fprintf(fp, "%s='%s'\n", ENVENT[i].var, ENVENT[i].val);
	fflush(fp);
	fclose(fp);

	/* make symlinks */
	if (strlen(bin)) {
		for (j = 0; path[j]; j++) {
			snprintf(buf, sizeof(buf), "%s/%s", path[j], bin);
			if (!access(buf, R_OK|X_OK)) break;
		}
		if (!path[j]) {
			errno = ENOENT;
			ERROR("Failed to find `%s' binary", bin);
		}
		len = strlen(buf)+1;
		ptr = buf+len;
		len = sizeof(buf)-len;
		for (i = 0; CMDSYM[i]; i++) {
			snprintf(ptr, len, "%s/bin/%s", SV_LIBDIR, CMDSYM[i]);
			(void)unlink(ptr);
			if (symlink(buf, ptr))
				ERROR("Failed to make `%s' symlink", ptr);
		}
		return EXIT_SUCCESS;
	}
	for (i = 0; CMDSYM[i]; i++) {
		for (j = 0; path[j]; j++) {
			snprintf(buf, sizeof(buf), "%s/%s%s", path[j], prefix, CMDSYM[i]);
			if (!access(buf, R_OK|X_OK)) break;
		}
		if (!path[j]) {
			errno = ENOENT;
			ERROR("Failed to find `%s%s' binary", prefix, CMDENT[i].val);
		}
		len = strlen(buf)+1;
		ptr = buf+len;
		snprintf(ptr, sizeof(buf)-len, "%s/bin/%s", SV_LIBDIR, CMDSYM[i]);
		(void)unlink(ptr);
		if (symlink(buf, ptr))
			ERROR("Failed to make `%s' symlink", ptr);
	}

	return EXIT_SUCCESS;
}

static int svupdate(void)
{
	char op[512], np[512];
	char const*const dir[2] = { SV_SVCDIR, SV_RUNDIR };
	DIR *nd, *od;
	int i, j, k, ofd, nfd;
	size_t sz;
	struct dirent *ent;
	struct utsname un;
#ifdef SV_DEBUG
	DBG("%s(void)\n", __func__);
#endif

	/* move v0.1[23].0 run level dirs to new v0.14.0 location */
	for (j = 0; sv_init_level[j]; j++) {
		snprintf(op, sizeof(op), "%s/.stage-%d", SV_SVCDIR, j);
		snprintf(np, sizeof(np), "%s/.%s"      , SV_SVCDIR, sv_init_level[j]);
		if (file_test(np, 'd'))
			memmove(op, np, sizeof(np));
		else if (!file_test(op, 'd'))
			break;
		if (!(od = opendir(op)))
			ERROR("Failed to opendir `%s'", op);
		snprintf(np, sizeof(np), "%s.init.d/%s", SV_SVCDIR, sv_init_level[j]);
		if (!(nd = opendir(np)))
			ERROR("Failed to opendir `%s'", np);
		if ((ofd = dirfd(od)) < 0)
			ERROR("failed to `dirfd(%s)'", op);
		if ((nfd = dirfd(nd)) < 0)
			ERROR("Failed to `dirfd(%s)'", np);

		while ((ent = readdir(od))) {
			if (*ent->d_name == '.') continue;
			renameat(ofd, ent->d_name, nfd, ent->d_name);
		}
		(void)closedir(od);
		(void)closedir(nd);
		(void)rmdir(op);
	}
	/* update ./{run,finish} symlinks to v0.14.0 */
	snprintf(np, sizeof(np), "%s/sh/cmd", SV_LIBDIR);
	for (j = 0; j < 2; j++) {
		if (!(od = opendir(dir[j])))
			ERROR("Failed to `opendir(%s)'", dir[j]);
		if ((ofd = dirfd(od)) < 0)
			ERROR("failed to `dirfd(%s)'", dir[j]);

		while ((ent = readdir(od))) {
			if (*ent->d_name == '.') continue;
			/* handle only supervision service */
#ifdef _DIRENT_HAVE_D_TYPE
			if (ent->d_type != DT_DIR) continue;
#else
			snprintf(op, sizeof(op), "%s/%s", dir[j], ent->d_name);
			if (!file_test(np, 'd')) continue;
#endif
			snprintf(op, sizeof(op), "%s", ent->d_name);
			len = strlen(op);

			/* handle user services */
			if (!j) {
				snprintf(op+len, sizeof(op)-len, "/%s", *cmd);
				sz = len+5U;
				if ((readlinkat(ofd, op, op+sz, sizeof(op)-sz) > 0) &&
					!(strstr(op+sz, "/.opt/")))
					continue;
				*(op+len) = '\0';
			}

			for (k = 0; k < 2; k++) {
				for (i = 0; i < 2; i++) {
					snprintf(op+len, sizeof(op)-len, "/%s", cmd[i]);
					(void)unlinkat(ofd, op, 0);
					if (symlinkat(np, ofd, op))
						ERR("Failed to make `%s/%s' symlink", dir[j], op);
				}
				/* handle log dir */
				if (k) break;
				snprintf(op, sizeof(op), "%s/log", ent->d_name);
				if (faccessat(ofd, op, F_OK, 0)) break;
				len = strlen(op);
			}
		}
		(void)closedir(od);
	}

	/* update SERVICE_{ENV,OPTIONS} to v0.13.0 format */
	snprintf(np, sizeof(np), "%s/.tmp", SV_RUNDIR);
	if (access(np, F_OK))
		return EXIT_SUCCESS;
	if (!(nd = opendir(np)))
		ERROR("Failed to `opendir(%s)'", np);
	if ((nfd = dirfd(nd)) < 0)
		ERROR("Failed to `dirfd(%s)'", np);
	if (!faccessat(nfd, "SV_OPTIONS", F_OK, 0))
		if (renameat(nfd, "SV_OPTIONS", nfd, "env"))
			ERR("Failed to `rename(.., %s,.., %s)'", "SV_OPTIONS", "env");
	if (!mkdirat(nfd, "envs", 0755) && errno != EEXIST)
		ERROR("Failed to `mkdirat(.., %s,..)'", "envs");
	if (!mkdirat(nfd, "opts", 0755) && errno != EEXIST)
		ERROR("Failed to `mkdirat(.., %s,..)'", "opts");

	while ((ent = readdir(nd))) {
		if (*ent->d_name == '.') continue;
#ifdef _DIRENT_HAVE_D_TYPE
		if (ent->d_type != DT_REG) continue;
#else
		snprintf(op, sizeof(op), "%s/.tmp/%s", SV_RUNDIR, ent->d_name);
		if (!file_test(op, 'f')) continue;
#endif
		len = strlen(ent->d_name);

		if ((*(ent->d_name+len-4U) == '_') && !strcmp(ent->d_name+len-4u, "_ENV")) {
			snprintf(op, sizeof(op), "%s/%s", "envs", ent->d_name);
			*(op+5U+len-4U) = '\0';
			if (renameat(nfd, ent->d_name, nfd, op))
				ERR("Failed to `renameat(.., %s,.., %s)'", ent->d_name, op);
			continue;
		}

		if ((*(ent->d_name+len-8U) == '_') && !strcmp(ent->d_name+len-8u, "_OPTIONS")) {
			snprintf(op, sizeof(op), "%s/%s", "opts", ent->d_name);
			*(op+5U+len-8U) = '\0';
			if (renameat(nfd, ent->d_name, nfd, op))
				ERR("Failed to `renameat(.., %s,.., %s)'", ent->d_name, op);
		}
	}

	/* update SV_TMPDIR/env environment file */
	if ((ofd = openat(nfd, "env", O_CREAT|O_TRUNC|O_RDWR, 0644)) > 0) {
		if (uname(&un) == -1)
			ERROR("Failed to get the name of the system: %s\n", strerror(errno));
		snprintf(op, sizeof(op), "SV_UNAME='%s'\nSV_UNAME_RELEASE='%s'\n",
				un.sysname, un.release);
		err_write(ofd, op, np);
	}
	else ERROR("Failed to update `%s/.tmp/env': %s\n", SV_RUNDIR, strerror(errno));
	(void)closedir(nd);

	return EXIT_SUCCESS;
}


static void help_message(int status)
{
	int i = 0;

	printf("Usage: %s ([OPTIONS] SERVICE COMMAND) | (--config ARG)\n", progname);
	printf("   %s [-l|--log] SERVICE new\n", progname);
	for ( ; i < 4; i++)
		printf("     -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);
	printf("   %s -c|--config %s\n", progname, SVENT);
	for ( ; longopts_help[i]; i++)
		printf("     -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);
	exit(status);
}

static int newsvc(const char *svc)
{
	int i, j;
	int nf, of;
	size_t nv, ov, sz;
	struct stat st;
	char *pt;
	char buf[BUFSIZ];
#ifdef SV_DEBUG
	DBG("%s(%s)\n", __func__, svc);
#endif

	if (!svc) {
		errno = ENOENT;
		ERROR("service name is required\n", NULL);
	}

	/* make ./{run,finish} along with the service */
	snprintf(buf, sizeof(buf),"%s/%s", SV_SVCDIR, svc);
	len = strlen(buf);
	for (i = 0; i < 2; i++) {
		if (access(buf, F_OK))
			if (mkdir(buf, 0755))
				ERROR("Failed to create `%s' directoryn", buf);
		for (j = 0; j < 2; j++) {
			snprintf(buf+len, sizeof(buf)-len, "/%s", cmd[j]);
			if (access(buf, F_OK))
				if (symlink(SV_LIBDIR "/sh/cmd", buf))
					ERROR("Failed to create `%s'", buf);
		}
		if (log > 0) {
			snprintf(buf+len, sizeof(buf)-len, "/log");
			len += 4U;
			log = -1;
		}
		else break;
	}

	if (log) {
		len -= 4U;
		*(buf+len) = '\0';
	}
	/* make a new OPTIONS file if necessary */
	snprintf(buf+len, sizeof(buf)-len, "/OPTIONS");
	if (!access(buf, F_OK)) {
		len += 8U;
		snprintf(buf+len, sizeof(buf)-len, ".%s", svc);
		if (!access(buf, F_OK)) {
			*(buf+len) = '\0';
			ERR("Failed to make `%s{,.%s}': files exist already\n", buf, svc);
			return EXIT_SUCCESS;
		}
	}

	if ((nf = open(buf, O_CREAT|O_RDWR|O_TRUNC, 0644)) < 0)
		ERROR("Failed to open `%s'", buf);
	snprintf(buf, sizeof(buf), "%s/sh/SV-OPTIONS.in", SV_LIBDIR);
	if ((of = open(buf, O_RDONLY, 0644)) < 0)
		ERROR("Failed to open `%s'", buf);

	memset(&st, 0, sizeof(st));
	if (stat(buf, &st))
		ERROR("Failed to stat(`%s',...)", buf);
	sz = st.st_size;
	
	do {
		ov = 0;
		i = read(of, buf, sizeof(buf));
		if (i < 0) {
			if (errno == EINTR) continue;
			ERROR("Failed to read `%s'", SV_LIBDIR "/sh/SV-OPTIONS.in");
		}
		ov += (size_t)i;

		/* replace @svc@ with the real service name */
		if ((sz == st.st_size) && (strlen(buf) > 77U) && (pt = strstr(buf, "@svc@"))) {
			len = strlen(svc);
			len = len > 38U ? 38U : len;
			memcpy(pt, svc, len);
			memcpy(pt+len, "/OPTIONS    ", 12U);
		}

		sz -= ov;
		nv = 0;
		do {
			j = write(nf, buf+nv, ov);
			if (j < 0) {
				if (errno == EINTR) continue;
				ERROR("Failed to write to `%s/%s/OPTIONS'", SV_SVCDIR, svc);
			}
			nv += (size_t)j;
			ov -= nv;
		} while (ov);
	} while (sz);

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	int opt, i = 1;
	char *args[8] = { "sv-run" };

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = *argv;
	else
		progname++;

	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
	{
		switch(opt) {
		case 'D':
			args[i++] = argv[optind];
			break;
		case 'l':
			log++;
			break;
		case 'd':
			args[i++] = argv[optind];
			break;
		case 'x':
			args[i++] = argv[optind];
			break;
		case 'c':
			len = strlen(optarg);
			if ((svent = strstr(SVENT, optarg)) && *(svent+len) == '|')
				*(svent+len) = '\0';
			else {
				ERR("invalid/unsuported `%s' supervisor\n", optarg);
				help_message(EXIT_FAILURE);
			}
			break;
		case 'u':
			return svupdate();
		case 'h':
			help_message(EXIT_SUCCESS);
		case 'v':
			printf("%s version %s\n", progname, VERSION);
			exit(EXIT_SUCCESS);
		case '?':
			exit(EXIT_FAILURE);
		default:
			ERR("unkown option -- `%s'\n", argv[optind]);
			help_message(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;
	if (svent) return svconfig();
	else if (argc >= 2) {
		if (!strcmp(argv[1], "new"))
			return newsvc(*argv);
		else {
			while (*argv && (i < 7)) args[i++] = *argv++;
			args[i++] = (char*)0;
			return execvp(EXEC_PREFIX "/sbin/sv-run", (char*const*)args);
		}
	}
	else {
		ERR("insufficient number of arguments `%d' -- two required\n", argc);
		help_message(EXIT_FAILURE);
	}

	exit(EXIT_FAILURE);
}
