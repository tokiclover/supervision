/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 */

#ifndef _RS_H
#define _RS_H

#define RS_COPYRIGHT "Copyright (C) 2015-6 tokiclover <tokiclover@gmail.com>\n" \
	"License 2-clause, new, simplified BSD or MIT (at your name choice)\n" \
	"This is free software: you are free to change and distribute it.\n" \
	"There is NO WARANTY, to the extend permitted by law.\n"


#include "queue.h"
#include "config.h"

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

extern int rs_stage;

enum {
	RS_STAGE_SYSINIT,
#define RS_STAGE_SYSINIT RS_STAGE_SYSINIT
	RS_STAGE_BOOT,
#define RS_STAGE_BOOT RS_STAGE_BOOT
	RS_STAGE_DEFAULT,
#define RS_STAGE_DEFAULT RS_STAGE_DEFAULT
	RS_STAGE_SHUTDOWN
#define RS_STAGE_SHUTDOWN RS_STAGE_SHUTDOWN
};
extern const char *const rs_stage_name[];

extern const char *const rs_deps_type[];
#define RS_DEPS_TYPE 4
#define RS_DEPS_AFTER  1
#define RS_DEPS_BEFORE 0
#define RS_DEPS_USE    2
#define RS_DEPS_NEED   3

/* singly-linked list using queue(3) */
typedef struct RS_String {
	char *str;
	SLIST_ENTRY(RS_String) entries;
} RS_String_T;
typedef SLIST_HEAD(RS_StringList, RS_String) RS_StringList_T;

typedef struct RS_SvcDeps {
	/* dependency type {after,before,use,need} */
	char *svc;
	char *virt;
	/* priority level list [0-RS_DEPS_TYPE] */
	RS_StringList_T *deps[RS_DEPS_TYPE];
	SLIST_ENTRY(RS_SvcDeps) entries;
} RS_SvcDeps_T;
typedef SLIST_HEAD(RS_SvcDepsList, RS_SvcDeps) RS_SvcDepsList_T;

/* string list helpers to manage string list using queue(3) */
RS_StringList_T *rs_stringlist_new(void);
RS_String_T *rs_stringlist_add (RS_StringList_T *list, const char *str);
RS_String_T *rs_stringlist_adu (RS_StringList_T *list, const char *str);
RS_String_T *rs_stringlist_find(RS_StringList_T *list, const char *str);
int          rs_stringlist_del (RS_StringList_T *list, const char *str);
int          rs_stringlist_rem (RS_StringList_T *list, RS_String_T *elm);
int          rs_stringlist_mov (RS_StringList_T *src, RS_StringList_T *dst, RS_String_T *ent);
void         rs_stringlist_free(RS_StringList_T *list);

/* the same used for service dependencies */
extern RS_SvcDepsList_T *service_deplist;
void rs_svcdeps_load(void);
RS_SvcDepsList_T *rs_svcdeps_new(void);
RS_SvcDeps_T *rs_svcdeps_add (RS_SvcDepsList_T *list, const char *svc);
RS_SvcDeps_T *rs_svcdeps_adu (RS_SvcDepsList_T *list, const char *svc);
RS_SvcDeps_T *rs_svcdeps_find(RS_SvcDepsList_T *list, const char *svc);
int           rs_svcdeps_del (RS_SvcDepsList_T *list, const char *svc);

RS_StringList_T **rs_deptree_load(void);
void              rs_deptree_free(RS_StringList_T **array);
extern size_t     rs_deptree_prio;

/* find a virtual service e.g. {net,dev,logger} */
RS_SvcDeps_T *rs_virtual_find(const char *svc);

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
