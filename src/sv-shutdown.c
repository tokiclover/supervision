/*
 * Utility providing an interface {halt,reboot,shutdown,poweroff}
 * per supervision backend.
 *
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision (Scripts Framework).
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 */

#include "error.h"
#include "helper.h"
#include <grp.h>
#include <pwd.h>
#include <utmpx.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/reboot.h>

#define VERSION "0.10.0"

#ifndef SYSCONFDIR
# define SYSCONFDIR "/etc"
#endif
#ifndef SV_SVCDIR
# define SV_SVCDIR SYSCONFDIR "/sv"
#endif
#ifndef SV_LIBDIR
# define SV_LIBDIR "/lib/sv"
#endif
#define SV_INIT_STAGE SV_LIBDIR "sh/init-stage"
#define SV_SVC_BACKEND SV_SVCDIR "/.opt/SVC_BACKEND"

#define SV_ACTION_SHUTDOWN 0
#define SV_ACTION_REBOOT   6

const char *prgname;

static const char *shortopts = "06rshHpPfFEntkuv";
static const struct option longopts[] = {
	{ "reboot",   0, NULL, 'r' },
	{ "shutdown", 0, NULL, 's' },
	{ "halt",     0, NULL, 's' },
	{ "poweroff", 0, NULL, 's' },
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
	"System reboot   (-6 alias)",
	"System shutdown (-0 alias)",
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

	printf("Usage: %s ACTION [-t TIME] [MESSAGE]\n", prgname);
	printf("    -6, -%c, --%-9s         %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	for ( ; i < 4; i++)
		printf("    -0, -%c, --%-9s         %s\n", longopts[i].val, longopts[i].name,
			longopts_help[i]);
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
	while (utxent = getutxent()) {
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

int sv_shutdown(int action)
{
	FILE *fp;
	size_t len = 0;
	char *act, cmd[512], *line = NULL, *ptr = NULL;
	const char *ent = "__SV_NAM__";
	size_t siz = strlen(ent);

	if ((fp = fopen(SV_SVC_BACKEND, "r")) == NULL)
		ERROR("Failed to open `%s'", SV_SVC_BACKEND);

	while (rs_getline(fp, &line, &len) > 0) {
		if (strncmp(line, ent, siz) == 0) {
			ptr = shell_string_value(line+siz+1);
			ptr = err_strdup(ptr);
			free(line);
			fclose(fp);
			break;
		}
	}

	if (ptr == NULL) {
		ERR("Failed to get supervision backend\n", NULL);
		return -1;
	}

	if (strcmp(ptr, "runit") == 0)
		snprintf(cmd, ARRAY_SIZE(cmd), "runit-init %d", action);
	else if (strcmp(ptr, "s6") == 0)
		snprintf(cmd, ARRAY_SIZE(cmd), "s6-svscanctl -%d", action);
	else if (strncmp(ptr, "daemontools", 11) == 0) {
		if (action)
			act = "reboot";
		else
			act = "shutdown";
		snprintf(cmd, ARRAY_SIZE(cmd), "ACTION=%s LEVEL=%d %s -%d", act,
				action, SV_INIT_STAGE, action);
	}
	else {
		ERR("Invalid supervision backend\n", NULL);
		return -1;
	}
	free(ptr);

	return system(cmd);
}

int main(int argc, char *argv[])
{
	int action = 0, fd;
	int RB_FLAG, FORCE = 0, SYNC = 1;
	int message = 0, opt, retval;
	pid_t pid;

	prgname = strrchr(argv[0], '/');
	if (prgname == NULL)
		prgname = argv[0];
	else
		prgname++;

	/* Parse options */
	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (opt) {
		case '0':
		case 's':
		case 'p':
		case 'P':
			action = SV_ACTION_SHUTDOWN+1;
			RB_FLAG = RB_POWER_OFF;
			break;
		case 'h':
		case 'H':
			action = SV_ACTION_SHUTDOWN+1;
			RB_FLAG = RB_HALT_SYSTEM;
			break;
		case '6':
		case 'r':
			action = SV_ACTION_REBOOT+1;
			RB_FLAG = RB_AUTOBOOT;
			break;
		case 'E':
			FORCE = 1;
			break;
		case 'n':
			SYNC = 0;
			break;
		case 'f':
			if ((fd = open("/fastboot", O_CREAT|O_WRONLY|O_NOFOLLOW,
						S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 1)
				ERROR("Failed to create /fastboot", NULL);
			close(fd);
			break;
		case 'F':
			if ((fd = open("/forcefsck", O_CREAT|O_WRONLY|O_NOFOLLOW,
						S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 1)
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

	if (message == 0 && action-- == 0) {
		ERR("-0|-6 required to proceed; see `%s -u'\n", prgname);
		exit(EXIT_FAILURE);
	}

	if (SYNC)
		sync();
	if (argv[optind]) {
		if (sv_wall(argv[optind]))
			ERR("Failed to broadcast message\n", NULL);
	}
	if (FORCE)
		retval = reboot(RB_FLAG);
	else
		retval = sv_shutdown(action);

	exit(retval);
}
