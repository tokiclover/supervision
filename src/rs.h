/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision (Scripts Framework).
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 */

#ifndef __RS_H__
#define __RS_H__

#include "queue.h"

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
#define SV_RUNDIR SV_SERVICE
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
#define RS_DEPS_TYPE ARRAY_SIZE(rs_deps_type)

/* singly-linked list using queue(3) */
typedef struct RS_String {
	char *str;
	SLIST_ENTRY(RS_String) entries;
} RS_String_T;
typedef SLIST_HEAD(RS_StringList, RS_String) RS_StringList_T;

/* number of priority level per dependency type */
#define RS_DEP_PRIORITY 3

typedef struct RS_DepType {
	/* dependency type {after,before,use,need} */
	char *type;
	/* priority level list [0-RS_DEP_PRIORITY] */
	RS_StringList_T *priority[RS_DEP_PRIORITY];
	SLIST_ENTRY(RS_DepType) entries;
} RS_DepType_T;
typedef SLIST_HEAD(RS_DepTypeList, RS_DepType) RS_DepTypeList_T;

/* string list helpers to manage string list using queue(3) */
RS_StringList_T *rs_stringlist_new(void);
RS_String_T *rs_stringlist_add(RS_StringList_T *list, const char *str);
RS_String_T *rs_stringlist_adu(RS_StringList_T *list, const char *str);
RS_String_T *rs_stringlist_find(RS_StringList_T *list, const char *str);
int          rs_stringlist_del(RS_StringList_T *list, const char *str);
void         rs_stringlist_free(RS_StringList_T *list);

/* the same used for dependencies list */
RS_DepTypeList_T *rs_deplist_load(void);
RS_DepTypeList_T *rs_deplist_new(void);
RS_DepType_T *rs_deplist_add(RS_DepTypeList_T *list, const char *str);
RS_DepType_T *rs_deplist_adu(RS_DepTypeList_T *list, const char *str);
RS_DepType_T *rs_deplist_find(RS_DepTypeList_T *list, const char *str);
int           rs_deplist_del(RS_DepTypeList_T *list, const char *str);
void          rs_deplist_free(RS_DepTypeList_T *list);

/* and finaly the same applied to service */
RS_String_T *rs_deplist_add_svc(RS_DepType_T *list, const char *str, int index);
RS_String_T *rs_deplist_adu_svc(RS_DepType_T *list, const char *str, int index);
RS_String_T *rs_deplist_find_svc(RS_DepType_T *list, const char *str, int index);
int          rs_deplist_del_svc(RS_DepType_T *list, const char *str, int index);

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

#endif /* __RS_H__ */
