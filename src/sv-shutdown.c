/*
 * Utility providing an interface {halt,reboot,shutdown,poweroff}
 * per supervision backend.
 *
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-shutdown.c
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "error.h"
#include "helper.h"
#include <grp.h>
#include <pwd.h>
#include <utmpx.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/reboot.h>

#define VERSION "0.12.0"

#ifndef LIBDIR
# define LIBDIR "/lib"
#endif
#ifndef SYSCONFDIR
# define SYSCONFDIR "/etc"
#endif
#define SV_LIBDIR LIBDIR "/sv"
#define SV_SVCDIR SYSCONFDIR "/sv"
#define SV_INIT_STAGE SV_LIBDIR "sh/init-stage"
#define SV_SVC_BACKEND SV_SVCDIR "/.opt/SVC_BACKEND"

#define SV_ACTION_SHUTDOWN 0
#define SV_ACTION_REBOOT   6

const char *prgname;

static const char *shortopts = "06rshpfFEHPntkuv";
static const struct option longopts[] = {
	{ "reboot",   0, NULL, 'r' },
	{ "shutdown", 0, NULL, 's' },
	{ "halt",     0, NULL, 'h' },
	{ "poweroff", 0, NULL, 'p' },
	{ "fast",     0, NULL, 'f' },
	{ "fsck",     0, NULL, 'F' },
	{ "force",    0, NULL, 'E' },
	{ "nosync",   0, NULL, 'n' },
	{ "time",     1, NULL, 't' },
	{ "message",  0, NULL, 'k' },
	{ "usage",    0, NULL, 'u' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"System reboot",
	"System shutdown",
	"System halt     (-0 alias)",
	"System poweroff (-0 alias)",
	"Skip  fsck(8) on reboot",
	"Force fsck(8) on reboot",
	"Force halt/reboot/shutdown",
	"Disable filesystem synchronizations",
	"Send signal after waiting",
	"Broadcast message only",
	"Print help massage",
	"Print version string",
	NULL
};

__NORETURN__ static void help_message(int status)
{
	int i = 0;

	printf("Usage: %s [OPTIONS] ACTION [-t TIME] [MESSAGE]\n", prgname);
	printf("    -6, -%c, --%-9s         %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	printf("    -0, -%c, --%-9s         %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	printf("    -H, -%c, --%-9s         %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	printf("    -P, -%c, --%-9s         %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	for ( ; longopts_help[i]; i++) {
		printf("        -%c, --%-9s", longopts[i].val, longopts[i].name);
		if (longopts[i].has_arg)
			printf("<ARG>    ");
		else
			printf("         ");
		puts(longopts_help[i]);
	}

	exit(status);
}

#ifndef UT_LINESIZE
# ifdef __UT_LINESIZE
#  define UT_LINESIZE __UT_LINESIZE
# else
#  define UT_LINESIZE 32
# endif
#endif

static int sv_wall(char *message)
{
	struct utmpx *utxent;
	char dev[UT_LINESIZE+8];
	int fd, n;
	size_t len;

	if (message == NULL) {
		errno = ENOENT;
		return -1;
	}
	len = strlen(message);

	setutxent();
	while ((utxent = getutxent())) {
		if (utxent->ut_type != USER_PROCESS)
			continue;
		snprintf(dev, UT_LINESIZE, "/dev/%s", utxent->ut_line);
		if ((fd = open(dev, O_WRONLY | O_CLOEXEC | O_NOCTTY)) < 0)
			ERROR("Failed to open `%s'", dev);
		if ((n = write(fd, message, len)) != len)
			ERROR("Failed to write to `%s'", dev);
		close(fd);
	}
	endutxent();

	return 0;
}

__NORETURN__ int sv_shutdown(int action)
{
	FILE *fp;
	size_t len = 0;
	char *line = NULL, *ptr = NULL;
	char *argv[8], opt[8];
	const char ent[] = "__SV_NAM__";
	size_t siz = sizeof(ent)-1;

	argv[0] = "rs", argv[2] = NULL;
	if (action)
		argv[1] = "reboot";
	else
		argv[1] = "shutdown";

	if ((fp = fopen(SV_SVC_BACKEND, "r")))
		while (rs_getline(fp, &line, &len) > 0)
			if (strncmp(line, ent, siz) == 0) {
				ptr = shell_string_value(line+siz+1);
				ptr = err_strdup(ptr);
				free(line);
				fclose(fp);
				break;
			}
	else
		ERR("Failed to open `%s': %s\n", SV_SVC_BACKEND, strerror(errno));

	if (ptr) {
		if (strcmp(ptr, "runit") == 0) {
			snprintf(opt, sizeof(opt), "%d", action);
			argv[0] = "runit-init";
			argv[1] = opt;
		}
		else if (strcmp(ptr, "s6") == 0) {
			snprintf(opt, sizeof(opt), "-%d", action);
			argv[0] = "s6-svscanctl";
			argv[1] = opt;
		}
		else if (strncmp(ptr, "daemontools", 11) == 0)
			;
		else
			ERR("Invalid supervision backend: %s\n", ptr);
		free(ptr);
	}
	else
		ERR("%s: Failed to get supervision backend\n", __func__);

	execvp(argv[0], argv);
	ERROR("Failed to execlp(%s, %s)", *argv, argv[1]);
}

int main(int argc, char *argv[])
{
	int action = -1, fd;
	int rb_flag = 0, rb_force = 0, rb_sync = 1;
	int open_flags = O_CREAT|O_WRONLY|O_NOFOLLOW;
	mode_t open_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
	int message = 0, opt, retval;

	prgname = strrchr(argv[0], '/');
	if (prgname == NULL)
		prgname = argv[0];
	else
		prgname++;

	/* Parse options */
	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (opt) {
		case '0':
		case 'P':
		case 'p':
		case 's':
			action = SV_ACTION_SHUTDOWN;
			rb_flag = RB_POWER_OFF;
			break;
		case 'H':
		case 'h':
			action = SV_ACTION_SHUTDOWN;
			rb_flag = RB_HALT_SYSTEM;
			break;
		case '6':
		case 'r':
			action = SV_ACTION_REBOOT;
			rb_flag = RB_AUTOBOOT;
			break;
		case 'E':
			rb_force = 1;
			break;
		case 'n':
			rb_sync = 0;
			break;
		case 'f':
			if ((fd = open("/fastboot", open_flags, open_mode)) < 1)
				ERROR("Failed to create /fastboot", NULL);
			close(fd);
			break;
		case 'F':
			if ((fd = open("/forcefsck", open_flags, open_mode)) < 1)
				ERROR("Failed to create /forcefsck", NULL);
			close(fd);
			break;
		case 't': /* ignored */
			break;
		case 'k':
			message = 1;
			break;
		case 'v':
			printf("%s version %s\n", prgname, VERSION);
			exit(EXIT_SUCCESS);
		case '?':
		case 'u':
			help_message(EXIT_SUCCESS);
		default:
			help_message(EXIT_FAILURE);
		}
	}


	if (argv[optind])
		retval = sv_wall(argv[optind]);
	if (message)
		exit(retval);
	else if (action == -1) {
		ERR("-0|-6 required to proceed; see `%s -u'\n", prgname);
		exit(EXIT_FAILURE);
	}

	if (rb_sync)
		sync();
	if (rb_force)
		retval = reboot(rb_flag);
	else
		retval = sv_shutdown(action);

	exit(retval);
}
