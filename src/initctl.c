/*
 * Utility providing an interface to SysVinit's halt, reboot, shutdown,
 * poweroff utilities.
 *
 * Copyright (c) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)initctl.c  0.12.6.4 2016/12/28
 */

#include <initreq.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "error.h"
#include "helper.h"

#ifndef LIBDIR
# define LIBDIR "/lib"
#endif
#define INIT_REQ_LEN 384
#define SV_SHUTDOWN LIBDIR "/sv/sbin/sv-shutdown"

const char *progname;

int main(int argc, char *argv[])
{
	struct init_request ireq;
	int fd;
	int len;
	char arg[PATH_MAX];
	mode_t m;

	progname = strrchr(argv[0], '/');
	if (!progname)
		progname = argv[0];
	else
		progname++;

	if (!file_test(INIT_FIFO, 'p')) {
		m = umask(0);
		if (mkfifo(INIT_FIFO, 0600) < 0)
			ERROR("Failed to create %s FIFO", INIT_FIFO);
		umask(m);
	}
	if ((fd = open(INIT_FIFO, O_RDONLY)) < 0)
		ERROR("Failed to open %s", INIT_FIFO);

	for (;;) {
		len = read(fd, (char *)&ireq, sizeof(ireq));
		if (len < 0)
			ERROR("Failed to read %s", INIT_FIFO);
		else if (len == 0)
			continue;
		else if (len != INIT_REQ_LEN) {
			ERR("Invalid %d bytes length.\n", len);
			continue;
		}
		if (ireq.magic != INIT_MAGIC) {
			ERR("Invalid magic header %x.\n", ireq.magic);
			continue;
		}
		if (ireq.cmd == INIT_CMD_RUNLVL) {
			snprintf(arg, ARRAY_SIZE(arg), "%s -%d -t%d", SV_SHUTDOWN,
					ireq.runlevel, ireq.sleeptime);
			if (system(arg))
				ERR("Failed to execute `%s'", arg);
		}
	}
	return(EXIT_SUCCESS);
}
