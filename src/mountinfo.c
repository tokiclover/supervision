/*
 * Simple utility providing providing mount point checking
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
#include <mntent.h>

#undef VERSION
#define VERSION "0.12.0"

const char *prgname;

static FILE *mntptr;
static struct mntent **mnttab;
static size_t mntcnt = 0, mntnum = 32;

enum {
	MOUNT_NDEV  = 0x01,
#define MOUNT_NODE MOUNT_NODE
	MOUNT_OPTS  = 0x02,
#define MOUNT_OPTS MOUNT_OPTS
	MOUNT_FSYS  = 0x04,
#define MOUNT_ARGS MOUNT_ARGS
};

static const char *shortopts = "o:t:nhqv";
static const struct option longopts[] = {
	{ "options",  1, NULL, 'o' },
	{ "fstype",   1, NULL, 't' },
	{ "netdev",   0, NULL, 'n' },
	{ "quiet",    0, NULL, 'q' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Mount options to find",
	"Filesystem to find",
	"Is a network device?",
	"Enable quiet mode",
	"Print help massage",
	"Print version string",
	NULL
};

__NORETURN__ static void help_message(int status)
{
	int i;

	printf("Usage: %s [OPTIONS] <MOUNTPOINTS>\n", prgname);
	for (i = 0; longopts_help[i]; i++) {
		printf("  -%c, --%-8s", longopts[i].val, longopts[i].name);
		if (longopts[i].has_arg)
			printf("<ARG>   %s\n", longopts_help[i]);
		else
			printf("        %s\n", longopts_help[i]);
	}
	exit(status);
}

static void endent(void)
{
	while (mntcnt)
		free(mnttab[--mntcnt]);
	free(mnttab);
	endmntent(mntptr);
}

static struct mntent *getent(const char *path)
{
	struct mntent *ent;
	int i;

	if (mntcnt == 0) {
		if ((mntptr = setmntent("/proc/mounts", "r")) == NULL)
			ERROR("Failed to open `/proc/mounts'", NULL);
		mnttab = err_calloc(mntnum, sizeof(void*));
		atexit(endent);
	}
	else if (path[0] == '*')
		;
	else {
		for (i = 0; i < mntcnt; i++)
			if (strcmp(mnttab[i]->mnt_dir, path) == 0)
				return mnttab[i];
	}

	while (ent = getmntent(mntptr)) {
		if (mntcnt == mntnum) {
			mntnum += 32;
			mnttab = err_realloc(mnttab, mntnum*sizeof(void*));
		}
		mnttab[mntcnt] = err_malloc(sizeof(struct mntent));
		memcpy(mnttab[mntcnt], ent, sizeof(struct mntent));

		if (path[0] == '*')
			return mnttab[mntcnt++];
		if (strcmp(mnttab[mntcnt]->mnt_dir, path) == 0)
			return mnttab[mntcnt++];
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int opt, task = 0;
	struct mntent *ent;
	char *fsys, *mntopts[64], *ptr;
	int i, nopts = 0, retval = 0;
	int quiet = 1;

	prgname = strrchr(argv[0], '/');
	if (prgname == NULL)
		prgname = argv[0];
	else
		prgname++;

	/* Parse options */
	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (opt) {
		case 't':
			task |= MOUNT_FSYS;
			fsys = optarg;
			break;
		case 'n':
			task |= MOUNT_NDEV;
			break;
		case 'o':
			task |= MOUNT_OPTS;
			ptr = mntopts[nopts++] = err_strdup(optarg);
			while (ptr = strchr(ptr, ',')) {
				*ptr++ = '\0';
				mntopts[nopts++] = ptr;
			}
			break;
		case 'q':
			quiet = 0;
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
			if (task & MOUNT_FSYS) {
				if (strcmp(ent->mnt_type, fsys)) {
					retval++;
					continue;
				}
			}
			if (task & MOUNT_NDEV) {
				if (strstr(ent->mnt_opts, "_netdev") == NULL)
					retval++;
			}
			if (task & MOUNT_OPTS)
				for (i = 0; i < nopts; i++)
					if (hasmntopt(ent, mntopts[i]) == NULL)
						retval++;
			if (argv[optind][0] == '*') {
				printf(" %s\n", ent->mnt_dir);
				continue;
			}
		}
		else {
			if (argv[optind][0] == '*')
				break;
			if (quiet)
				ERR("Invalid mount entry: `%s'\n", argv[optind]);
			retval++;
		}
		optind++;
	}

	exit(retval);
}
