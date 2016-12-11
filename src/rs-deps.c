/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs-deps.c
 */

#include "rs-deps.h"
#include <dirent.h>

static const char *const rs_deps_type[] = { "before", "after", "use", "need" };

struct RS_Services SERVICES = {
	.svclist      = NULL,
	.svcdeps      = NULL,
	.virt_svcdeps = NULL,
	.virt_count   = 0,
};

static int  rs_deptree_file_load(RS_DepTree_T *deptree);
static int  rs_deptree_file_save(RS_DepTree_T *deptree);
static void rs_svcdeps_free(void);
static RS_SvcDepsList_T *rs_svcdeps_new(void);
static RS_SvcDeps_T *rs_svcdeps_add (const char *svc);
static RS_SvcDeps_T *rs_svcdeps_find(const char *svc);
/* load generate service dependency */
static int           rs_svcdeps_gen(const char *svc);
static void rs_virtsvc_insert(RS_SvcDeps_T *elm);

static void rs_deptree_alloc(RS_DepTree_T *deptree)
{
	int p;

	deptree->size += RS_DEPTREE_PRIO;
	deptree->tree = err_realloc(deptree->tree, deptree->size*sizeof(void*));
	for (p = deptree->size-RS_DEPTREE_PRIO; p < deptree->size; p++)
		deptree->tree[p] = rs_stringlist_new();
}

void rs_deptree_free(RS_DepTree_T *deptree)
{
	int i;
	for (i = 0; i < deptree->size; i++)
		rs_stringlist_free(&deptree->tree[i]);
	deptree->size = 0;
}

static int rs_deptree_add(int type, int prio, char *svc, RS_DepTree_T *deptree)
{
	char *s = svc;
	RS_SvcDeps_T *svc_deps = rs_svcdeps_find(s);
	RS_String_T *ent, *elm;
	int add, pri;
	int p, t, r;

	if (s == NULL)
		return 0;
	/* add service to list if and only if, either a service is {use,need}ed or
	 * belongs to this particular init-stage
	 */
	if (type < RS_DEPS_USE && !rs_stringlist_find(deptree->list, s))
		return -prio;
	/* insert the real service instead of a virtual one */
	if (!svc_deps && (svc_deps = rs_virtsvc_find(deptree->list, s)))
		s = svc_deps->svc;
	if (prio < 0) {
		if (svc_deps) {
			if (svc_deps->deps[RS_DEPS_AFTER] || svc_deps->deps[RS_DEPS_USE] ||
					svc_deps->deps[RS_DEPS_NEED])
				prio = 0;
			else if (svc_deps->deps[RS_DEPS_BEFORE])
				prio = 1;
		}
		else
			return -1;
	}
	pri = prio+1;

	/* expand the list when needed */
	if (pri > deptree->size && deptree->size < RS_DEPTREE_MAX)
		rs_deptree_alloc(deptree);

	if (svc_deps && pri < RS_DEPTREE_MAX) {
		/* handle {after,use,need} type  which insert dependencies above */
		if (type) {
			for (t = RS_DEPS_AFTER; t < RS_DEPS_TYPE; t++)
			SLIST_FOREACH(ent, svc_deps->deps[t], entries) {
				add = 1;
				for (p = pri; p < deptree->size; p++)
					if ((elm = rs_stringlist_find(deptree->tree[p], ent->str))) {
						add = 0;
						break;
					}
				if (add)
					rs_deptree_add(t, pri, ent->str, deptree);
			}
		}
		else {
			/* handle before type which incerts dependencies below */
			SLIST_FOREACH(ent, svc_deps->deps[type], entries) {
				add = 1;
				for (p = 0; p < prio; p++)
					if ((elm = rs_stringlist_find(deptree->tree[p], ent->str))) {
						add = 0;
						break;
					}
				/* issue here is to add everything nicely */
				if (add) {
					/* prio level should be precisely handled here; so, the
					 * follow up is required to get before along with the others
					 */
					r = rs_deptree_add(type, prio, ent->str, deptree);
					if (r < 0)
						continue;
					r = rs_deptree_add(RS_DEPS_AFTER, r, ent->str, deptree);
					prio = ++r > prio ? r : prio;
				}
			}
		}
	}

	/* move up anything found before anything else */
	for (p = 0; p < prio; p++)
		if ((elm = rs_stringlist_find(deptree->tree[p], s))) {
			if (prio < RS_DEPTREE_MAX) {
				rs_stringlist_mov(deptree->tree[p], deptree->tree[prio], elm);
				rs_deptree_add(RS_DEPS_AFTER, prio, s, deptree);
			}
			return prio;
		}
	/* add only if necessary */
	for (p = prio; p < deptree->size; p++)
		if (rs_stringlist_find(deptree->tree[p], s))
			return p;
	prio = prio > RS_DEPTREE_MAX ? RS_DEPTREE_MAX-1 : prio;
	rs_stringlist_add(deptree->tree[prio], s);
	return prio;
}

static int rs_deptree_file_load(RS_DepTree_T *deptree)
{
	int p;
	char path[256];
	char *line = NULL, *ptr, *tmp, svc[128];
	FILE *fp;
	size_t len, pos;

	snprintf(path, ARRAY_SIZE(path), "%s/%d_deptree", SV_TMPDIR_DEPS, rs_stage);
	if (access(path, F_OK))
		return -1;
	if ((fp = fopen(path, "r+")) == NULL) {
		ERR("Failed to open %s: %s\n", path, strerror(errno));
		return -1;
	}

	while (rs_getline(fp, &line, &len) > 0) {
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
				rs_deptree_alloc(deptree);

		/* append service list */
		while (*ptr) {
			if ((tmp = strchr(ptr, ' ')) == NULL)
				pos = strlen(ptr);
			else
				pos = tmp-ptr;
			memcpy(svc, ptr, pos);
			svc[pos] = '\0';
			rs_stringlist_add(deptree->tree[p], svc);
			ptr += pos+1;
		}
	}
	fclose(fp);

	return 0;
}

static int rs_deptree_file_save(RS_DepTree_T *deptree)
{
	RS_String_T *ent;
	int p;
	char path[256];
	FILE *fp;

	if (!deptree) {
		errno = ENOENT;
		return -1;
	}

	snprintf(path, ARRAY_SIZE(path), "%s/%d_deptree", SV_TMPDIR_DEPS, rs_stage);
	if ((fp = fopen(path, "w+")) == NULL) {
		ERR("Failed to open `%s': %s\n", path, strerror(errno));
		return -1;
	}

	for (p = 0; p < deptree->size; p++) {
		fprintf(fp, "dep_%d='", p);
		SLIST_FOREACH(ent, deptree->tree[p], entries)
			if (ent)
				fprintf(fp, "%s ", ent->str);
		fprintf(fp, "'\n");
	}
	fclose(fp);

	return 0;
}

void svc_deptree_load(RS_DepTree_T *deptree)
{
	RS_String_T *ent;
	rs_deptree_alloc(deptree);

	SLIST_FOREACH(ent, deptree->list, entries)
		rs_deptree_add(RS_DEPS_USE, -1, ent->str, deptree);
}

RS_SvcDeps_T *svc_deps_find(const char *svc)
{
	if (!rs_stringlist_find(SERVICES.svclist, svc))
		return rs_svcdeps_load(svc);
	return rs_svcdeps_find(svc);
}

void rs_deptree_load(RS_DepTree_T *deptree)
{
	RS_String_T *ent;

	/* load previous deptree file if any, or initialize a new list */
	if (rs_deptree_file_load(deptree))
		rs_deptree_alloc(deptree);
	if (!deptree->list)
		deptree->list = rs_svclist_load(NULL);
	rs_svcdeps_load(NULL);

	/* XXX: handle {after,use,need} first */
	SLIST_FOREACH(ent, deptree->list, entries)
		rs_deptree_add(RS_DEPS_AFTER, -1, ent->str, deptree);
	SLIST_FOREACH(ent, deptree->list, entries)
		rs_deptree_add(RS_DEPS_BEFORE, 0, ent->str, deptree);

	/* save everything to a file */
	rs_deptree_file_save(deptree);
}

RS_StringList_T *rs_svclist_load(char *dir_path)
{
	char path[256], *ptr;
	DIR *dir;
	struct dirent *ent;
	RS_StringList_T *svclist;

	/*
	 * get the service list for this stage
	 */
	if (dir_path)
		ptr = dir_path;
	else {
		ptr = path;
		snprintf(path, ARRAY_SIZE(path), "%s/.stage-%d", SV_SVCDIR, rs_stage);
	}
	if ((dir = opendir(ptr)) == NULL) {
		ERR("Failed to open `%s' directory: %s\n", ptr, strerror(errno));
		exit(EXIT_FAILURE);
	}

	svclist = rs_stringlist_new();
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
			rs_stringlist_add(svclist, ent->d_name);
	}
	closedir(dir);

	return svclist;
}

static int rs_svcdeps_gen(const char *svc)
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

RS_SvcDeps_T *rs_svcdeps_load(const char *service)
{
	char dep[128], type[16];
	char *line = NULL, *ptr, *tmp, svc[128];
	FILE *fp;
	size_t len, pos, l;
	int t = 0;

	if (SERVICES.svcdeps && !service)
		return NULL;
	if (service) {
		if (rs_svcdeps_gen(service))
			return NULL;
		l = strlen(service);
	}

	/* initialize SV_RUNDIR if necessary */
	if (!file_test(SV_TMPDIR_DEPS, 'd')) {
		snprintf(dep, ARRAY_SIZE(dep), "%s -0", SV_INIT_STAGE);
		if (system(dep))
			WARN("Failed to execute %s: %s\n", SV_INIT_STAGE, strerror(errno));
	}

	/* get dependency list file */
	if (access(RS_SVCDEPS_FILE, F_OK))
		if (rs_svcdeps_gen(NULL))
			return NULL;
	if ((fp = fopen(RS_SVCDEPS_FILE, "r")) == NULL) {
		ERR("Failed to open %s: %s\n", RS_SVCDEPS_FILE, strerror(errno));
		return NULL;
	}

	/* create a new list only when not updating the list */
	if (!SERVICES.svcdeps)
		SERVICES.svcdeps = rs_svcdeps_new();
	RS_SvcDeps_T *svc_deps = NULL;

	while (rs_getline(fp, &line, &len) > 0) {
		/* break the loop when updating the list */
		if (service && strncmp(line, service, l)) {
			if (svc_deps) {
				free(line);
				fclose(fp);
				return svc_deps;
			}
			/* skip lines when updating the list */
			continue;
		}

		/* get service name */
		ptr = strchr(line, ':');
		*ptr++ = '\0';
		pos = ptr-line;
		memcpy(svc, line, pos);

		/* get dependency type/name */
		ptr = strchr(ptr, '=');
		*ptr++ = '\0';
		memcpy(type, line+pos, ptr-line-pos);

		if (!svc_deps || strcmp(svc, svc_deps->svc)) {
			svc_deps = rs_svcdeps_add(svc);
			svc_deps->virt = NULL;
		}
		if (strcmp(type, "provide") == 0) {
			if ((ptr = shell_string_value(ptr))) {
				svc_deps->virt = err_strdup(ptr);
				rs_virtsvc_insert(svc_deps);
			}
			continue;
		}
		for (t = 0; strcmp(type, rs_deps_type[t]); t++)
			;

		/* append service list */
		ptr = shell_string_value(ptr);
		while (*ptr) {
			if ((tmp = strchr(ptr, ' ')) == NULL)
				pos = strlen(ptr);
			else
				pos = tmp-ptr;
			memcpy(dep, ptr, pos);
			dep[pos] = '\0';
			rs_stringlist_add(svc_deps->deps[t], dep);
			ptr += pos+1;
		}
	}
	fclose(fp);
	/* nothing found */
	if (service)
		return NULL;

	/* get service list file */
	if (access(RS_SVCLIST_FILE, F_OK))
		if (rs_svcdeps_gen(NULL))
			return NULL;
	if ((fp = fopen(RS_SVCLIST_FILE, "r")) == NULL) {
		ERR("Failed to open %s: %s\n", RS_SVCLIST_FILE, strerror(errno));
		return NULL;
	}

	SERVICES.svclist = rs_stringlist_new();
	while (rs_getline(fp, &line, &len) > 0) {
		ptr = line;
		pos = 0;
		while (*ptr) {
			if ((tmp = strchr(ptr, ' ')) == NULL)
				pos = strlen(ptr);
			else
				pos = tmp-ptr;
			memcpy(svc, ptr, pos);
			svc[pos] = '\0';
			rs_stringlist_add(SERVICES.svclist, svc);
			ptr += pos+1;
		}
	}
	fclose(fp);

	atexit(rs_svcdeps_free);
	return svc_deps;
}

static RS_SvcDepsList_T *rs_svcdeps_new(void)
{
	RS_SvcDepsList_T *list = err_malloc(sizeof(RS_SvcDepsList_T));
	SLIST_INIT(list);
	return list;
}

static RS_SvcDeps_T *rs_svcdeps_add(const char *svc)
{
	RS_SvcDeps_T *elm = err_malloc(sizeof(RS_SvcDeps_T));
	elm->svc = err_strdup(svc);

	for (int i = 0; i < RS_DEPS_TYPE; i++)
		elm->deps[i] = rs_stringlist_new();
	SLIST_INSERT_HEAD(SERVICES.svcdeps, elm, entries);

	return elm;
}

static RS_SvcDeps_T *rs_svcdeps_find(const char *svc)
{
	RS_SvcDeps_T *elm;

	SLIST_FOREACH(elm, SERVICES.svcdeps, entries)
		if (strcmp(elm->svc, svc) == 0)
			return elm;
	return NULL;
}

RS_SvcDeps_T *rs_virtsvc_find(RS_StringList_T *svclist, const char *svc)
{
	int i;
	RS_SvcDeps_T *d = NULL;

	if (!svc || !SERVICES.virt_svcdeps)
		return NULL;

	for (i = 0; i < SERVICES.virt_count; i++) {
		if (strcmp(svc, SERVICES.virt_svcdeps[i]->virt))
			continue;
		d = SERVICES.virt_svcdeps[i];
		if (!svclist)
			return d;
		/* insert any provider included in the init-stage */
		if (rs_stringlist_find(svclist, d->svc))
			return d;
	}

	if (d)
		return d;
	return NULL;
}

static void rs_virtsvc_insert(RS_SvcDeps_T *elm)
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

static void rs_svcdeps_free(void)
{
	int i;
	RS_SvcDeps_T *elm;

	if (!SERVICES.svcdeps)
		return;

	while (!SLIST_EMPTY(SERVICES.svcdeps)) {
		elm = SLIST_FIRST(SERVICES.svcdeps);
		for (i = 0; i < RS_DEPS_TYPE; i++)
			rs_stringlist_free(&(elm->deps[i]));

		free(elm->svc);
		if (elm->virt)
			free(elm->virt);
		SLIST_REMOVE_HEAD(SERVICES.svcdeps, entries);
		free(elm);
	}
	rs_stringlist_free(&SERVICES.svclist);
	free(SERVICES.virt_svcdeps);
	SERVICES.svcdeps      = NULL;
	SERVICES.virt_svcdeps = NULL;
	SERVICES.virt_count   = 0;
}

