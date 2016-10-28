/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs-deps.h
 */

#ifndef _RS_DEPS_H
#define _RS_DEPS_H

#include "rs.h"
#include "rs-list.h"
#include "helper.h"

#define RS_DEPS_TYPE   4
#define RS_DEPS_AFTER  1
#define RS_DEPS_BEFORE 0
#define RS_DEPS_USE    2
#define RS_DEPS_NEED   3

#define SV_DEPGEN SV_LIBDIR "/sh/dep"
#define SV_INIT_STAGE SV_LIBDIR "/sh/init-stage"
#define RS_DEPTREE_PRIO 16
/* safety net for broken cyclic dependency */
#define RS_DEPTREE_MAX  1024

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RS_SvcDeps {
	/* dependency type {after,before,use,need} */
	char *svc;
	char *virt;
	/* priority level list [0-RS_DEPS_TYPE] */
	RS_StringList_T *deps[RS_DEPS_TYPE];
	SLIST_ENTRY(RS_SvcDeps) entries;
} RS_SvcDeps_T;
typedef SLIST_HEAD(RS_SvcDepsList, RS_SvcDeps) RS_SvcDepsList_T;

typedef struct RS_DepTree {
	RS_StringList_T **tree;
	RS_StringList_T  *list;
	size_t prio;
	size_t size;
} RS_DepTree_T;

/* the same used for service dependencies */
extern RS_SvcDepsList_T *service_deplist;
extern void rs_svcdeps_load(void);
extern RS_SvcDepsList_T *rs_svcdeps_new(void);
extern RS_SvcDeps_T *rs_svcdeps_add (RS_SvcDepsList_T *list, const char *svc);
extern RS_SvcDeps_T *rs_svcdeps_adu (RS_SvcDepsList_T *list, const char *svc);
extern RS_SvcDeps_T *rs_svcdeps_find(RS_SvcDepsList_T *list, const char *svc);
extern int           rs_svcdeps_del (RS_SvcDepsList_T *list, const char *svc);

extern RS_SvcDepsList_T *service_deplist;
extern void              rs_deptree_load(RS_DepTree_T *deptree);
extern void              rs_deptree_free(RS_DepTree_T *deptree);
extern RS_StringList_T  *rs_svclist_load(char *dir_path);

/* find a virtual service e.g. {net,dev,logger} */
extern size_t         rs_virtual_count;
extern RS_SvcDeps_T **virtual_deplist;
extern RS_SvcDeps_T  *rs_virtual_find(const char *svc, RS_StringList_T *svclist);

extern void svc_deptree_load(RS_DepTree_T *deptree);

#ifdef __cplusplus
}
#endif

#endif /* _RS_DEPS_H */
