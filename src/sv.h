/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv.h  0.14.0 2018/08/18
 */

#ifndef SV_H
#define SV_H

#define SV_COPYRIGHT "@(#) Copyright (c) 2015-2018 tokiclover <tokiclover@gmail.com>\n" \
	"License 2-clause, new, simplified BSD or MIT (at your name choice)\n" \
	"This is free software: you are free to change and distribute it.\n" \
	"There is NO WARANTY, to the extend permitted by law.\n"

#define SV_VERSION "0.14.0"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "sv-conf.h"

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

#define SV_ENVIRON SV_TMPDIR "/environ"
#define SV_LOGFILE SV_TMPDIR "/sv.log"
#define SV_PIDFILE SV_TMPDIR "/sv.pid"
#define SV_RUN_SH SV_LIBDIR "/sh/sv-run.sh"

/* status command to issue to svc_{mark,state} when the STAT querry the status
 * and MARK command remove the status; so the command is only valid with svc_mark
 */
#define SV_SVC_STAT_ACTIVE 'a'
#define SV_SVC_STAT_FAIL 'f'
#define SV_SVC_MARK_FAIL 'F'
#define SV_SVC_STAT_DOWN 'd'
#define SV_SVC_MARK_DOWN 'D'
#define SV_SVC_STAT_PIDS 'p'
#define SV_SVC_STAT_STAR 's'
#define SV_SVC_MARK_STAR 'S'
#define SV_SVC_STAT_WAIT 'w'
#define SV_SVC_MARK_WAIT 'W'

#if defined(ERROR_H) && defined(SV_DEBUG)
#include <stdio.h>
extern int debugfd;
extern FILE *debugfp;
#  undef ERR
#  define ERR(fmt, ...) do {              \
	fprintf(stderr, "%s: %serror%s: "    fmt, progname, \
		print_color(COLOR_RED, COLOR_FG), \
		print_color(COLOR_RST, COLOR_RST), __VA_ARGS__); \
	fprintf(debugfp, "%s: error: "    fmt, progname, __VA_ARGS__); \
  } while (0/*CONSTCOND*/)
#  undef WARN
#  define WARN(fmt, ...) do {             \
	fprintf(stderr, "%s: %swarning%s: " fmt, progname, \
		print_color(COLOR_YLW, COLOR_FG), \
		print_color(COLOR_RST, COLOR_RST), __VA_ARGS__); \
	fprintf(debugfp, "%s: warning: "    fmt, progname, __VA_ARGS__); \
  } while (0/*CONSTCOND*/)
#  undef ERROR
#  define ERROR(fmt, ...)  do {           \
	fprintf(debugfp, "%s: error: "    fmt, progname, __VA_ARGS__); \
	error(errno, "%s: %serror%s: "   fmt, progname, \
		print_color(COLOR_RED, COLOR_FG), \
		print_color(COLOR_RST, COLOR_RST), __VA_ARGS__); \
  } while (0/*CONSTCOND*/)
#  undef DBG
#  define DBG(fmt, ...) do { \
	fprintf(debugfp, "%s: debug: %s:%d: " fmt, progname, __FILE__, __LINE__, __VA_ARGS__); \
	if (debugfp != stderr)   \
	fprintf(stderr , "%s: debug: %s:%d: " fmt, progname, __FILE__, __LINE__, __VA_ARGS__); \
  } while (0/*CONSTCOND*/)
#endif /* ERROR_H && SV_DEBUG */

extern int sv_debug;
extern int sv_parallel;
extern int sv_level;
extern int sv_init;
extern int sv_system;
extern unsigned int sv_timeout;
extern int svc_deps;
extern int svc_quiet;
extern const char *progname;
extern const char *signame[];

enum {
	SV_SHUTDOWN_LEVEL,
#define SV_SHUTDOWN_LEVEL SV_SHUTDOWN_LEVEL
	SV_SINGLE_LEVEL,
#define SV_SINGLE_LEVEL SV_SINGLE_LEVEL
	SV_NOWNETWORK_LEVEL,
#define SV_NOWNETWORK_LEVEL SV_NOWNETWORK_LEVEL
	SV_DEFAULT_LEVEL,
#define SV_DEFAULT_LEVEL SV_DEFAULT_LEVEL
	SV_SYSINIT_LEVEL,
#define SV_SYSINIT_LEVEL SV_SYSINIT_LEVEL
	SV_SYSBOOT_LEVEL,
#define SV_SYSBOOT_LEVEL SV_SYSBOOT_LEVEL
	SV_REBOOT_LEVEL,
#define SV_REBOOT_LEVEL SV_REBOOT_LEVEL
};
extern const char *const sv_init_level[];

extern void sv_cleanup(void);

/* tiny function to print end string like the shell end() counterpart */
extern int svc_end(const char *svc, int status);

/* simple function to help debug (vfprintf(3) clone) */
extern int svc_log(const char *fmt, ...);
#define LOG_ERR(fmt, ...)  svc_log("%s: error: "  fmt, applet, __VA_ARGS__)
#define LOG_WARN(fmt, ...) svc_log("%s: warning: " fmt, applet, __VA_ARGS__)
#define LOG_INFO(fmt, ...) svc_log("%s: info: "  fmt, applet, __VA_ARGS__)

extern void svc_sigsetup(void);

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

/* update SV_TMPDIR/environ environment file */
extern off_t ENVIRON_OFF;
extern int svc_environ_update(off_t off);

#ifdef __cplusplus
}
#endif

#endif /* SV_H */
