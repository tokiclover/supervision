/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs-deps.h  0.13.6.4 2016/12/24
 */

#ifndef RS_DEPS_H
#define RS_DEPS_H

#include "sv.h"
#include "rs-list.h"
#include "helper.h"

#define RS_DEPS_TYPE   5
#define RS_DEPS_AFTER  1
#define RS_DEPS_BEFORE 0
#define RS_DEPS_USE    2
#define RS_DEPS_NEED   3
#define RS_DEPS_KWD    4

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
	int timeout;
	/* priority level list [0-RS_DEPS_TYPE] */
	RS_StringList_T *deps[RS_DEPS_TYPE];
	TAILQ_ENTRY(RS_SvcDeps) entries;
} RS_SvcDeps_T;
typedef TAILQ_HEAD(RS_SvcDepsList, RS_SvcDeps) RS_SvcDepsList_T;

typedef struct RS_DepTree {
	RS_StringList_T **tree;
	RS_StringList_T  *list;
	size_t prio;
	size_t size;
} RS_DepTree_T;

struct RS_Services {
	RS_StringList_T  *svclist;
	RS_SvcDepsList_T *svcdeps;
	RS_SvcDeps_T **virt_svcdeps;
	size_t virt_count;
};
extern struct RS_Services SERVICES;

struct svcrun {
	RS_String_T *svc;
	RS_SvcDeps_T *dep;
	const char *name;
	const char *path;
	pid_t pid;
	int lock;
	int argc;
	const char **argv;
	const char **envp;
	const char **ARGV;
	int cmd, tmp, mark,
		sig, status;
	pid_t cld;
};

/* the same used for service dependencies */
extern RS_SvcDeps_T     *rs_svcdeps_load(const char *svc);

extern void             svc_deptree_load(RS_DepTree_T *deptree);
extern void              rs_deptree_load(RS_DepTree_T *deptree);
extern void              rs_deptree_free(RS_DepTree_T *deptree);
extern RS_StringList_T  *rs_svclist_load(char *dir_path);

/* find a virtual service e.g. {net,dev,logger} */
extern RS_SvcDeps_T  *rs_virtsvc_find(RS_StringList_T *svclist, const char *svc);

#ifdef __cplusplus
}
#endif

#endif /* RS_DEPS_H */
