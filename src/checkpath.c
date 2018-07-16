/*
 * Simple utility providing providing checkpath/mktemp
 *
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)checkpath.c  0.14.0 2018/07/14
 */

#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <getopt.h>
#include <fcntl.h>
#include "error.h"
#include "helper.h"

#define VERSION "0.11.0"

const char *progname;

enum {
	TYPE_FILE   = 0x01,
#define TYPE_FILE TYPE_FILE
	TYPE_DIR    = 0x02,
#define TYPE_DIR TYPE_DIR
	TYPE_PIPE   = 0x04,
#define TYPE_PIPE TYPE_PIPE
	TYPE_CHECK  = 0x10,
#define TYPE_CHECK TYPE_CHECK
	TYPE_MKTEMP = 0x20,
#define TYPE_MKTEMP TYPE_MKTEMP
	TYPE_TRUNC  = 0x40,
#define TYPE_TRUNC TYPE_TRUNC
	TYPE_WRITE  = 0x80,
#define TYPE_WRITE TYPE_WRITE
};

static const char *shortopts = "DdFfg:hm:o:Pp:qv";
static const struct option longopts[] = {
	{ "dir-trunc",  0, NULL, 'D' },
	{ "file-trunc", 0, NULL, 'F' },
	{ "dir",      0, NULL, 'd' },
	{ "file",     0, NULL, 'f' },
	{ "pipe",     0, NULL, 'P' },
	{ "group",    1, NULL, 'g' },
	{ "owner",    1, NULL, 'o' },
	{ "mode",     1, NULL, 'm' },
	{ "tmpdir",   1, NULL, 'p' },
	{ "quiet",    0, NULL, 'q' },
	{ "help",     0, NULL, 'h' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Create an empty directory",
	"Create an empty file",
	"Create a directory",
	"Create a regular file",
	"Create a named pipe (FIFO)",
	"User group",
	"User owner[:group]",
	"Octal permision mode",
	"Enable mktemp mode",
	"Enable quiet mode",
	"Print help massage",
	"Print version string",
	NULL
};


__attribute__((__noreturn__)) static void help_message(int status)
{
	int i;

	printf("Usage: %s [OPTIONS] <TEMPLATES>|<DIRS>|<FILES>\n", progname);
	for (i = 0; longopts_help[i]; i++) {
		printf("    -%c, --%-10s", longopts[i].val, longopts[i].name);
		if (longopts[i].has_arg)
			printf("<ARG>    ");
		else
			printf("         ");
		puts(longopts_help[i]);
	}

	exit(status);
}

static int checkpath(char *file, char *tmpdir, uid_t uid, gid_t gid, mode_t mode,
		int type)
{
	char path[2048], *tmp;
	int fd, len, off = 0;
	int open_flags = 0, unlink_flags;
	DIR *dir;
	struct dirent *ent;
	static mode_t m = 0;
	struct stat stb, std;

	memset(&stb, 0, sizeof(stb));
	if (type & TYPE_CHECK)
		lstat(file, &stb);
	if (!m)
		m = umask(0);

	if (type & TYPE_MKTEMP) {
		len = strlen(file);
		if (strcmp(file+len-6, "XXXXXX")) {
			ERR("Invalid template: `%s'\n", file);
			return -1;
		}

		if (file[0] == '/')
			off++;
		snprintf(path, sizeof(path), "%s/%s", tmpdir, file+off);

		if (type & TYPE_FILE) {
			fd = mkstemp(path);
			if (fd < 0) {
				ERR("Failed to create `%s' file: %s\n", path,
						strerror(errno));
				return -1;
			}
			close(fd);
		}
		else if (type & TYPE_DIR)
			tmp = mkdtemp(path);

		if (strlen(path))
			puts(path);
		else {
			ERR("Failed to create `%s'\n", file);
			return -1;
		}
		tmp = path;
	}
	else if (type & TYPE_PIPE) {
		if (!S_ISFIFO(stb.st_mode)) {
			if (!mode) /* 0600 */
				mode = S_IRUSR | S_IWUSR;
			fd = mkfifo(file, mode);
			if (fd < 0) {
				ERR("Failed to create `%s' named pipe (FIFO): %s\n", file,
						strerror(errno));
				return -1;
			}
		}
		tmp = file;
	}
	else if (type & TYPE_DIR) {
		if (!S_ISDIR(stb.st_mode)) {
			if (!mode) /* 0755 */
				mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
			fd = mkdir(file, mode);
			if (fd < 0) {
				ERR("Failed to create `%s' directory: %s\n", file, strerror(errno));
				return -1;
			}
		}
		else if (type & TYPE_TRUNC) {
			dir = opendir(file);
			if (dir == NULL) {
				ERR("Failed to open `%s' direcotry: %s\n", file, strerror(errno));
				return -1;
			}
			fd = dirfd(dir);
			if (fd < 0) {
				ERR("%s\n", strerror(errno));
				return -1;
			}
			while ((ent = readdir(dir))) {
				if (strcmp(ent->d_name, "..") == 0 || strcmp(ent->d_name, ".") == 0)
					continue;

				memset(&std, 0, sizeof(std));
				unlink_flags = 0;
				if (fstatat(fd, ent->d_name, &std, AT_SYMLINK_NOFOLLOW) < 0) {
					ERR("Failed to fstatat(...%s/%s...): %s\n", file, ent->d_name,
							strerror(errno));
					continue;
				}
				if (S_ISDIR(std.st_mode))
					unlink_flags = AT_REMOVEDIR;
				if (unlinkat(fd, ent->d_name, unlink_flags) < 0)
					ERR("Failed to remove \%s/%s\n", file, ent->d_name);
			}
			closedir(dir);
		}
		tmp = file;
	}
	else if (type & TYPE_FILE) {
		if (!S_ISREG(stb.st_mode) || type & TYPE_TRUNC) {
			if (!mode) /* 0644 */
				mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
			open_flags |= O_CREAT | O_WRONLY | O_NOFOLLOW;
			if (type & TYPE_TRUNC)
				open_flags |= O_TRUNC;
			fd = open(file, open_flags, mode);
			if (fd < 0) {
				ERR("Failed to create `%s' file: %s\n", file, strerror(errno));
				return -1;
			}
		}
		tmp = file;
	}
	else return -1;

	if (mode && (stb.st_mode & 07777) != mode) {
		if (S_ISLNK(stb.st_mode)) {
			ERR("Not changing permission mode for `%s' (symlink)\n", tmp);
			return -1;
		}
		if (chmod(tmp, mode)) {
			ERR("Failed to change permision mode for `%s': %s\n", tmp,
					strerror(errno));
		}
	}

	if (uid && gid && ((stb.st_uid != uid) || (stb.st_gid != gid))) {
		if (S_ISLNK(stb.st_mode)) {
			ERR("Not changing owner for `%s' (symlink)\n", tmp);
			return -1;
		}
		if (chown(tmp, uid, gid)) {
			ERR("Failed to change owner for `%s': %s\n", tmp, strerror(errno));
			return -1;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	uid_t uid = geteuid();
	gid_t gid = getegid();
	mode_t mode = 0;
	int id, opt, retval = 0, type = TYPE_CHECK;
	char *grn = NULL, *pwn = NULL, *ptr = NULL;
	char *grnam = NULL, *pwnam = NULL, *tmpdir = NULL;
	struct passwd *pwd = NULL;
	struct group *grp = NULL;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		progname++;

	/* Parse options */
	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (opt) {
		case 'F':
			type |= TYPE_FILE | TYPE_TRUNC;
			break;
		case 'D':
			type |= TYPE_DIR | TYPE_TRUNC;
			break;
		case 'f':
			type |= TYPE_FILE;
			break;
		case 'd':
			type |= TYPE_DIR;
			break;
		case 'P':
			type |= TYPE_PIPE;
			break;
		case 'm':
			sscanf(optarg, "%o", &mode);
			break;
		case 'o':
			pwnam = optarg;
			break;
		case 'g':
			grnam = optarg;
			break;
		case 'q': /* ignored */
			break;
		case 'v':
			printf("%s version %s\n", progname, VERSION);
			exit(EXIT_SUCCESS);
		case 'p':
			type |= TYPE_MKTEMP;
			tmpdir = optarg;
			break;
		case '?':
		case 'h':
			help_message(EXIT_SUCCESS);
		default:
			help_message(EXIT_FAILURE);
		}
	}

	if ((argc-optind) < 1 || !(type & ~TYPE_CHECK)) {
		fprintf(stderr, "%s: Insufficient number of arguments\n", progname);
		help_message(EXIT_FAILURE);
	}

	if (pwnam) {
		ptr = pwn = err_strdup(pwnam);
		grn = strchr(pwn, ':');
		if (grn)
			*grn++ = '\0';
	}
	if (grnam)
		grn = grnam;

	if (pwn) {
		if (sscanf(pwn, "%d", &id) == 1)
			pwd = getpwuid((uid_t) id);
		else
			pwd = getpwnam(pwn);
		if (pwd)
			uid = pwd->pw_uid;
		else {
			fprintf(stderr, "%s: Failed to get owner\n", progname);
			exit(EXIT_FAILURE);
		}
	}
	if (grn) {
		if (sscanf(grn, "%d", &id) == 1)
			grp = getgrgid((gid_t) id);
		else
			grp = getgrnam(grn);
		if (grp)
			gid = grp->gr_gid;
		else {
			fprintf(stderr, "%s: Failed to get group\n", progname);
			exit(EXIT_FAILURE);
		}
	}
	free(ptr);

	while (argv[optind]) {
		if (checkpath(argv[optind], tmpdir, uid, gid, mode, type))
			retval++;
		optind++;
	}

	exit(retval);
}
