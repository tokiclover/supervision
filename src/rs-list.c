/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 */

#include "helper.h"
#include "rs.h"
#include <dirent.h>

#define SV_DEPGEN SV_LIBDIR "/sh/dep"
#define SV_TMPDIR_DEPS SV_TMPDIR "/deps"
#define SV_INIT_STAGE SV_LIBDIR "/sh/init-stage"
#define RS_DEPTREE_PRIO 16
/* safety net for broken cyclic dependency */
#define RS_DEPTREE_MAX  1024

RS_SvcDepsList_T *service_deplist;
static RS_SvcDeps_T **virtual_deplist;
static RS_StringList_T  *stage_svclist;
static RS_StringList_T **deptree_list;
size_t rs_deptree_prio = 0;

static int  rs_deptree_file_save(void);
static void rs_svclist_load(void);
static void rs_svcdeps_free(void);
static void rs_virtual_insert(RS_SvcDeps_T *elm);
static size_t rs_virtual_count;

static void rs_deptree_alloc(void)
{
	int p;
	rs_deptree_prio += RS_DEPTREE_PRIO;
	deptree_list = err_realloc(deptree_list, rs_deptree_prio*sizeof(void*));
	for (p = rs_deptree_prio-RS_DEPTREE_PRIO; p < rs_deptree_prio; p++)
		deptree_list[p] = rs_stringlist_new();
}

void rs_deptree_free(RS_StringList_T **array)
{
	int i;
	for (i = 0; i < rs_deptree_prio; i++)
		rs_stringlist_free(array[i]);
	rs_deptree_prio = 0;
}

static int rs_deptree_add(int type, int prio, char *svc)
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
	if (type < RS_DEPS_USE && !rs_stringlist_find(stage_svclist, s))
		return -prio;
	/* insert the real service instead of a virtual one */
	if (!svc_deps && (svc_deps = rs_virtual_find(s)))
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
	if (pri > rs_deptree_prio && rs_deptree_prio < RS_DEPTREE_MAX)
		rs_deptree_alloc();

	if (svc_deps && pri < RS_DEPTREE_MAX) {
		/* handle {after,use,need} type  which insert dependencies above */
		if (type) {
			for (t = RS_DEPS_AFTER; t < RS_DEPS_TYPE; t++)
			SLIST_FOREACH(ent, svc_deps->deps[t], entries) {
				add = 1;
				for (p = pri; p < rs_deptree_prio; p++)
					if ((elm = rs_stringlist_find(deptree_list[p], ent->str))) {
						add = 0;
						break;
					}
				if (add)
					rs_deptree_add(t, pri, ent->str);
			}
		}
		else {
			/* handle before type which incerts dependencies below */
			SLIST_FOREACH(ent, svc_deps->deps[type], entries) {
				add = 1;
				for (p = 0; p < prio; p++)
					if ((elm = rs_stringlist_find(deptree_list[p], ent->str))) {
						add = 0;
						break;
					}
				/* issue here is to add everything nicely */
				if (add) {
					/* prio level should be precisely handled here; so, the
					 * follow up is required to get before along with the others
					 */
					r = rs_deptree_add(type, prio, ent->str);
					if (r < 0)
						continue;
					r = rs_deptree_add(RS_DEPS_AFTER, r, ent->str);
					r = ++r > prio ? r : prio;
					rs_deptree_add(type, pri > r ? pri : r, s);
				}
			}
		}
	}

	/* move up anything found before anything else */
	for (p = 0; p < prio; p++)
		if ((elm = rs_stringlist_find(deptree_list[p], s))) {
			if (prio < RS_DEPTREE_MAX) {
				rs_stringlist_mov(deptree_list[p], deptree_list[prio], elm);
				rs_deptree_add(RS_DEPS_AFTER, prio, s);
			}
			return prio;
		}
	/* add only if necessary */
	for (p = prio; p < rs_deptree_prio; p++)
		if (rs_stringlist_find(deptree_list[p], s))
			return p;
	prio = prio > RS_DEPTREE_MAX ? RS_DEPTREE_MAX-1 : prio;
	rs_stringlist_add(deptree_list[prio], s);
	return prio;
}

static RS_StringList_T **rs_deptree_file_load(void)
{
	int p;
	char deppath[256];
	char *line = NULL, *ptr, *tmp, svc[128];
	FILE *depfile;
	size_t len, pos;

	snprintf(deppath, sizeof(deppath), "%s/%d_deptree", SV_TMPDIR_DEPS, rs_stage);
	if (file_test(deppath, 0) == 0)
		return (RS_StringList_T **)0;
	if ((depfile = fopen(deppath, "r+")) == NULL) {
		ERR("Failed to open %s\n", deppath);
		return (RS_StringList_T **)0;
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
		if (p >= rs_deptree_prio)
			rs_deptree_alloc();

		/* append service list */
		while (*ptr) {
			if ((tmp = strchr(ptr, ' ')) == NULL)
				pos = strlen(ptr);
			else
				pos = tmp-ptr;
			memcpy(svc, ptr, pos);
			svc[pos] = '\0';
			rs_stringlist_add(deptree_list[p], svc);
			ptr += pos+1;
		}
	}
	fclose(depfile);

	return deptree_list;
}

static int rs_deptree_file_save(void)
{
	RS_String_T *ent;
	int p;
	char deppath[256];
	FILE *depfile;

	if (!deptree_list) {
		errno = ENOENT;
		return -1;
	}

	snprintf(deppath, sizeof(deppath), "%s/%d_deptree", SV_TMPDIR_DEPS,	rs_stage);
	if ((depfile = fopen(deppath, "w+")) == NULL) {
		ERR("Failed to open `%s': %s\n", deppath, strerror(errno));
		return -1;
	}

	for (p = 0; p < rs_deptree_prio; p++) {
		fprintf(depfile, "dep_%d='", p);
		SLIST_FOREACH(ent, deptree_list[p], entries)
			if (ent)
				fprintf(depfile, "%s ", ent->str);
		fprintf(depfile, "'\n");
	}
	fclose(depfile);

	return 0;
}

RS_StringList_T **rs_deptree_load(void)
{
	RS_String_T *ent;

	/* load previous deptree file if any, or initialize a new list */
	if (!rs_deptree_file_load())
		rs_deptree_alloc();
	rs_svclist_load();
	rs_svcdeps_load();

	/* XXX: handle {after,use,need} first */
	SLIST_FOREACH(ent, stage_svclist, entries)
		rs_deptree_add(RS_DEPS_AFTER, -1, ent->str);
	SLIST_FOREACH(ent, stage_svclist, entries)
		rs_deptree_add(RS_DEPS_BEFORE, 0, ent->str);

	/* save everything to a file */
	rs_deptree_file_save();

	/* clean unnecessary list */
	rs_stringlist_free(stage_svclist);

	return deptree_list;
}

static void rs_svclist_load(void)
{
	char path[256];
	DIR *dir;
	struct dirent *ent;

	/*
	 * get the service list for this stage
	 */
	snprintf(path, ARRAY_SIZE(path), "%s/.stage-%d", SV_SVCDIR, rs_stage);
	if ((dir = opendir(path)) == NULL) {
		ERR("Failed to open `%s' directory: %s\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	stage_svclist = rs_stringlist_new();
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] != '.')
			rs_stringlist_add(stage_svclist, ent->d_name);
	}
	closedir(dir);
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
	if (file_test(deppath, 0) <= 0) {
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

RS_StringList_T *rs_stringlist_new(void)
{
	RS_StringList_T *list = err_malloc(sizeof(*list));
	SLIST_INIT(list);
	return list;
}

RS_String_T *rs_stringlist_add(RS_StringList_T *list, const char *str)
{
	RS_String_T *elm = err_malloc(sizeof(RS_String_T));
	elm->str = err_strdup(str);
	SLIST_INSERT_HEAD(list, elm, entries);
	return elm;
}

RS_String_T *rs_stringlist_adu(RS_StringList_T *list, const char *str)
{
	RS_String_T *elm = rs_stringlist_find(list, str);
	if (elm)
		return elm;

	return rs_stringlist_add(list, str);
}

int rs_stringlist_del(RS_StringList_T *list, const char *str)
{
	RS_String_T *elm = rs_stringlist_find(list, str);

	if (elm) {
		SLIST_REMOVE(list, elm, RS_String, entries);
		free(elm->str);
		free(elm);
		return 0;
	}
	errno = EINVAL;
	return -1;
}

int rs_stringlist_rem(RS_StringList_T *list, RS_String_T *elm)
{
	if (elm) {
		SLIST_REMOVE(list, elm, RS_String, entries);
		free(elm->str);
		free(elm);
		return 0;
	}
	errno = EINVAL;
	return -1;
}

RS_String_T *rs_stringlist_find(RS_StringList_T *list, const char *str)
{
	RS_String_T *elm;

	if (list)
		SLIST_FOREACH(elm, list, entries)
			if (strcmp(elm->str, str) == 0)
				return elm;
	return NULL;
}

int rs_stringlist_mov(RS_StringList_T *src, RS_StringList_T *dst, RS_String_T *ent)
{
	if (src == NULL || dst == NULL) {
		errno = EINVAL;
		return -1;
	}
	SLIST_REMOVE(src, ent, RS_String, entries);
	SLIST_INSERT_HEAD(dst, ent, entries);
	return 0;
}

void rs_stringlist_free(RS_StringList_T *list)
{
	RS_String_T *elm;

	if (list)
		while (!SLIST_EMPTY(list)) {
			elm = SLIST_FIRST(list);
			free(elm->str);
			SLIST_REMOVE_HEAD(list, entries);
			free(elm);
		}
	list = NULL;
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

RS_SvcDeps_T *rs_virtual_find(const char *svc)
{
	int i;
	RS_SvcDeps_T *d = NULL;

	if (!svc || !virtual_deplist)
		return NULL;

	for (i = 0; i < rs_virtual_count; i++) {
		if (strcmp(svc, virtual_deplist[i]->virt))
			continue;
		d = virtual_deplist[i];
		if (!stage_svclist)
			return d;
		/* insert any provider included in the init-stage */
		if (rs_stringlist_find(stage_svclist, d->svc))
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
			rs_stringlist_free(elm->deps[i]);

		free(elm->svc);
		if (elm->virt)
			free(elm->virt);
		SLIST_REMOVE_HEAD(service_deplist, entries);
		free(elm);
	}
	service_deplist = NULL;
}

