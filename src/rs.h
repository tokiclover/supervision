/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision (Scripts Framework).
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

#ifndef SYSCONFDIR
# define SYSCONFDIR "/etc"
#endif
#define SV_LIBDIR "/lib/sv"
#define RS_SVCDIR SYSCONFDIR "/rs.d"
#define SV_SVCDIR SYSCONFDIR "/sv"
#define SV_SERVICE SYSCONFDIR "/service"
#if defined(STATIC_SERVICE)
# define SV_RUNDIR SV_SERVICE
#elif defined(__linux__) || (defined(__FreeBSD_kernel__) && \
		defined(__GLIBC__)) || defined(__GNU__)
#define SV_RUNDIR "/run/service"
#else
#define SV_RUNDIR "/var/run/service"
#endif
#define SV_TMPDIR SV_RUNDIR "/.tmp"

struct RS_Stage {
	int level;
	const char *type;
} RS_STAGE;

/*
 * constant definition used with the following const arrays
 */
enum {
	RS_STAGE_RUNSCRIPT,
#define RS_STAGE_RUNSCRIPT RS_STAGE_RUNSCRIPT
	RS_STAGE_SUPERVISION
#define RS_STAGE_SUPERVISION RS_STAGE_SUPERVISION
};
extern const char *const rs_stage_type[];

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

/* singly-linked list using queue(3) */
typedef struct RS_String {
	char *str;
	SLIST_ENTRY(RS_String) entries;
} RS_String_T;
typedef SLIST_HEAD(RS_StringList, RS_String) RS_StringList_T;

/* number of priority level per dependency type */
#define RS_DEP_PRIORITY 4

typedef struct RS_DepType {
	/* dependency type {after,before,use,need} */
	char *type;
	/* priority level list [0-RS_DEP_PRIORITY] */
	RS_StringList_T *priority[RS_DEP_PRIORITY];
	SLIST_ENTRY(RS_DepType) entries;
} RS_DepType_T;
typedef SLIST_HEAD(RS_DepTypeList, RS_DepType) RS_DepTypeList_T;

typedef struct RS_SvcDeps {
	/* dependency type {after,before,use,need} */
	char *svc;
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
int          rs_stringlist_mov (RS_StringList_T *src, RS_StringList_T *dst, RS_String_T *ent);
void         rs_stringlist_free(RS_StringList_T *list);

/* the same used for dependencies list */
RS_DepTypeList_T *rs_deplist_load(void);
RS_DepTypeList_T *rs_deplist_new(void);
RS_DepType_T *rs_deplist_add (RS_DepTypeList_T *list, const char *str);
RS_DepType_T *rs_deplist_adu (RS_DepTypeList_T *list, const char *str);
RS_DepType_T *rs_deplist_find(RS_DepTypeList_T *list, const char *str);
int           rs_deplist_del (RS_DepTypeList_T *list, const char *str);
void          rs_deplist_free(RS_DepTypeList_T *list);

/* the same used for service dependencies */
RS_SvcDepsList_T *rs_svcdeps_load(void);
RS_SvcDepsList_T *rs_svcdeps_new(void);
RS_SvcDeps_T *rs_svcdeps_add (RS_SvcDepsList_T *list, const char *svc);
RS_SvcDeps_T *rs_svcdeps_adu (RS_SvcDepsList_T *list, const char *svc);
RS_SvcDeps_T *rs_svcdeps_find(RS_SvcDepsList_T *list, const char *svc);
int           rs_svcdeps_del (RS_SvcDepsList_T *list, const char *svc);
void          rs_svcdeps_free(RS_SvcDepsList_T *list);

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
