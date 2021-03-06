/*
 * Utility providing an interface to SysVinit's halt, reboot, shutdown,
 * poweroff utilities.
 *
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)initctl.c  0.15.0 2019/03/14
 */

#include <initreq.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include "error.h"
#include "helper.h"
#include "config.h"

const char *progname;

int main(int argc, char *argv[])
{
	struct init_request ireq;
	int fd;
	int len;
	char arg[] = EXEC_PREFIX "/sbin/sv-shutdown -0";
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
	if ((fd = open(INIT_FIFO, O_RDONLY | O_NDELAY | O_NONBLOCK)) < 0)
		ERROR("Failed to open %s", INIT_FIFO);
	if (open(INIT_FIFO, O_WRONLY | O_NDELAY | O_NONBLOCK) < 0)
		ERROR("Failed to open %s", INIT_FIFO);

	for (;;) {
		len = read(fd, (char *)&ireq, sizeof(ireq));
		if (len < 0)
			ERROR("Failed to read %s", INIT_FIFO);
		else if (len == 0)
			continue;
		else if (len != sizeof(ireq)) {
			ERR("Invalid %d bytes length.\n", len);
			continue;
		}
		if (ireq.magic != INIT_MAGIC) {
			ERR("Invalid magic header %x.\n", ireq.magic);
			continue;
		}
		if (ireq.cmd == INIT_CMD_RUNLVL) {
			sprintf(arg+strlen(arg)-1, "%c", ireq.runlevel);
			if (system(arg))
				ERR("Failed to execute `%s'", arg);
		}
	}
	return(EXIT_SUCCESS);
}
