/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv.h  0.13.0 2016/12/26
 */

#ifndef SV_H
#define SV_H

#define SV_COPYRIGHT "@(#) Copyright (c) 2015, 2016 tokiclover <tokiclover@gmail.com>\n" \
	"License 2-clause, new, simplified BSD or MIT (at your name choice)\n" \
	"This is free software: you are free to change and distribute it.\n" \
	"There is NO WARANTY, to the extend permitted by law.\n"

#define SV_VERSION "0.13.0"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LIBDIR
# define LIBDIR "/lib"
#endif
#ifndef SYSCONFDIR
# define SYSCONFDIR "/etc"
#endif
#define SV_LIBDIR LIBDIR "/sv"
#define SV_SVCDIR SYSCONFDIR "/sv"
#if defined(__linux__) || (defined(__FreeBSD_kernel__) && \
		defined(__GLIBC__)) || defined(__GNU__)
#define SV_RUNDIR "/run/sv"
#else
#define SV_RUNDIR "/var/run/sv"
#endif
#define SV_TMPDIR SV_RUNDIR "/.tmp"
#define SV_TMPDIR_DEPS SV_TMPDIR "/deps"
#define SV_SVCDEPS_FILE SV_TMPDIR_DEPS "/svcdeps"

#define SV_TMPDIR_DOWN SV_TMPDIR "/down"
#define SV_TMPDIR_FAIL SV_TMPDIR "/fail"
#define SV_TMPDIR_PIDS SV_TMPDIR "/pids"
#define SV_TMPDIR_STAR SV_TMPDIR "/star"
#define SV_TMPDIR_WAIT SV_TMPDIR "/wait"

#define SV_LOGFILE SV_TMPDIR "/sv.log"
#define SV_PIDFILE SV_TMPDIR "/sv.pid"
#define RS_RUNSCRIPT SV_LIBDIR "/sh/runscript"

/* status command to issue to svc_{mark,state} when the STAT querry the status
 * and MARK command remove the status; so the command is only valid with svc_mark
 */
#define SV_SVC_STAT_FAIL 'f'
#define SV_SVC_MARK_FAIL 'F'
#define SV_SVC_STAT_DOWN 'd'
#define SV_SVC_MARK_DOWN 'D'
#define SV_SVC_STAT_PIDS 'p'
#define SV_SVC_STAT_STAR 's'
#define SV_SVC_MARK_STAR 'S'
#define SV_SVC_STAT_WAIT 'w'
#define SV_SVC_MARK_WAIT 'W'

extern int sv_nohang;
extern int sv_parallel;
extern int sv_runlevel;
extern int sv_stage;
extern int svc_deps;
extern int svc_quiet;
extern const char *progname;

enum {
	SV_RUNLEVEL_SHUTDOWN,
#define SV_RUNLEVEL_SHUTDOWN SV_RUNLEVEL_SHUTDOWN
	SV_RUNLEVEL_SINGLE,
#define SV_RUNLEVEL_SINGLE SV_RUNLEVEL_SINGLE
	SV_RUNLEVEL_NONETWORK,
#define SV_RUNLEVEL_NONETWORK SV_RUNLEVEL_NONETWORK
	SV_RUNLEVEL_DEFAULT,
#define SV_RUNLEVEL_DEFAULT SV_RUNLEVEL_DEFAULT
	SV_RUNLEVEL_SYSINIT,
#define SV_RUNLEVEL_SYSINIT SV_RUNLEVEL_SYSINIT
	SV_RUNLEVEL_BOOT,
#define SV_RUNLEVEL_BOOT SV_RUNLEVEL_BOOT
	SV_RUNLEVEL_REBOOT,
#define SV_RUNLEVEL_REBOOT SV_RUNLEVEL_REBOOT
};
extern const char *const sv_runlevel_name[];

/*
 * retrieve a configuration value like getenv(3)
 * @envp: configuration list;
 * @env: configuration name;
 */
extern const char *rs_getconf(const char *env);
/*
 * simple helper to ease yes/no querries of config settings
 * @return: true/false;
 */
extern int rs_conf_yesno(const char *env);

/* tiny function to print end string like the shell end() counterpart */
extern int svc_end(const char *svc, int status);

/* simple function to help debug (vfprintf(3) clone) */
extern int svc_log(const char *fmt, ...);
#define LOG_ERR(fmt, ...)  svc_log("ERROR: %s: " fmt, applet, __VA_ARGS__)
#define LOG_WARN(fmt, ...) svc_log( "WARN: %s: " fmt, applet, __VA_ARGS__)
#define LOG_INFO(fmt, ...) svc_log(       "%s: " fmt, applet, __VA_ARGS__)

extern void svc_sigsetup(void);
extern int svc_mark(const char *svc, int status, const char *what);

enum {
	SV_SVC_CMD_STOP,
#define SV_SVC_CMD_STOP    SV_SVC_CMD_STOP
	SV_SVC_CMD_START,
#define SV_SVC_CMD_START   SV_SVC_CMD_START
	SV_SVC_CMD_ADD,
#define SV_SVC_CMD_ADD     SV_SVC_CMD_ADD
	SV_SVC_CMD_DEL,
#define SV_SVC_CMD_DEL     SV_SVC_CMD_DEL
	SV_SVC_CMD_DESC,
#define SV_SVC_CMD_DESC    SV_SVC_CMD_DESC
	SV_SVC_CMD_REMOVE,
#define SV_SVC_CMD_REMOVE  SV_SVC_CMD_REMOVE
	SV_SVC_CMD_RESTART,
#define SV_SVC_CMD_RESTART SV_SVC_CMD_RESTART
	SV_SVC_CMD_STATUS,
#define SV_SVC_CMD_STATUS  SV_SVC_CMD_STATUS
	SV_SVC_CMD_ZAP
#define SV_SVC_CMD_ZAP     SV_SVC_CMD_ZAP
};
extern const char *const sv_svc_cmd[];

#ifdef __cplusplus
}
#endif

#endif /* SV_H */
