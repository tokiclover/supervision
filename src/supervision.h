/*
 * Copyright (c) 2016-2019 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * new BSD License included in the distriution of this package.
 *
 * @(#)supervision.h  0.15.0 2019/03/14
 */

#ifndef SUPERVISION_H
#define SUPERVISION_H

#ifdef __cplusplus
extern "C" {
#endif

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
# define SV_RUNDIR RUNDIR "/supervision"
#endif

#ifndef SV_PIDFILE
# define SV_PIDFILE ".tmp/supervision.pid"
#endif
#ifndef SV_FIFO
# define SV_FIFO    ".tmp/supervision.ctl"
#endif
#ifndef SVD
# define SVD EXEC_PREFIX "/bin/svd"
#endif

#ifdef __cplusplus
}
#endif
#endif /*SUPERVISION_H*/
