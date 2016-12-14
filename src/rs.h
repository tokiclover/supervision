/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs.h
 */

#ifndef _RS_H
#define _RS_H

#define RS_COPYRIGHT "Copyright (C) 2015-6 tokiclover <tokiclover@gmail.com>\n" \
	"License 2-clause, new, simplified BSD or MIT (at your name choice)\n" \
	"This is free software: you are free to change and distribute it.\n" \
	"There is NO WARANTY, to the extend permitted by law.\n"


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
#if defined(STATIC_SERVICE)
# define SV_RUNDIR SV_SERVICE
#elif defined(__linux__) || (defined(__FreeBSD_kernel__) && \
		defined(__GLIBC__)) || defined(__GNU__)
#define SV_RUNDIR "/run/sv"
#else
#define SV_RUNDIR "/var/run/sv"
#endif
#define SV_TMPDIR SV_RUNDIR "/.tmp"
#define SV_TMPDIR_DEPS SV_TMPDIR "/deps"
#define RS_SVCDEPS_FILE SV_TMPDIR_DEPS "/svcdeps"

extern int rs_stage;
extern int rs_runlevel;

enum {
	RS_RUNLEVEL_SHUTDOWN,
#define RS_RUNLEVEL_SHUTDOWN RS_RUNLEVEL_SHUTDOWN
	RS_RUNLEVEL_SINGLE,
#define RS_RUNLEVEL_SINGLE RS_RUNLEVEL_SINGLE
	RS_RUNLEVEL_NONETWORK,
#define RS_RUNLEVEL_NONETWORK RS_RUNLEVEL_NONETWORK
	RS_RUNLEVEL_DEFAULT,
#define RS_RUNLEVEL_DEFAULT RS_RUNLEVEL_DEFAULT
	RS_RUNLEVEL_SYSINIT,
#define RS_RUNLEVEL_SYSINIT RS_RUNLEVEL_SYSINIT
	RS_RUNLEVEL_BOOT,
#define RS_RUNLEVEL_BOOT RS_RUNLEVEL_BOOT
	RS_RUNLEVEL_REBOOT,
#define RS_RUNLEVEL_REBOOT RS_RUNLEVEL_REBOOT
};
extern const char *const rs_runlevel_name[];

/*
 * retrieve a configuration value like getenv(3)
 * @envp: configuration list;
 * @env: configuration name;
 */
const char *rs_getconf(const char *env);
/*
 * simple helper to ease yes/no querries of config settings
 * @return: true/false;
 */
int rs_conf_yesno(const char *env);

#ifdef __cplusplus
}
#endif

#endif /* _RS_H */
