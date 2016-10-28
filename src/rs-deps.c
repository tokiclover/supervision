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

RS_SvcDepsList_T *service_deplist;
RS_SvcDeps_T **virtual_deplist;

static int  rs_deptree_file_load(const char *head, RS_DepTree_T *deptree);
static int  rs_deptree_file_save(const char *head, RS_DepTree_T *deptree);
static void rs_svcdeps_free(void);
static void rs_virtual_insert(RS_SvcDeps_T *elm);
size_t rs_virtual_count;

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
	RS_SvcDeps_T *svc_deps = rs_svcdeps_find(service_deplist, s);
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
	if (!svc_deps && (svc_deps = rs_virtual_find(s, deptree->list)))
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

static int rs_deptree_file_load(const char *head, RS_DepTree_T *deptree)
{
	int p;
	char deppath[256];
	char *line = NULL, *ptr, *tmp, svc[128];
	FILE *depfile;
	size_t len, pos;

	snprintf(deppath, sizeof(deppath), "%s/%d_deptree", head, rs_stage);
	if (!access(deppath, F_OK) == 0)
		return -1;
	if ((depfile = fopen(deppath, "r+")) == NULL) {
		ERR("Failed to open %s\n", deppath);
		return -1;
	}

	while (rs_getline(depfile, &line, &len) > 0) {
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
	fclose(depfile);

	return 0;
}

static int rs_deptree_file_save(const char *head, RS_DepTree_T *deptree)
{
	RS_String_T *ent;
	int p;
	char deppath[256];
	FILE *depfile;

	if (!deptree) {
		errno = ENOENT;
		return -1;
	}

	snprintf(deppath, sizeof(deppath), "%s/%d_deptree", head, rs_stage);
	if ((depfile = fopen(deppath, "w+")) == NULL) {
		ERR("Failed to open `%s': %s\n", deppath, strerror(errno));
		return -1;
	}

	for (p = 0; p < deptree->size; p++) {
		fprintf(depfile, "dep_%d='", p);
		SLIST_FOREACH(ent, deptree->tree[p], entries)
			if (ent)
				fprintf(depfile, "%s ", ent->str);
		fprintf(depfile, "'\n");
	}
	fclose(depfile);

	return 0;
}

void svc_deptree_load(RS_DepTree_T *deptree)
{
	RS_String_T *ent;

	deptree->tree = err_realloc(deptree->tree, deptree->size*sizeof(void*));
	rs_svcdeps_load();

	SLIST_FOREACH(ent, deptree->list, entries)
		rs_deptree_add(RS_DEPS_USE, -1, ent->str, deptree);
}

void rs_deptree_load(RS_DepTree_T *deptree)
{
	RS_String_T *ent;

	/* load previous deptree file if any, or initialize a new list */
	if (rs_deptree_file_load(SV_TMPDIR_DEPS, deptree))
		rs_deptree_alloc(deptree);
	if (!deptree->list)
		deptree->list = rs_svclist_load(NULL);
	rs_svcdeps_load();

	/* XXX: handle {after,use,need} first */
	SLIST_FOREACH(ent, deptree->list, entries)
		rs_deptree_add(RS_DEPS_AFTER, -1, ent->str, deptree);
	SLIST_FOREACH(ent, deptree->list, entries)
		rs_deptree_add(RS_DEPS_BEFORE, 0, ent->str, deptree);

	/* save everything to a file */
	rs_deptree_file_save(SV_TMPDIR_DEPS, deptree);

	/* clean unnecessary list */
	rs_stringlist_free(&deptree->list);
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
		if (ent->d_name[0] != '.')
			rs_stringlist_add(svclist, ent->d_name);
	}
	closedir(dir);

	return svclist;
}

void rs_svcdeps_load(void)
{
	char deppath[256], dep[128], type[16];
	char *line = NULL, *ptr, *tmp, svc[128];
	FILE *depfile;
	size_t len, pos;
	int t = 0;

	if (service_deplist)
		return;

	/* initialize SV_RUNDIR if necessary */
	if (!file_test(SV_TMPDIR_DEPS, 'd')) {
		snprintf(deppath, ARRAY_SIZE(deppath), "%s -0", SV_INIT_STAGE);
		if (system(deppath))
			WARN("Failed to execute %s\n", SV_INIT_STAGE);
	}

	/* get dependency list file */
	snprintf(deppath, ARRAY_SIZE(deppath), "%s/svcdeps", SV_TMPDIR_DEPS);
	if (!access(deppath, F_OK) <= 0) {
		if (system(SV_DEPGEN)) {
			ERR("Failed to execute `%s': %s\n", SV_DEPGEN, strerror(errno));
			return;
		}
	}

	if ((depfile = fopen(deppath, "r")) == NULL) {
		ERR("Failed to open %s: %s\n", deppath, strerror(errno));
		return;
	}

	service_deplist = rs_svcdeps_new();
	RS_SvcDeps_T *svc_deps = NULL;

	while (rs_getline(depfile, &line, &len) > 0) {
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
			svc_deps = rs_svcdeps_add(service_deplist, svc);
			svc_deps->virt = NULL;
		}
		if (strcmp(type, "provide") == 0) {
			if ((ptr = shell_string_value(ptr))) {
				svc_deps->virt = err_strdup(ptr);
				rs_virtual_insert(svc_deps);
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
	fclose(depfile);
	atexit(rs_svcdeps_free);
}

RS_SvcDepsList_T *rs_svcdeps_new(void)
{
	RS_SvcDepsList_T *list = err_malloc(sizeof(RS_SvcDepsList_T));
	SLIST_INIT(list);
	return list;
}

RS_SvcDeps_T *rs_svcdeps_add(RS_SvcDepsList_T *list, const char *svc)
{
	RS_SvcDeps_T *elm = err_malloc(sizeof(RS_SvcDeps_T));
	elm->svc = err_strdup(svc);

	for (int i = 0; i < RS_DEPS_TYPE; i++)
		elm->deps[i] = rs_stringlist_new();
	SLIST_INSERT_HEAD(list, elm, entries);

	return elm;
}

RS_SvcDeps_T *rs_svcdeps_adu(RS_SvcDepsList_T *list, const char *svc)
{
	RS_SvcDeps_T *elm = rs_svcdeps_find(list, svc);
	if (elm)
		return elm;

	return rs_svcdeps_add(list, svc);
}

RS_SvcDeps_T *rs_svcdeps_find(RS_SvcDepsList_T *list, const char *svc)
{
	RS_SvcDeps_T *elm;

	if (list)
		SLIST_FOREACH(elm, list, entries)
			if (strcmp(elm->svc, svc) == 0)
				return elm;
	return NULL;
}

RS_SvcDeps_T *rs_virtual_find(const char *svc, RS_StringList_T *svclist)
{
	int i;
	RS_SvcDeps_T *d = NULL;

	if (!svc || !virtual_deplist)
		return NULL;

	for (i = 0; i < rs_virtual_count; i++) {
		if (strcmp(svc, virtual_deplist[i]->virt))
			continue;
		d = virtual_deplist[i];
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

static void rs_virtual_insert(RS_SvcDeps_T *elm)
{
	static size_t num;

	if (rs_virtual_count == num) {
		num += 8;
		virtual_deplist = err_realloc(virtual_deplist, num*sizeof(void*));
	}
	if (elm)
		virtual_deplist[rs_virtual_count++] = elm;
}

static void rs_svcdeps_free(void)
{
	int i;
	RS_SvcDeps_T *elm;

	if (!service_deplist)
		return;

	while (!SLIST_EMPTY(service_deplist)) {
		elm = SLIST_FIRST(service_deplist);
		for (i = 0; i < RS_DEPS_TYPE; i++)
			rs_stringlist_free(&(elm->deps[i]));

		free(elm->svc);
		if (elm->virt)
			free(elm->virt);
		SLIST_REMOVE_HEAD(service_deplist, entries);
		free(elm);
	}
	service_deplist = NULL;
}

