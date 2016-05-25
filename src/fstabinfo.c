/*
 * fstab utility to querry re/mount entries
 *
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision (Scripts Framework).
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 */

#include "error.h"
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>

#if defined(__linux__)
# define HAVE_GETMNTENT
# include <mntent.h>
# define MNTENT struct mntent
# define SETMNTENT ptrtab = setmntent("/etc/fstab", "r")
# define GETMNTENT getmntent(ptrtab)
# define ENDMNTENT endmntent(ptrtab)
# define MNT_NODE(ent) ent->mnt_fsname
# define MNT_FILE(ent) ent->mnt_dir
# define MNT_TYPE(ent) ent->mnt_type
# define MNT_OPTS(ent) ent->mnt_opts
# define MNT_PASS(ent) ent->mnt_passno
#else
# define HAVE_GETFSENT
# include <fstab.h>
# define MNTENT struct fstab
# define SETMNTENT
# define GETMNTENT getfsent()
# define ENDMNTENT endfsent()
# define MNT_NODE(ent) ent->fs_spec
# define MNT_FILE(ent) ent->fs_file
# define MNT_TYPE(ent) ent->fs_vfstype
# define MNT_OPTS(ent) ent->fs_mntops
# define MNT_PASS(ent) ent->fs_passno
#endif

#define VERSION "0.10.0"

const char *prgname;

#if defined(HAVE_GETMNTENT)
static FILE *ptrtab;
#endif
static MNTENT **mnttab;
static size_t mntcnt = 0, mntnum = 32;

enum {
	FSTAB_NODE  = 0x01,
#define FSTAB_NODE FSTAB_NODE
	FSTAB_OPTS  = 0x02,
#define FSTAB_OPTS FSTAB_OPTS
	FSTAB_FSYS = 0x04,
#define FSTAB_FSYS FSTAB_FSYS
	FSTAB_ARGS  = 0x08,
#define FSTAB_ARGS FSTAB_ARGS
	FSTAB_MOUNT = 0x10,
#define FSTAB_MOUNT FSTAB_MOUNT
	FSTAB_REMOUNT  = 0x20
#define FSTAB_REMOUNT FSTAB_REMOUNT
};

static const char *shortopts = "adthmorv";
static const struct option longopts[] = {
	{ "mount",    0, NULL, 'm' },
	{ "remount",  0, NULL, 'r' },
	{ "device",   0, NULL, 'd' },
	{ "options",  0, NULL, 'o' },
	{ "fstype",   0, NULL, 't' },
	{ "mntargs",  0, NULL, 'a' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Mount entry",
	"Remount entry",
	"Print device node",
	"Print mount options",
	"Print filesystem",
	"Print mount arguments",
	"Print help massage",
	"Print version string",
	NULL
};

__NORETURN__ static void help_message(int status)
{
	int i;

	printf("Usage: %s [OPTIONS] <MOUNTPOINTS>\n", prgname);
	for (i = 0; longopts_help[i]; i++)
		printf("  -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
			longopts_help[i]);

	exit(status);
}

static void endent(void)
{
	while (mntcnt)
		free(mnttab[--mntcnt]);
	free(mnttab);
	ENDMNTENT;
}

static struct mntent *getent(const char *path)
{
	MNTENT *ent;
	int i;
	
	if (mntcnt == 0) {
		mnttab = err_calloc(mntnum, sizeof(void*));
		atexit(endent);
		SETMNTENT;
	}
	else {
		for (i = 0; i < mntcnt; i++)
			if (strcmp(MNT_FILE(mnttab[i]), path) == 0)
				return mnttab[i];
	}

	while (ent = GETMNTENT) {
		if (mntcnt == mntnum) {
			mntnum += 32;
			mnttab = err_realloc(mnttab, mntnum*sizeof(void*));
		}
		mnttab[mntcnt] = err_malloc(sizeof(MNTENT));
		memcpy(mnttab[mntcnt], ent, sizeof(MNTENT));

		if (strcmp(MNT_FILE(mnttab[mntcnt++]), path) == 0)
			return mnttab[mntcnt-1];
	}

	return NULL;
}

static int fstab_mount(MNTENT *ent, int flag)
{
	pid_t pid;
	int status;
	char *argv[12] = { "mount",
		"-o", MNT_OPTS(ent),
		"-t", MNT_TYPE(ent),
	};

	static int sigsetup = 1;
	static struct sigaction sa_sigint, sa_sigquit;
	static sigset_t ss_savemask;
	static struct sigaction sa_ign;
	static sigset_t ss_child;

	if (sigsetup) {
		/* ignore SIGINT and SIGQUIT */
		sa_ign.sa_handler = SIG_IGN;
		sa_ign.sa_flags = 0;
		sigemptyset(&sa_ign.sa_mask);

		if (sigaction(SIGINT, &sa_ign, &sa_sigint) < 0)
			ERROR("%s: sigaction(SIGINT)", __func__);
		if (sigaction(SIGQUIT, &sa_ign, &sa_sigquit) < 0)
			ERROR("%s: sigaction(SIGQUIT)", __func__);

		/* block SIGCHLD */
		sigemptyset(&ss_child);
		sigaddset(&ss_child, SIGCHLD);
		if (sigprocmask(SIG_BLOCK, &ss_child, &ss_savemask) < 0)
			ERROR("%s: sigprocmask(SIG_BLOCK)", __func__);
		sigsetup = 0;
	}

	if (!flag) {
		argv[5] = MNT_NODE(ent);
		argv[6] = MNT_FILE(ent);
		argv[7] = NULL;
	} else {
#if defined(__linux__)
		argv[5] = "-o";
		argv[6] = "remount";
		argv[7] = MNT_NODE(ent);
		argv[8] = MNT_FILE(ent);
		argv[9] = NULL;
#else
		argv[5] = "-u";
		argv[6] = MNT_NODE(ent);
		argv[7] = MNT_FILE(ent);
		argv[8] = NULL;
#endif
	}

	pid = fork();
	if (pid == 0) {
		/* restore previous signal setup */
		sigprocmask(SIG_SETMASK, &ss_savemask, NULL);
		sigaction(SIGINT, &sa_sigint, NULL);
		sigaction(SIGQUIT, &sa_sigquit, NULL);

		execvp(argv[0], argv);
		ERROR("%s: Failed to execvp()", __func__);
	}
	else if (pid > 0) {
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		else
			return -1;
	}
	else
		ERROR("%s: Failed to fork()", __func__);
}

int main(int argc, char *argv[])
{
	MNTENT *ent;
	int retval = 0, task = 0, opt;

	prgname = strrchr(argv[0], '/');
	if (prgname == NULL)
		prgname = argv[0];
	else
		prgname++;

	/* Parse options */
	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (opt) {
		case 'm':
			task |= FSTAB_MOUNT;
			break;
		case 'r':
			task |= FSTAB_REMOUNT;
			break;
		case 'a':
			task |= FSTAB_ARGS;
			break;
		case 't':
			task |= FSTAB_FSYS;
			break;
		case 'd':
			task |= FSTAB_NODE;
			break;
		case 'o':
			task |= FSTAB_OPTS;
			break;
		case 'v':
			printf("%s version %s\n", prgname, VERSION);
			exit(EXIT_SUCCESS);
		case '?':
		case 'h':
			help_message(EXIT_SUCCESS);
		default:
			help_message(EXIT_FAILURE);
		}
	}

	if ((argc-optind) < 1) {
		ERR("Insufficient number of arguments\n", NULL);
		help_message(EXIT_FAILURE);
	}

	while (argv[optind]) {
		if (ent = getent(argv[optind])) {
			if (task & FSTAB_MOUNT || task & FSTAB_REMOUNT) {
				if (fstab_mount(ent, task & FSTAB_REMOUNT)) {
					ERR("Failed to (re)mount: `%s'\n", argv[optind]);
					retval++;
				}
			}
			else if (task & FSTAB_ARGS)
				printf("-t %s -o %s %s %s\n", MNT_TYPE(ent), MNT_OPTS(ent),
						MNT_NODE(ent), MNT_FILE(ent));
			else {
				if (task & FSTAB_NODE)
					printf("%s", MNT_NODE(ent));
				if (task & FSTAB_FSYS)
					printf(" %s", MNT_TYPE(ent));
				if (task & FSTAB_OPTS)
					printf(" %s", MNT_OPTS(ent));
				printf("\n");
			}
		}
		else {
			ERR("Inexistant fstab entry: `%s'\n", argv[optind]);
			retval++;
		}
		optind++;
	}

	exit(retval);
}
