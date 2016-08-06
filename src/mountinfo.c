/*
 * Simple utility providing providing mount point checking
 *
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)mountinfo.c
 */

#include "rs-list.h"
#include <getopt.h>
#include <regex.h>

#if defined(__DragonFly__) || defined(__FreeBSD__)
# include <sys/ucred.h>
# include <sys/mount.h>
# define F_FLAGS f_flags
#elif defined(BSD) && !defined(__GNU__)
# include <sys/statvfs.h>
# define statfs statvfs
# define F_FLAGS f_flag
#elif defined (__linux__) || (defined(__FreeBSD_kernel__) && \
		defined(__GLIBC__)) || defined(__GNU__)
# include <mntent.h>
#else
# error "Unsupported Operating System"
#endif

#define VERSION "0.12.0"
#define REGFREE(var) if(var) { regfree(var); free(var); }
#define REGCOMP(var) REGFREE(var); var = comp_regex(optarg)

const char *prgname;
static RS_StringList_T *mount_list;

enum {
	mount_point,
	mount_node,
	mount_type,
	mount_opts,
};
struct mntargs {
	regex_t *reg_node,
			*reg_type,
			*reg_opts,
			*reg_point,
			*reg_NODE,
			*reg_TYPE,
			*reg_OPTS,
			*reg_POINT;
	RS_StringList_T *mnt_list;
	int mnt_type;
	int mnt_net;
};

static char *filter_mount_point(struct mntargs *args, char *node, char *point,
		char *type, char *opts, int netdev)
{
	char *p;

	if (args->mnt_net && netdev < 1)
		return NULL;
	else {
		if (args->reg_type &&  regexec(args->reg_type, type, 0, NULL, 0))
			return NULL;
		if (args->reg_node &&  regexec(args->reg_node, node, 0, NULL, 0))
			return NULL;
		if (args->reg_opts &&  regexec(args->reg_opts, opts, 0, NULL, 0))
			return NULL;
		if (args->reg_TYPE && !regexec(args->reg_TYPE, type, 0, NULL, 0))
			return NULL;
		if (args->reg_NODE && !regexec(args->reg_NODE, node, 0, NULL, 0))
			return NULL;
		if (args->reg_OPTS && !regexec(args->reg_OPTS, opts, 0, NULL, 0))
			return NULL;
		if (args->reg_point &&  regexec(args->reg_point, point, 0, NULL, 0))
			return NULL;
		if (args->reg_POINT && !regexec(args->reg_POINT, point, 0, NULL, 0))
			return NULL;
	}
	if (args->mnt_list && !rs_stringlist_find(args->mnt_list, point))
		return NULL;

	switch(args->mnt_type) {
	case mount_point:
		p = point;
		break;
	case mount_type:
		p = type;
		break;
	case mount_node:
		p = node;
		break;
	case mount_opts:
		p = opts;
		break;
	default:
		p = NULL;
		break;
	}
	if (p)
		rs_stringlist_add(mount_list, p);
	return p;
}

#if defined(BSD) && !defined(__GNU__)

/* Translate the mounted options to english
 * This is taken directly from FreeBSD mount.c */
static struct mntoption {
	int flag;
	const char name;
} mntoptions[] = {
	{ MNT_ASYNC,       "asynchronous" },
	{ MNT_EXPORTED,    "NFS exported" },
	{ MNT_LOCAL,       "local" },
	{ MNT_NOATIME,     "noatime" },
	{ MNT_NOEXEC,      "noexec" },
	{ MNT_NOSUID,      "nosuid" },
#ifdef MNT_NOSYMFOLLOW
	{ MNT_NOSYMFOLLOW, "nosymfollow" },
#endif
	{ MNT_QUOTA,       "with quotas" },
	{ MNT_RDONLY,      "read-only" },
	{ MNT_SYNCHRONOUS, "synchronous" },
	{ MNT_UNION,       "union" },
#ifdef MNT_NOCLUSTERR
	{ MNT_NOCLUSTERR,  "noclusterr" },
#endif
#ifdef MNT_NOCLUSTERW
	{ MNT_NOCLUSTERW,  "noclusterw" },
#endif
#ifdef MNT_SUIDDIR
	{ MNT_SUIDDIR,     "suiddir" },
#endif
	{ MNT_SOFTDEP,     "soft-updates" },
#ifdef MNT_MULTILABEL
	{ MNT_MULTILABEL,  "multilabel" },
#endif
#ifdef MNT_ACLS
	{ MNT_ACLS,        "acls" },
#endif
#ifdef MNT_GJOURNAL
	{ MNT_GJOURNAL,    "gjournal" },
#endif
	{ 0, NULL }
};

static void find_mount_point(struct mntargs *args)
{
	struct statfs *mnt_pnts;
	char *mnt_opts = NULL;
	int i, mnt_num, netdev;
	struct mntoption *opt;
	uint64_t flags;
	size_t len, siz;

	if (!(mnt_num = getmntinfo(&mnt_num, MNT_NOWAIT)))
		ERROR("Failed to exec getmntinfo()", NULL);

	for (i = 0; i < mnt_num; i++) {
		netdev = -1, siz = 1;
		flags = mnti_pnts[i].F_FLAGS & MNT_VISFLAGMASK;

		for (opt = mntoptions; flags && opt->flag; opt++) {
			if (opt->flag & flags) {
				len = strlen(opt->name);
				if (opt->flag == MNT_LOCAL)
					netdev = 0;
				else
					netdev = 1;
				mnt_opts = err_realloc(mnt_opts, len+siz+1);
				snprintf(mnt_opts+siz-1, ",%s", opt->name, len);
				siz += len+1;
			}
			flags &= ~opt->flag;
		}

		filter_mount_point(args, mnt_pnts[i].f_mntfromname,
				mnt_pnts[i].f_mntonname, mnt_pnts[i].f_fstypename,
				mnt_opts, netdev);
		free(mnt_opts);
		mnt_opts = NULL;
	}
}

#elif defined (__linux__) || (defined (__FreeBSD_kernel__) && \
		defined(__GLIBC__))

static FILE *mntptr;
static struct mntent **mnttab;
static size_t mntcnt = 0, mntnum = 32;

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
	const char file[] = "/etc/fstab";
	int i;

	if (mntcnt == 0) {
		if ((mntptr = setmntent(file, "r")) == NULL)
			ERROR("Failed to open `%s'", file);
		mnttab = err_calloc(mntnum, sizeof(void*));
		atexit(endent);
	}
	else {
		for (i = 0; i < mntcnt; i++)
			if (strcmp(mnttab[i]->mnt_dir, path) == 0)
				return mnttab[i];
	}

	while ((ent = getmntent(mntptr))) {
		if (mntcnt == mntnum) {
			mntnum += 32;
			mnttab = err_realloc(mnttab, mntnum*sizeof(void*));
		}
		mnttab[mntcnt] = err_malloc(sizeof(struct mntent));
		memcpy(mnttab[mntcnt], ent, sizeof(struct mntent));

		if (strcmp(mnttab[mntcnt]->mnt_dir, path) == 0)
			return mnttab[mntcnt++];
	}

	return NULL;
}

static void find_mount_point(struct mntargs *args)
{
	char *node, *point, *type, *opts;
	const char file[] = "/proc/mounts";
	char buf[BUFSIZ], *ptr;
	FILE *fp;
	struct mntent *ent;
	int netdev;

	if ((fp = fopen(file, "r")) == NULL)
		ERROR("Failed to open(%s...)", file);

	while (fgets(buf, BUFSIZ, fp)) {
		netdev = -1;
		ptr = buf;
		node  = strsep(&ptr, " ");
		point = strsep(&ptr, " ");
		type  = strsep(&ptr, " ");
		opts  = strsep(&ptr, " ");

		if (strcmp(node, "rootfs") == 0 || strcmp(type, "rootfs") == 0)
			continue;
		if ((ent = getent(point))) {
			if (strstr(ent->mnt_opts, "_netdev"))
				netdev = 0;
			else
				netdev = 1;
		}
		filter_mount_point(args, node, point, type, opts, netdev);
	}
	fclose(fp);
}

#endif

static const char *shortopts = "bD:d:fO:o:P:p:T:t:mnhqv";
static const struct option longopts[] = {
	{ "device-skip-regex",  1, NULL, 'D' },
	{ "options-skip-regex", 1, NULL, 'O' },
	{ "fstype-skip-regex",  1, NULL, 'T' },
	{ "mpoint-skip-regex",  1, NULL, 'P' },
	{ "device-regex",  1, NULL, 'd' },
	{ "options-regex", 1, NULL, 'o' },
	{ "fstype-regex",  1, NULL, 't' },
	{ "mpoint-regex",  1, NULL, 'p' },
	{ "options",  0, NULL, 'm' },
	{ "device",   0, NULL, 'b' },
	{ "fstype",   0, NULL, 'f' },
	{ "netdev",   0, NULL, 'n' },
	{ "quiet",    0, NULL, 'q' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Device node regex to skip",
	"Mount options regex to skip",
	"Filesystem to regex skip",
	"Mountpoint to regex skip",
	"Device node regex to find",
	"Mount options regex to find",
	"Filesystem to regex find",
	"Mountpoint to regex find",
	"Print mount options",
	"Print device node",
	"Print filesystem",
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
		printf("  -%c, --%-20s", longopts[i].val, longopts[i].name);
		if (longopts[i].has_arg)
			printf("<ARG>   %s\n", longopts_help[i]);
		else
			printf("        %s\n", longopts_help[i]);
	}

	exit(status);
}

static regex_t *comp_regex(const char *regex)
{
	regex_t *re = err_malloc(sizeof(regex_t));
	int rv;
	char buf[256];

	if ((rv = regcomp(re, regex, REG_EXTENDED | REG_NOSUB))) {
		regerror(rv, (const regex_t *)re, buf, sizeof(buf));
		ERR("Failed to compile `%s': %s\n", regex, buf);
		exit(EXIT_FAILURE);
	}

	return re;
}

int main(int argc, char *argv[])
{
	int opt;
	struct mntargs args;
	int retval = 0;
	int quiet = 1;
	RS_String_T *s;

	prgname = strrchr(argv[0], '/');
	if (prgname == NULL)
		prgname = argv[0];
	else
		prgname++;
	memset(&args, 0, sizeof(args));

	/* Parse options */
	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (opt) {
		case 'D':
			REGCOMP(args.reg_NODE);
			break;
		case 'd':
			REGCOMP(args.reg_node);
			break;
		case 'O':
			REGCOMP(args.reg_OPTS);
			break;
		case 'o':
			REGCOMP(args.reg_opts);
			break;
		case 'P':
			REGCOMP(args.reg_POINT);
			break;
		case 'p':
			REGCOMP(args.reg_point);
			break;
		case 'T':
			REGCOMP(args.reg_TYPE);
			break;
		case 't':
			REGCOMP(args.reg_type);
			break;
		case 'b':
			args.mnt_type = mount_node;
			break;
		case 'm':
			args.mnt_type = mount_opts;
			break;
		case 'f':
			args.mnt_type = mount_type;
			break;
		case 'n':
			args.mnt_net = 1;
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

	if ((argc-optind) > 0) {
		args.mnt_list = rs_stringlist_new();
		for ( ; argv[optind]; optind++) {
			rs_stringlist_add(args.mnt_list, argv[optind]);
			retval++;
		}
	}
	mount_list = rs_stringlist_new();
	find_mount_point(&args);

	SLIST_FOREACH(s, mount_list, entries) {
		if (retval)
			retval--;
		if (quiet)
			puts(s->str);
	}

	rs_stringlist_free(&mount_list);
	REGFREE(args.reg_type);
	REGFREE(args.reg_TYPE);
	REGFREE(args.reg_node);
	REGFREE(args.reg_NODE);
	REGFREE(args.reg_opts);
	REGFREE(args.reg_OPTS);

	exit(retval);
}
