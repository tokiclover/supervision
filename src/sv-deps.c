/*
 * Copyright (c) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs-deps.c  0.13.0 2016/12/30
 */

#include "sv-deps.h"
#include <dirent.h>

static const char *const sv_svcdeps_type[] = { "before", "after", "use", "need",
	"keyword" };

struct SV_Services SERVICES = {
	.svclist      = NULL,
	.svcdeps      = NULL,
	.virt_svcdeps = NULL,
	.virt_count   = 0,
};

static int  sv_deptree_file_load(SV_DepTree_T *deptree);
static int  sv_deptree_file_save(SV_DepTree_T *deptree);
static void sv_svcdeps_free(void);
static SV_SvcDepsList_T *sv_svcdeps_new(void);
static SV_SvcDeps_T *sv_svcdeps_add (const char *svc);
static SV_SvcDeps_T *sv_svcdeps_adu (const char *svc);
static SV_SvcDeps_T *sv_svcdeps_find(const char *svc);
/* load generate service dependency */
static int           sv_svcdeps_gen(const char *svc);
static void sv_virtsvc_insert(SV_SvcDeps_T *elm);

static void sv_runlevel_migrate(void);

static void sv_deptree_alloc(SV_DepTree_T *deptree)
{
	int p;

	deptree->size += SV_DEPTREE_PRIO;
	deptree->tree = err_realloc(deptree->tree, deptree->size*sizeof(void*));
	for (p = deptree->size-SV_DEPTREE_PRIO; p < deptree->size; p++)
		deptree->tree[p] = sv_stringlist_new();
}

void sv_deptree_free(SV_DepTree_T *deptree)
{
	int i;
	for (i = 0; i < deptree->size; i++)
		sv_stringlist_free(&deptree->tree[i]);
	deptree->size = 0;
}

static int sv_deptree_add(int type, int prio, SV_String_T *svc, SV_DepTree_T *deptree)
{
#define FIND_SVCDEPS(ent, first, last) add = 1;                           \
	for (p = first; p < last; p++)                                        \
		if (sv_stringlist_find(deptree->tree[p], ent->str)) {             \
			add = 0; break;                                               \
		}

	char *s = svc->str;
	SV_SvcDeps_T *d = svc->data;
	SV_String_T *ent;
	int add, pri;
	int p, t, r;

	if (s == NULL)
		return 0;
	/* add service to list if and only if, either a service is {use,need}ed or
	 * belongs to this particular init-stage/runlevel */
	if (type < SV_SVCDEPS_USE && !sv_stringlist_find(deptree->list, s))
		return -prio;
	if (d) {
		if (d->virt) s = d->svc;
	}
	else
		svc->data = d = sv_svcdeps_find(s);
	/* insert the real service instead of a virtual one */
	if (!d && (d = sv_virtsvc_find(deptree->list, s)))
		s = d->svc, svc->data = d;
	if (!d)
		return -1;

	if (prio < 0) {
		if (d->deps[SV_SVCDEPS_AFTER] || d->deps[SV_SVCDEPS_USE] ||
				d->deps[SV_SVCDEPS_NEED])
			prio = 0;
		else if (d->deps[SV_SVCDEPS_BEFORE])
			prio = 1;
	}
	pri = prio+1;

	/* expand the list when needed */
	if (pri > deptree->size && deptree->size < SV_DEPTREE_MAX)
		sv_deptree_alloc(deptree);

	if (pri < SV_DEPTREE_MAX) {
		/* handle {after,use,need} type  which insert dependencies above */
		if (type) {
			for (t = SV_SVCDEPS_AFTER; t <= SV_SVCDEPS_NEED; t++)
			TAILQ_FOREACH(ent, d->deps[t], entries) {
				FIND_SVCDEPS(ent, pri, deptree->size);
				if (add)
					sv_deptree_add(t, pri, ent, deptree);
			}
		}
		else {
			/* handle before type which incerts dependencies below */
			TAILQ_FOREACH(ent, d->deps[type], entries) {
				FIND_SVCDEPS(ent, 0, prio);
				/* issue here is to add everything nicely */
				if (add) {
					/* prio level should be precisely handled here; so, the
					 * follow up is required to get before along with the others
					 */
					r = sv_deptree_add(type, prio, ent, deptree);
					if (r < 0)
						continue;
					r = sv_deptree_add(SV_SVCDEPS_AFTER, r, ent, deptree);
					prio = ++r > prio ? r : prio;
				}
			}
		}
	}

	/* move up anything found before anything else */
	for (p = 0; p < prio; p++)
		if ((ent = sv_stringlist_find(deptree->tree[p], s))) {
			if (prio < SV_DEPTREE_MAX) {
				sv_stringlist_mov(deptree->tree[p], deptree->tree[prio], ent);
				sv_deptree_add(SV_SVCDEPS_AFTER, prio, svc, deptree);
			}
			return prio;
		}
	/* add only if necessary */
	for (p = prio; p < deptree->size; p++)
		if (sv_stringlist_find(deptree->tree[p], s))
			return p;
	prio = prio > SV_DEPTREE_MAX ? SV_DEPTREE_MAX-1 : prio;
	ent = sv_stringlist_add(deptree->tree[prio], s);
	ent->data = d;
	return prio;
#undef FIND_SVCDEPS
}

static int sv_deptree_file_load(SV_DepTree_T *deptree)
{
	int p;
	char path[256];
	char *line = NULL, *ptr, *tmp, svc[128];
	FILE *fp;
	size_t len, pos;

	snprintf(path, ARRAY_SIZE(path), "%s/%s", SV_TMPDIR_DEPS, sv_runlevel[sv_stage]);
	if (access(path, F_OK))
		return -1;
	if ((fp = fopen(path, "r+")) == NULL) {
		ERR("Failed to open %s: %s\n", path, strerror(errno));
		return -1;
	}

	while (sv_getline(fp, &line, &len) > 0) {
		ptr = strchr(line, '_');
		p = atoi(++ptr);
		ptr = strchr(line, '=');
		pos = ++ptr-line;

		/* add service to dependency appropriate priority */
		ptr = shell_string_value(ptr);
		if (ptr == NULL)
			continue;
		if (p >= deptree->size)
			while (p >= deptree->size)
				sv_deptree_alloc(deptree);

		/* append service list */
		while (*ptr) {
			if ((tmp = strchr(ptr, ' ')) == NULL)
				pos = strlen(ptr);
			else
				pos = tmp-ptr;
			memcpy(svc, ptr, pos);
			svc[pos] = '\0';
			sv_stringlist_add(deptree->tree[p], svc);
			ptr += pos+1;
		}
	}
	fclose(fp);

	return 0;
}

static int sv_deptree_file_save(SV_DepTree_T *deptree)
{
	SV_String_T *ent;
	int p;
	char path[256];
	FILE *fp;

	if (!deptree) {
		errno = ENOENT;
		return -1;
	}

	snprintf(path, ARRAY_SIZE(path), "%s/%s", SV_TMPDIR_DEPS, sv_runlevel[sv_stage]);
	if ((fp = fopen(path, "w+")) == NULL) {
		ERR("Failed to open `%s': %s\n", path, strerror(errno));
		return -1;
	}

	for (p = 0; p < deptree->size; p++) {
		fprintf(fp, "dep_%d='", p);
		TAILQ_FOREACH(ent, deptree->tree[p], entries)
			if (ent)
				fprintf(fp, "%s ", ent->str);
		fprintf(fp, "'\n");
	}
	fclose(fp);

	return 0;
}

void svc_deptree_load(SV_DepTree_T *deptree)
{
	SV_String_T *ent;
	sv_deptree_alloc(deptree);

	TAILQ_FOREACH(ent, deptree->list, entries)
		sv_deptree_add(SV_SVCDEPS_USE, -1, ent, deptree);
}

void sv_deptree_load(SV_DepTree_T *deptree)
{
	SV_String_T *ent;

	/* load previous deptree file if any, or initialize a new list */
	if (sv_deptree_file_load(deptree))
		sv_deptree_alloc(deptree);
	if (!deptree->list)
		deptree->list = sv_svclist_load(NULL);
	sv_svcdeps_load(NULL);

	/* XXX: handle {after,use,need} first */
	TAILQ_FOREACH(ent, deptree->list, entries)
		sv_deptree_add(SV_SVCDEPS_AFTER, -1, ent, deptree);
	TAILQ_FOREACH(ent, deptree->list, entries)
		sv_deptree_add(SV_SVCDEPS_BEFORE, 0, ent, deptree);

	/* save everything to a file */
	sv_deptree_file_save(deptree);
}

static void sv_runlevel_migrate(void)
{
	char op[256], np[256];
	DIR *nd, *od;
	int i, ofd, nfd;
	struct dirent *ent;

	switch (sv_stage) {
	case SV_SYSINIT_LEVEL:
		i = 0; break;
	case SV_SYSBOOT_LEVEL:
		i = 1; break;
	case SV_DEFAULT_LEVEL:
		i = 2; break;
	case SV_SHUTDOWN_LEVEL:
		i = 3; break;
	default: return; }

	snprintf(op, ARRAY_SIZE(op), "%s/.stage-%d", SV_SVCDIR, i);
	if (access(op, F_OK))
		return;
	od = opendir(op);
	snprintf(np, ARRAY_SIZE(np), "%s/.%s", SV_SVCDIR, sv_runlevel[sv_stage]);
	nd = opendir(np);
	if (!od || !nd)
		return;
	ofd = dirfd(od);
	nfd = dirfd(nd);
	if (ofd < 0 || nfd < 0)
		return;

	while ((ent = readdir(od))) {
		if (*ent->d_name == '.')
			continue;
		renameat(ofd, ent->d_name, nfd, ent->d_name);
	}
	closedir(od);
	closedir(nd);
	rmdir(op);
}

SV_StringList_T *sv_svclist_load(char *dir_path)
{
	char path[256], *ptr;
	DIR *dir;
	struct dirent *ent;
	SV_StringList_T *svclist;

	/*
	 * get the service list for this stage
	 */
	if (dir_path)
		ptr = dir_path;
	else {
		ptr = path;
		sv_runlevel_migrate();
		snprintf(path, ARRAY_SIZE(path), "%s/.%s", SV_SVCDIR, sv_runlevel[sv_stage]);
	}
	if ((dir = opendir(ptr)) == NULL) {
		ERR("Failed to open `%s' directory: %s\n", ptr, strerror(errno));
		exit(EXIT_FAILURE);
	}

	svclist = sv_stringlist_new();
	while ((ent = readdir(dir))) {
#ifdef _DIRENT_HAVE_D_TYPE
		switch (ent->d_type) {
		case DT_DIR:
		case DT_LNK:
		case DT_REG:
			break;
		default:
			continue;
			break;
		}
#endif
		if (ent->d_name[0] != '.')
			sv_stringlist_add(svclist, ent->d_name);
	}
	closedir(dir);

	return svclist;
}

static int sv_svcdeps_gen(const char *svc)
{
	int retval;
	char cmd[1024], *ptr;

	if (svc) {
		ptr = cmd;
		snprintf(cmd, ARRAY_SIZE(cmd), "%s %s", SV_DEPGEN, svc);
	}
	else
		ptr = SV_DEPGEN;

	retval = system(ptr);
	if (retval)
		ERR("Failed to execute `%s': %s\n", cmd, strerror(errno));
	return retval;
}

SV_SvcDeps_T *sv_svcdeps_load(const char *service)
{
	char cmd[128], *ptr, *svc, *type, *line = NULL;
	FILE *fp;
	size_t len, l = 0;
	int t = 0;
	SV_SvcDeps_T *deps = NULL;

	/* create a new list only when not updating the list */
	if (SERVICES.svcdeps) {
		if (service)
			deps = sv_svcdeps_find(service);
		else
			return NULL;
		if (deps)
			return deps;
		if (sv_svcdeps_gen(service))
			return NULL;
		l = strlen(service);
	}
	else
		SERVICES.svcdeps = sv_svcdeps_new();

	/* initialize SV_RUNDIR if necessary */
	if (!file_test(SV_TMPDIR_DEPS, 'd')) {
		snprintf(cmd, ARRAY_SIZE(cmd), "%s -0", SV_INIT_STAGE);
		if (system(cmd))
			WARN("Failed to execute %s: %s\n", SV_INIT_STAGE, strerror(errno));
	}

	/* get dependency list file */
	if (access(SV_SVCDEPS_FILE, F_OK))
		if (sv_svcdeps_gen(NULL))
			return NULL;
	if ((fp = fopen(SV_SVCDEPS_FILE, "r")) == NULL) {
		ERR("Failed to open %s: %s\n", SV_SVCDEPS_FILE, strerror(errno));
		return NULL;
	}

	while (sv_getline(fp, &line, &len) > 0) {
		if (service) {
			/* break the loop when updating the list */
			if (strncmp(line, service, l)) {
				if (deps) {
					free(line);
					fclose(fp);
					return deps;
				}
				/* skip lines when updating the list */
				continue;
			}
			else
				deps = sv_svcdeps_adu(svc);
		}

		/* get service name */
		svc = line;
		ptr = strchr(line, ':');
		*ptr++ = '\0';
		/* get dependency type */
		type = ptr;
		ptr = strchr(ptr, '=');
		*ptr++ = '\0';
		ptr = shell_string_value(ptr);
		if (!ptr)
			continue;

		if (!deps || strcmp(svc, deps->svc)) {
			deps = sv_svcdeps_add(svc);
			deps->virt = NULL;
		}
		if (strcmp(type, "provide") == 0) {
			if ((ptr = shell_string_value(ptr))) {
				deps->virt = err_strdup(ptr);
				sv_virtsvc_insert(deps);
			}
			continue;
		}
		else if (strcmp(type, "nohang") == 0 && sv_yesno(ptr)) {
			SV_SVCOPTS_SET(deps, SV_SVCOPTS_NOHANG);
			continue;
		}
		for (t = 0; strcmp(type, sv_svcdeps_type[t]); t++)
			;
		if (t >= SV_SVCDEPS_TYPE)
			continue;

		/* append service list */
		while (ptr && *ptr) {
			svc = ptr;
			ptr = strchr(ptr, ' ');
			if (ptr)
				*ptr++ = '\0';
			sv_stringlist_add(deps->deps[t], svc);
		}
	}
	fclose(fp);
	/* nothing found */
	if (service)
		return NULL;

	atexit(sv_svcdeps_free);
	return deps;
}

static SV_SvcDepsList_T *sv_svcdeps_new(void)
{
	SV_SvcDepsList_T *list = err_malloc(sizeof(SV_SvcDepsList_T));
	TAILQ_INIT(list);
	return list;
}

static SV_SvcDeps_T *sv_svcdeps_add(const char *svc)
{
	SV_SvcDeps_T *elm = err_malloc(sizeof(SV_SvcDeps_T));
	elm->svc = err_strdup(svc);
	elm->timeout = 0;
	elm->options = 0;

	for (int i = 0; i < SV_SVCDEPS_TYPE; i++)
		elm->deps[i] = sv_stringlist_new();
	TAILQ_INSERT_TAIL(SERVICES.svcdeps, elm, entries);

	return elm;
}

SV_SvcDeps_T *sv_svcdeps_adu(const char *svc)
{
	SV_SvcDeps_T *elm = sv_svcdeps_find(svc);
	if (elm)
		return elm;

	return sv_svcdeps_add(svc);
}

static SV_SvcDeps_T *sv_svcdeps_find(const char *svc)
{
	SV_SvcDeps_T *elm;

	TAILQ_FOREACH(elm, SERVICES.svcdeps, entries)
		if (strcmp(elm->svc, svc) == 0)
			return elm;
	return NULL;
}

SV_SvcDeps_T *sv_virtsvc_find(SV_StringList_T *svclist, const char *svc)
{
	int i;
	SV_SvcDeps_T *d = NULL;

	if (!svc || !SERVICES.virt_svcdeps)
		return NULL;

	for (i = 0; i < SERVICES.virt_count; i++) {
		if (strcmp(svc, SERVICES.virt_svcdeps[i]->virt))
			continue;
		d = SERVICES.virt_svcdeps[i];
		if (!svclist)
			return d;
		/* insert any provider included in the init-stage */
		if (sv_stringlist_find(svclist, d->svc))
			return d;
	}

	if (d)
		return d;
	return NULL;
}

static void sv_virtsvc_insert(SV_SvcDeps_T *elm)
{
	static size_t num;

	if (SERVICES.virt_count == num) {
		num += 8;
		SERVICES.virt_svcdeps = err_realloc(SERVICES.virt_svcdeps,
				num*sizeof(void*));
	}
	if (elm)
		SERVICES.virt_svcdeps[SERVICES.virt_count++] = elm;
}

static void sv_svcdeps_free(void)
{
	int i;
	SV_SvcDeps_T *elm;

	if (!SERVICES.svcdeps)
		return;

	TAILQ_FOREACH(elm, SERVICES.svcdeps, entries) {
		TAILQ_REMOVE(SERVICES.svcdeps, elm, entries);

		for (i = 0; i < SV_SVCDEPS_TYPE; i++)
			sv_stringlist_free(&(elm->deps[i]));
		free(elm->svc);
		if (elm->virt)
			free(elm->virt);
		free(elm);
	}
	free(SERVICES.svcdeps);
	free(SERVICES.virt_svcdeps);
	SERVICES.svcdeps      = NULL;
	SERVICES.virt_svcdeps = NULL;
	SERVICES.virt_count   = 0;
}
