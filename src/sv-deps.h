/*
 * Copyright (c) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs-deps.h  0.13.0 2016/01/02
 */

#ifndef SV_DEPS_H
#define SV_DEPS_H

#include "sv.h"
#include "sv-list.h"
#include "helper.h"

#define SV_SVCDEPS_TYPE   5
#define SV_SVCDEPS_AFTER  1
#define SV_SVCDEPS_BEFORE 0
#define SV_SVCDEPS_USE    2
#define SV_SVCDEPS_NEED   3
#define SV_SVCDEPS_KWD    4

/* some macro to set/get service options */
#define SV_SVCOPTS_NOHANG 0x01
#define SV_SVCOPTS_GET(dep, opt) ((dep)->options  & (opt))
#define SV_SVCOPTS_SET(dep, opt) ((dep)->options |= (opt))

#define SV_DEPGEN SV_LIBDIR "/sh/dep"
#define SV_INIT_STAGE SV_LIBDIR "/sh/init-stage"
#define SV_DEPTREE_INC 16
/* safety net for broken cyclic dependency */
#define SV_DEPTREE_MAX  256

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SV_SvcDeps {
	/* dependency type {after,before,use,need} */
	unsigned int did;
	char *svc;
	char *virt;
	int timeout;
	int options;
	/* priority level list [0-SV_SVCDEPS_TYPE] */
	SV_StringList_T *deps[SV_SVCDEPS_TYPE];
	TAILQ_ENTRY(SV_SvcDeps) entries;
	/* align on 16 words */
	int __pad[4];
} SV_SvcDeps_T;
typedef TAILQ_HEAD(SV_SvcDepsList, SV_SvcDeps) SV_SvcDepsList_T;

typedef struct SV_DepTree {
	SV_StringList_T **tree;
	SV_StringList_T  *list;
	size_t prio;
	size_t size;
} SV_DepTree_T;

struct SV_Services {
	SV_StringList_T  *svclist;
	SV_SvcDepsList_T *svcdeps;
	SV_SvcDeps_T **virt_svcdeps;
	size_t virt_count;
};
extern struct SV_Services SERVICES;

struct svcrun {
	SV_String_T *svc;
	SV_SvcDeps_T *dep;
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
extern SV_SvcDeps_T     *sv_svcdeps_load(const char *svc);

extern void             svc_deptree_load(SV_DepTree_T *deptree);
extern void              sv_deptree_load(SV_DepTree_T *deptree);
extern void              sv_deptree_free(SV_DepTree_T *deptree);
extern SV_StringList_T  *sv_svclist_load(char *dir_path);

/* find a virtual service e.g. {net,dev,logger} */
extern SV_SvcDeps_T  *sv_virtsvc_find(SV_StringList_T *svclist, const char *svc);

#ifdef __cplusplus
}
#endif

#endif /* SV_DEPS_H */
