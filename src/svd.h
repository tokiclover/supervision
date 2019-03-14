/*
 * Copyright (c) 2016-2019 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * new BSD License included in the distriution of this package.
 *
 * @(#)svd.h  0.15.0 2019/03/14
 */

#ifndef SVD_H
#define SVD_H

#include "config.h"

#ifndef SYSCONFDIR
# define SYSCONFDIR "/etc"
#endif
#ifndef RUNDIR
# if defined(__linux__) || (defined(__FreeBSD_kernel__) && \
		defined(__GLIBC__)) || defined(__GNU__)
#  define RUNDIR "/run"
# else
#  define RUNDIR "/var/run"
# endif
#endif
#ifndef SV_CONFDIR
# define SV_SVCDIR SYSCONFDIR "/sv"
#endif
#ifndef SV_RUNDIR
# define SV_RUNDIR RUNDIR "/sv"
#endif

#ifndef SV_TIMEOUT
# define SV_TIMEOUT 1U
#endif
#ifndef SV_RUN_NUM
# define SV_RUN_NUM 100
#endif
#ifndef SVD
# define SVD EXEC_PREFIX "/bin/svd"
#endif
#ifndef SVL
# define SVL EXEC_PREFIX "/bin/svl"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ST_WANT_UP   'u'
#define ST_WANT_DOWN 'd'
enum {
	ST_DOWN,
#define ST_DOWN ST_DOWN
	ST_RUN,
#define ST_RUN ST_RUN
	ST_FINISH
#define ST_FINISH ST_FINISH
};

#define ST_BUF_SIZ 24LU
static char st_buf[ST_BUF_SIZ];

enum {
	ST_OFF_PSEC  =  0LU,
	ST_OFF_NSEC  =  8LU,
	ST_OFF_PID   = 16LU,
	ST_OFF_PAUSE = 20LU,
#define ST_OFF_PAUSE ST_OFF_PAUSE
	ST_OFF_UP    = 21LU,
#define ST_OFF_UP ST_OFF_UP
	ST_OFF_TERM  = 22LU,
#define ST_OFF_TERM ST_OFF_TERM
	ST_OFF_RUN   = 23LU,
#define ST_OFF_RUN ST_OFF_RUN
};

enum {
	SW_DOWN   = 0x00,
#define SW_DOWN SW_DOWN
	SW_PAUSE  = 0x01,
#define SW_PAUSE SW_PAUSE
	SW_TERM   = 0x02,
#define SW_TERM SW_TERM
	SW_EXIT   = 0x04,
#define SW_EXIT SW_EXIT
	SW_RUN    = 0x08,
#define SW_RUN SW_RUN
	SW_FINISH = 0x10,
#define SW_FINISH SW_FINISH
};
enum {
	SV_EXEC_RUN,
#define SV_EXEC_RUN SV_EXEC_RUN
	SV_EXEC_FINISH
#define SV_EXEC_FINISH SV_EXEC_FINISH
};

static struct sdent {
	int sd_pid, sd_stat, sd_ctrl, sd_want, sd_ret;
	int fd_lock, fd_ctrl, fd_status, sv_run, sv_log;
	void *fp_lock, *fp_ctrl, *fp_status;
	char *sv_pid, *sv_stat, *sv_status, *sv_control, *sv_lock, *sv_ok;
	struct timespec sv_time;
} sde[2] = {
	[0] = {
		.sd_want = SW_RUN,
		.sd_ctrl = SW_RUN,
		.sv_control = "supervise/control",
		.sv_pid     = "supervise/pid",
		.sv_status  = "supervise/status",
		.sv_stat    = "supervise/stat",
		.sv_lock    = "supervise/lock",
		.sv_ok      = "supervise/ok",
	},
	[1] = {
		.sd_ctrl = SW_RUN,
		.sv_control = "log/supervise/control",
		.sv_pid     = "log/supervise/pid",
		.sv_stat    = "log/supervise/stat",
		.sv_status  = "log/supervise/status",
		.sv_lock    = "log/supervise/lock",
		.sv_ok      = "log/supervise/ok",
	}
}, *sd_svc = &sde[0], *sd_log = &sde[1];

#ifdef __cplusplus
}
#endif
#endif /*SVD_H*/
