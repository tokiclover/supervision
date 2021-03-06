/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs-deps.h  0.14.0 2019/01/12
 */

#ifndef SV_DEPS_H
#define SV_DEPS_H

#include "error.h"
#include "sv-list.h"
#include "sv.h"
#include "helper.h"

#define SV_SVCDEPS_TYPE   4
#define SV_SVCDEPS_AFTER  1
#define SV_SVCDEPS_BEFORE 0
#define SV_SVCDEPS_USE    2
#define SV_SVCDEPS_NEED   3

/* some macro to set/get service options */
#define SV_SVCOPTS_GET(dep, opt) (dep->options  & (opt))
#define SV_SVCOPTS_SET(dep, opt) (dep->options |= (opt))

/* some macro to set/get keyword bit */
#define SV_KEYWORD_GET(dep, kwd) (dep->keyword  & (1<<kwd))
#define SV_KEYWORD_SET(dep, kwd) (dep->keyword |= (1<<kwd))

#define SV_DEPS_SH SV_LIBDIR "/sh/sv-deps.sh"
#define SV_INIT_SH SV_LIBDIR "/sh/sv-init.sh"
#define SV_DEPTREE_INC 16
/* safety net for broken cyclic dependency */
#define SV_DEPTREE_MAX  256

#ifdef __cplusplus
extern "C" {
#endif

enum {
	SV_KEYWORD_TIMEOUT = 1,
	SV_KEYWORD_SHUTDOWN,
	SV_KEYWORD_SUPERVISION,
	SV_KEYWORD_DOCKER,
	SV_KEYWORD_JAIL,
	SV_KEYWORD_LXC,
	SV_KEYWORD_OPENVZ,
	SV_KEYWORD_PREFIX,
	SV_KEYWORD_SYSTEMD_NSPAWN,
	SV_KEYWORD_UML,
	SV_KEYWORD_VSERVER,
	SV_KEYWORD_XEN0,
	SV_KEYWORD_XENU,
};
extern const char *const sv_keywords[];

typedef struct SV_SvcDeps {
	/* dependency type {after,before,use,need} */
	unsigned int did;
	unsigned int timeout;
	unsigned long int options;
	unsigned long int keyword;
	int status;
	int command;
	/* priority level list [0-SV_SVCDEPS_TYPE] */
	char *svc;
	char *virt;
	SV_StringList_T *deps[SV_SVCDEPS_TYPE];
	TAILQ_ENTRY(SV_SvcDeps) entries;
} SV_SvcDeps_T;
typedef TAILQ_HEAD(SV_SvcDepsList, SV_SvcDeps) SV_SvcDepsList_T;

typedef struct SV_DepTree {
	SV_StringList_T **tree;
	SV_StringList_T  *list;
	size_t prio;
	size_t size;
} SV_DepTree_T;
extern SV_DepTree_T DEPTREE;

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
	const char **argv;
	const char **envp;
	const char **ARGV;
	void *rl_svc;
	pid_t pid;
	int lock;
	int argc;
	int cmd, mark, sig,
		status;
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
