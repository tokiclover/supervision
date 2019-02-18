/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)svp.c  0.15.0 2019/02/14
 */

#include <dirent.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <fcntl.h>
#include "error.h"
#include "helper.h"
#include "sv-copyright.h"

#define SV_VERSION "0.15.0"

#define EXECVP(argv) do {                            \
	a = *argv;                                       \
	if ((p = strrchr(*argv, '/'))) *argv = p;        \
	execvp(a, argv);                                 \
	ERROR("Failed to execute `%s'", a);              \
} while (0/*CONSTCOND*/)

const char *progname;
char fb[BUFSIZ];
static struct slimit {
	unsigned long int sl_resource, sl_value;
	const char *sl_name, *__sl_ptr;
} SLIMIT[8] = {
	{ .sl_resource = RLIMIT_CORE, .sl_name = "core" },
	{ .sl_resource = RLIMIT_DATA, .sl_name = "data" },
	{ .sl_resource = RLIMIT_AS  , .sl_name = "memory" },
	{ .sl_resource = RLIMIT_NOFILE, .sl_name = "open" },
	{ .sl_resource = RLIMIT_FSIZE, .sl_name = "file" },
#ifdef RLIMIT_NPROC
	{ .sl_resource = RLIMIT_NPROC, .sl_name = "nproc" },
#endif
};
static char *group, *user, *pwnam;
static char *a, *p;
static int fd, o, rv;
static int f_lock = LOCK_EX;
static uid_t uid;
static gid_t gid;
static int f_egid, f_euid, f_env, id;
static struct passwd *pwd;
static struct group *grp;
static int f_exec;

static const char *shortopts = "012hPsVv/:b:c:d:e:f:G:g:L:l:m:n:o:p:U:u:";
static const struct option longopts[] = {
	{ "chroot" , 0, NULL, '/' },
	{ "name"   , 0, NULL, 'b' },
	{ "core"   , 0, NULL, 'c' },
	{ "data"   , 0, NULL, 'd' },
	{ "envdir" , 0, NULL, 'e' },
	{ "output" , 0, NULL, 'f' },
	{ "egid"   , 0, NULL, 'G' },
	{ "euid"   , 0, NULL, 'U' },
	{ "Lock"   , 0, NULL, 'L' },
	{ "lock"   , 0, NULL, 'l' },
	{ "memory" , 0, NULL, 'm' },
	{ "nice"   , 0, NULL, 'n' },
	{ "open"   , 0, NULL, 'o' },
	{ "proc"   , 0, NULL, 'p' },
	{ "gid"    , 0, NULL, 'g' },
	{ "uid"    , 0, NULL, 'u' },
	{ "sid"    , 0, NULL, 's' },
	{ "pgrp"   , 0, NULL, 'P' },
	{ "stdin"  , 0, NULL, '0' },
	{ "stdout" , 0, NULL, '1' },
	{ "stderr" , 0, NULL, '2' },
	{ "verbose", 0, NULL, 'V' },
	{ "help"   , 0, NULL, 'h' },
	{ "version", 0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Set root directory",
	"Set first vector argument",
	"Set core size limit",
	"Set data size limit",
	"Use environment directory",
	"Set output size limit",
	"Set effective group identity",
	"Set effective user  identity",
	"Set exclusive lock file",
	"Set lock file",
	"Set memory limit",
	"Set process niceness",
	"Set open file limit",
	"Set process limit",
	"Set group identity",
	"Set user  identity",
	"Set session identity",
	"Set process group",
	"Close standard input",
	"Close standard output",
	"Close standard error",
	"Enable verbose output",
	"Print help message",
	"Print version string",
	NULL
};

static void envdir(int argc, char *argv[]);
static void setuidgid(int argc, char *argv[]);
static void setlock(int argc, char *argv[]);
static void softlimit(int argc, char *argv[]);
static void help_message(int retval);

__attribute__((__noreturn__)) static void help_message(int retval)
{
	printf("Usage: %s [OPTIONS] PROGRAM [ARGUMENTS]\n", progname);
	for ( o = 0; o < 16; o++)
		printf("    -%c, --%-6s  ARG    %s\n", longopts[o].val, longopts[o].name,
				longopts_help[o]);
	for ( ; longopts_help[o]; o++)
		printf("    -%c, --%-14s %s\n", longopts[o].val, longopts[o].name,
				longopts_help[o]);

	exit(retval);
}

static void envdir(int argc, char *argv[])
{
	DIR  *dp;
	int df, fd;
	struct dirent *de;

	if ((argc < 2)) exit(EXIT_FAILURE);
	if (!(dp = opendir(*argv)))
		ERROR("cannot open `%s' directory", *argv);
	df = dirfd(dp);

	while ((de = readdir(dp))) {
		if (*de->d_name == '.') continue;
		if ((fd = openat(df, de->d_name, O_RDONLY)) == -1) {
			ERR("cannot open `%s' file\n", de->d_name);
			continue;
		}
		
		rv = read(fd, fb, BUFSIZ);
		close(fd);
		if (rv == -1) continue;
		setenv(de->d_name, fb, 1);
	}

	argv++;
	if (!f_exec) return;
	EXECVP(argv);
}

static void setuidgid(int argc, char *argv[])
{
	if (f_exec) {
	static const char *shortopts = "G:g:U:u:";
	static const struct option longopts[] = {
		{ "egid"   , 0, NULL, 'G' },
		{ "euid"   , 0, NULL, 'U' },
		{ "gid"    , 0, NULL, 'g' },
		{ "uid"    , 0, NULL, 'u' },
		{ 0, 0, 0, 0 }
	};

	if (argc < 2) {
help_message:
		puts("Usage: %s [-U|-u USER:GROUP] [-G|-g GROUP] PROG [ARGS]");
		ERR("more arguments required!\n", NULL);
		exit(EXIT_FAILURE);
	}

	while ((o = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (o) {
		case 'G': f_egid++;
		case 'g': group = optarg; break;
		case 'U': f_euid++;
		case 'u': user = optarg; break;
		default: goto help_message;
		}
	}
	argc -= optind, argv += optind;
	}

	if (user) {
		pwnam = err_strdup(user);
		group = strchr(pwnam, ':');
		if (group)
			*group++ = '\0';
	}
	if (group)
		group = group;

	if (user) {
		if (sscanf(user, "%d", &id) == 1)
			pwd = getpwuid((uid_t) id);
		else
			pwd = getpwnam(user);
		if (pwd)
			uid = pwd->pw_uid;
		else {
			ERR("cannot get `%s' user identification\n", user);
			exit(EXIT_FAILURE);
		}
	}
	if (group) {
		if (sscanf(group, "%d", &id) == 1)
			grp = getgrgid((gid_t) id);
		else
			grp = getgrnam(group);
		if (grp)
			gid = grp->gr_gid;
		else {
			ERR("cannot get `%s' group identification\n", group);
			exit(EXIT_FAILURE);
		}
	}
	free(pwnam);

	if (uid) {
		if (setuid(uid)) ERROR("cannot set `%s' user  identification", user );
		if (f_env) {
			setenv("USER", user, 1);
			sprintf(fb, "%d", uid);
			setenv("UID", fb, 1);
		}
	}
	if (gid) {
		if (setgid(gid)) ERROR("cannot set `%s' group identification", group);
		if (f_env) {
			setenv("GROUP", group, 1);
			sprintf(fb, "%d", gid);
			setenv("GID", fb, 1);
		}
	}
	if (f_euid) {
		if (seteuid(uid))
			ERROR("cannot set `%s' effective user  identification", user );
	}
	if (f_egid) {
		if (setegid(gid))
			ERROR("cannot set `%s' effective group identification", group);
	}

	if (!f_exec) return;
	EXECVP(argv);
}

static void setlock(int argc, char *argv[])
{
	if (f_exec) {
	if (argc < 2) {
help_message:
		printf("Usage: %s [-n] FILE PROG [ARGS]\n", progname);
		exit(EXIT_FAILURE);
	}

	while ((o = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (o) {
		case 'n':
#ifdef HAVE_FLOCK
			f_lock |= LOCK_NB;
#else
			f_lock |= F_TLOCK;
#endif
			break;
		case 'N':
		case 'x':
		case 'X': break;
		default: goto help_message;
		}
	}
	argc -= optind, argv += optind;
	if (argc < 2) goto help_message;
	}
	
	if ((fd = open(*argv, O_CREAT | O_RDWR, 0644)) == -1)
		ERROR("cannot open `%s'", *argv);
	do {
#ifdef HAVE_FLOCK
		rv = flock(fd, f_lock);
#else
		rv = lockf(fd, f_lock, 0LU);
#endif
		if (rv == -1) {
			if (errno == EINTR) continue;
			ERROR("cannot lock `%s' file", *argv);
		}
	} while (rv);	
	argv++;

	if (!f_exec) return;
	EXECVP(argv);
}

static void softlimit(int argc, char *argv[])
{
	struct rlimit limit;

	if (f_exec) {
	if (argc < 2) {
help_message:
		printf("Usage: %s [-c ARG] [-d ARG] [-f ARG] [-m ARG] [-o ARG] [-p ARG] PROG [ARGS]\n", progname);
		exit(EXIT_FAILURE);
	}
	/* Parse options */
	while ((o = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		errno = 0;
		switch (o) {
			case 'c':
				SLIMIT[0].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				break;
			case 'd':
				errno = 0;
				SLIMIT[1].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				break;
			case 'f':
				SLIMIT[4].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				break;
			case 'm':
				SLIMIT[2].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				break;
			case 'o':
				SLIMIT[3].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				SLIMIT[3].sl_value = SLIMIT[3].sl_value < _POSIX_OPEN_MAX ? _POSIX_OPEN_MAX : SLIMIT[3].sl_value;
				break;
			case 'p': /* IGNORED */
#ifdef RLIMIT_NPROC
				SLIMIT[5].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
#endif
				break;
			default: goto help_message;
		}
	}
	argc -= optind, argv += optind;
	if (argc < 1) goto help_message;
	}

	for (o = 0; SLIMIT[o].sl_name; o++) {
		if (SLIMIT[o].sl_value) {
			if (getrlimit(SLIMIT[o].sl_resource, &limit))
				ERROR("cannot get `%s' limit", SLIMIT[o].sl_name);
			limit.rlim_cur = limit.rlim_max < SLIMIT[o].sl_value ? limit.rlim_max :
				SLIMIT[o].sl_value;
			if (setrlimit(SLIMIT[o].sl_resource, &limit))
				ERR("cannot set `%s' limit\n", SLIMIT[o].sl_name);
		}
	}

	if (!f_exec) return;
	EXECVP(argv);

range_error:
	ERROR("invalid argument -- `%s'", argv[optind]);

	exit(EXIT_FAILURE);
}

#define ATOI(p) ((p[0] - '0')*10 + (p[1] - '0'))

int main(int argc, char *argv[])
{
	static int f_in, f_out, f_err, f_grp, f_sid;
	static int f_limit, f_nice, f_verbose;
	static char *env_dir, *work_dir;
	static char *f_arg, *lock;

	progname = strrchr(argv[0], '/');
	if (!progname)
		progname = argv[0];
	else
		progname++;

	if (argc < 1)
		help_message(1);

	if (strcmp(progname, "svp")) {
		argc--; argv++;
		f_exec++;
	}
	if (!strcmp(progname, "envdir")) {
		envdir(argc, argv);
	}
	else if (!strcmp(progname, "pgrphack")) {
		if (setpgid(0, 0) < 0) ERROR("cannot create a new session", NULL);
		EXECVP(argv);
		ERROR("Failed to execute `%s'", *argv);
	}
	else if (!strcmp(progname, "setsid")) {
		if (setsid() < 0) ERROR("cannot create a new session", NULL);
		EXECVP(argv);
		ERROR("Failed to execute `%s'", *argv);
	}
	else if (!strcmp(progname, "envuidgid")) {
		f_env++;
		setuidgid(argc, argv);
	}
	else if (!strcmp(progname, "setuidgid")) {
		setuidgid(argc, argv);
	}
	else if (!strcmp(progname, "setlock")) {
		setlock(argc, argv);
	}
	else if (!strcmp(progname, "softlimit")) {
		setuidgid(argc, argv);
	}

	/* Parse options */
	while ((o = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		errno = 0;
		switch (o) {
			case '0': f_in++ ; break;
			case '1': f_out++; break;
			case '2': f_err++; break;
			case 'P': f_grp++; break;
			case 's': f_sid++; break;
			case 'V': f_verbose++; break;
			case '/':
				work_dir = optarg;
				break;
			case 'b':
				f_arg = optarg;
				break;
			case 'c':
				errno = 0;
				SLIMIT[0].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				f_limit++;
				break;
			case 'd':
				SLIMIT[1].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				f_limit++;
				break;
			case 'e':
				env_dir = optarg;
				break;
			case 'f':
				SLIMIT[4].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				break;
			case 'G': f_egid++;
			case 'g': group = optarg; break;
			case 'U': f_euid++;
			case 'u': user = optarg; break;
			case 'L':
#ifdef HAVE_FLOCK
				f_lock |= LOCK_NB;
#else
				f_lock |= F_TLOCK;
#endif
			case 'l':
				lock = optarg;
				break;
			case 'm':
				SLIMIT[2].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				f_limit++;
				break;
			case 'n':
				f_nice = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				break;
			case 'o':
				SLIMIT[3].sl_value = strtol(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
				f_limit++;
				SLIMIT[3].sl_value = SLIMIT[3].sl_value < _POSIX_OPEN_MAX ? _POSIX_OPEN_MAX : SLIMIT[3].sl_value;
				break;
			case 'p': /* IGNORED */
#ifdef RLIMIT_NPROC
				SLIMIT[5].sl_value = strtoul(optarg, (char**restrict)0, 10);
				if (errno == ERANGE) goto range_error;
#endif
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
	if (argc < 1) {
		ERR("no enough arguments\n", NULL);
		exit(EXIT_FAILURE);
	}

	if (env_dir) {
		argc++; argv--;
		*argv = env_dir;
		envdir(argc, argv);
		argv++;
	}
	if (user || group) setuidgid(argc, argv); 
	if (lock) {
		argc++, argv--;
		*argv = lock;
		setlock(argc, argv);
		argv++;
	}
	if (f_limit) softlimit(argc, argv);
	if (f_nice) {
		errno = 0;
		if ((nice(f_nice) == -1) && errno) ERROR("cannot set `%d' niceness to process\n", f_nice);
	}
	if (work_dir) {
		if (chdir(work_dir))
			ERROR("cannot change current directory to `%s'", work_dir);
	}
	if (f_grp && (getpid() != getpgrp())) {
		if (setpgid(0, 0) < 0) ERROR("cannot create a new group", NULL);
	}
	if (f_sid) {
		if (setsid() < 0) ERROR("cannot create a new session", NULL);
	}
	if (f_in)  close(STDIN_FILENO);
	if (f_out) close(STDOUT_FILENO);
	if (f_err) close(STDERR_FILENO);

	EXECVP(argv);

range_error:
	ERROR("invalid argument -- `%s'", argv[optind]);

	return EXIT_FAILURE;
}
