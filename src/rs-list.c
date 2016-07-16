/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision (Scripts Framework).
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 */

#include "helper.h"
#include "rs.h"

#define SV_DEPGEN SV_LIBDIR "/sh/dep"
#define SV_DEPDIR SV_TMPDIR "/deps"

static RS_SvcDepsList_T *service_deplist;
static RS_DepTypeList_T *stage_deplist;
static RS_StringList_T *deptree_list[RS_DEPTREE_PRIO];

void rs_deptree_free(RS_StringList_T **array)
{
	int i;
	for (i = 0; i < RS_DEPTREE_PRIO; i++)
		rs_stringlist_free(array[i]);
}

int rs_deptree_add(int type, int prio, char *svc)
{
	RS_SvcDeps_T *svc_deps = rs_svcdeps_find(service_deplist, svc);
	RS_String_T *ent, *elm;
	int add, pri = prio+1;
	int p, t, r;
	static int lim = RS_DEPTREE_PRIO-1;

	if (svc == NULL)
		return 0;

	if (pri < RS_DEPTREE_PRIO && svc_deps) {
		/* handle {after,use,need} type  which insert dependencies above */
		if (type) {
			for (t = RS_DEPS_AFTER; t < RS_DEPS_TYPE; t++)
			SLIST_FOREACH(ent, svc_deps->deps[t], entries) {
				add = 1;
				for (p = pri; p <= lim; p++)
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
					r = rs_deptree_add(type, prio > 2 ? prio : 2, ent->str);
					rs_deptree_add(RS_DEPS_AFTER, r, ent->str);
					r = ++r > prio ? r : prio;
					rs_deptree_add(type, pri > r ? pri : r, svc);
				}
			}
		}
	}

	/* move up anything found before anything else */
	for (p = 0; p < prio; p++)
		if ((elm = rs_stringlist_find(deptree_list[p], svc))) {
			if (prio < RS_DEPTREE_PRIO) {
				rs_stringlist_mov(deptree_list[p], deptree_list[prio], elm);
				if (type && prio < lim)
					rs_deptree_add(type, prio, svc);
			}
			return prio;
		}
	/* add only if necessary */
	for (p = prio; p < RS_DEPTREE_PRIO; p++)
		if (rs_stringlist_find(deptree_list[p], svc))
			return p;
	prio = prio > lim ? lim : prio;
	rs_stringlist_add(deptree_list[prio], svc);
	return prio;
}

RS_StringList_T **rs_deptree_file_load(void)
{
	int p;
	char deppath[256];
	char *line = NULL, *ptr, *tmp, svc[128];
	FILE *depfile;
	size_t len, pos;

	snprintf(deppath, sizeof(deppath), "%s/stage-%d/deptree_%s", SV_DEPDIR,
			RS_STAGE.level, RS_STAGE.type);
	if (file_test(deppath, 0) == 0)
		return (RS_StringList_T **)0;
	if ((depfile = fopen(deppath, "r+")) == NULL) {
		ERR("Failed to open %s\n", deppath);
		return (RS_StringList_T **)0;
	}

	/* initialize the list */
	for (p = 0; p < RS_DEPTREE_PRIO; p++)
		deptree_list[p] = rs_stringlist_new();

	while (rs_getline(depfile, &line, &len) > 0) {
		/* get dependency type */
		ptr = strchr(line, '_');
		p = atoi(++ptr);
		ptr = strchr(line, '=');
		pos = ++ptr-line;

		/* add service to dependency appropriate priority */
		ptr = shell_string_value(ptr);
		if (ptr == NULL)
			continue;

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

int rs_deptree_file_save(RS_StringList_T *deptree[])
{
	RS_String_T *ent;
	int p;
	char deppath[256];
	FILE *depfile;

	if (deptree == NULL)
		deptree = deptree_list;
	if (deptree == NULL) {
		errno = ENOENT;
		return -1;
	}

	snprintf(deppath, sizeof(deppath), "%s/stage-%d/deptree_%s", SV_DEPDIR,
			RS_STAGE.level, RS_STAGE.type);
	if ((depfile = fopen(deppath, "wa+")) == NULL) {
		ERR("Failed to open %s\n", deppath);
		return -1;
	}

	for (p = 0; p < RS_DEPTREE_PRIO; p++) {
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
	int p, t;
	RS_DepType_T *deptype[RS_DEPS_TYPE];
	RS_String_T *ent, *elm;
	RS_StringList_T **ptr;

	/* try to load the file if any */
	if ((ptr = rs_deptree_file_load())) {
		return ptr;
	}
	stage_deplist = rs_deplist_load();
	service_deplist = rs_svcdeps_load();

	for (t = 0; t < RS_DEPS_TYPE; t++)
		deptype[t] = rs_deplist_find(stage_deplist, rs_deps_type[t]);
	for (p = 0; p < RS_DEPTREE_PRIO; p++)
		deptree_list[p] = rs_stringlist_new();

	/* handle high priority first to be sure to satisfy lower prio services */
	for (t = 0; t < RS_DEPS_TYPE; t++)
		for (p = RS_DEPS_PRIO-1; p > 0; p--)
			SLIST_FOREACH(ent, deptype[t]->priority[p], entries) {
				if ((elm = rs_stringlist_find(deptype[t]->priority[p-1], ent->str)))
					rs_stringlist_rem(deptype[t]->priority[p-1], elm);
				rs_deptree_add(t, p, ent->str);
			}

	/* merge remaining lower priority services */
	for (t = 0; t < RS_DEPS_TYPE; t++)
		for (p = 0; p < RS_DEPS_PRIO; p++)
			SLIST_FOREACH(ent, deptype[t]->priority[p], entries)
				rs_deptree_add(t, p, ent->str);

	/* save everything to a file */
	rs_deptree_file_save(deptree_list);

	/* clean unnecessary list */
	rs_deplist_free(stage_deplist);
	/* this is now used in svc_stage()
	rs_svcdeps_free(service_deplist);
	*/

	return deptree_list;
}

RS_DepTypeList_T *rs_deplist_load(void)
{
	char depcmd[256], deppath[256], dep[16];
	char *line = NULL, *ptr, *tmp, svc[128];
	FILE *depfile;
	size_t len, pos;
	int pri;

	/* get dependency list file */
	snprintf(deppath, ARRAY_SIZE(deppath), "%s/stage-%d/%s", SV_DEPDIR,
			RS_STAGE.level, RS_STAGE.type);
	if (RS_STAGE.level == 2 || file_test(deppath, 0) <= 0) {
		snprintf(depcmd, ARRAY_SIZE(depcmd), "%s -%d --%s", SV_DEPGEN,
				RS_STAGE.level, RS_STAGE.type);
		if (system(depcmd)) {
			ERR("Failed to execute `%s'\n", depcmd);
			exit(EXIT_FAILURE);
		}
	}

	if ((depfile = fopen(deppath, "r")) == NULL) {
		ERR("Failed to open %s\n", deppath);
		exit(EXIT_FAILURE);
	}

	RS_DepTypeList_T *deplist = rs_deplist_new();
	RS_DepType_T *dlp;

	while (rs_getline(depfile, &line, &len) > 0) {
		/* get dependency type */
		ptr = strchr(line, '_');
		pos = ptr-line;
		pri = atoi(++ptr);
		memcpy(dep, line, pos);
		dep[pos] = '\0';
		ptr += 2;

		/* add and initialize a dependency type list */
		if (pri == 0)
			dlp = rs_deplist_add(deplist, dep);
		/* add service to dependency appropriate priority */
		ptr = shell_string_value(ptr);
		if (ptr == NULL)
			continue;

		/* append service list */
		while (*ptr) {
			if ((tmp = strchr(ptr, ' ')) == NULL)
				pos = strlen(ptr);
			else
				pos = tmp-ptr;
			memcpy(svc, ptr, pos);
			svc[pos] = '\0';
			rs_stringlist_add(dlp->priority[pri], svc);
			ptr += pos+1;
		}
	}
	fclose(depfile);

	return deplist;
}

RS_SvcDepsList_T *rs_svcdeps_load(void)
{
	char depcmd[256], deppath[256], dep[128], type[16];
	char *line = NULL, *ptr, *tmp, svc[128], old[128];
	FILE *depfile;
	size_t len, pos;
	int t = 0, o = 1;

	if (service_deplist)
		return service_deplist;

	/* get dependency list file */
	snprintf(deppath, ARRAY_SIZE(deppath), "%s/stage-%d/deps_%s", SV_DEPDIR,
			RS_STAGE.level, RS_STAGE.type);
	if (file_test(deppath, 0) <= 0) {
		snprintf(depcmd, ARRAY_SIZE(depcmd), "%s -%d --%s", SV_DEPGEN,
				RS_STAGE.level, RS_STAGE.type);
		if (system(depcmd)) {
			ERR("Failed to execute `%s'\n", depcmd);
			return NULL;
		}
	}

	if ((depfile = fopen(deppath, "r")) == NULL) {
		ERR("Failed to open %s\n", deppath);
		return NULL;
	}

	RS_SvcDepsList_T *deps_list = rs_svcdeps_new();
	RS_SvcDeps_T *svc_deps;

	while (rs_getline(depfile, &line, &len) > 0) {
		/* get service name */
		ptr = strchr(line, ':');
		*ptr++ = '\0';
		pos = ptr-line;
		memcpy(svc, line, pos);
		/* get dependency type/name */
		ptr = strchr(ptr, '=');
		*ptr++ = '\0';
		memcpy(type, line+pos, ptr-line);
		while (strcmp(type, rs_deps_type[t]))
			t++;

		/* add and initialize a service dependency */
		if (o) {
			memcpy(old, svc, pos+1);
			svc_deps = rs_svcdeps_add(deps_list, svc);
			o = 0;
		}
		else if (strcmp(svc, old)) {
			svc_deps = rs_svcdeps_add(deps_list, svc);
			memcpy(old, svc, pos+1);
		}

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
		t = 0;
	}
	fclose(depfile);

	return deps_list;
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


RS_DepTypeList_T *rs_deplist_new(void)
{
	RS_DepTypeList_T *list = err_malloc(sizeof(RS_DepTypeList_T));
	SLIST_INIT(list);
	return list;
}

RS_DepType_T *rs_deplist_add(RS_DepTypeList_T *list, const char *str)
{
	RS_DepType_T *elm = err_malloc(sizeof(RS_DepType_T));
	elm->type = err_strdup(str);

	for (int i = 0; i < RS_DEPS_PRIO; i++)
		elm->priority[i] = rs_stringlist_new();
	SLIST_INSERT_HEAD(list, elm, entries);

	return elm;
}

RS_DepType_T *rs_deplist_adu(RS_DepTypeList_T *list, const char *str)
{
	RS_DepType_T *elm = rs_deplist_find(list, str);
	if (elm)
		return elm;

	return rs_deplist_add(list, str);
}

RS_DepType_T *rs_deplist_find(RS_DepTypeList_T *list, const char *str)
{
	RS_DepType_T *elm;

	if (list)
		SLIST_FOREACH(elm, list, entries)
			if (strcmp(elm->type, str) == 0)
				return elm;
	return NULL;
}

void rs_deplist_free(RS_DepTypeList_T *list)
{
	int i;
	RS_DepType_T *elm;

	if (!list)
		return;

	while (!SLIST_EMPTY(list)) {
		elm = SLIST_FIRST(list);
		for (i = 0; i < RS_DEPS_PRIO; i++)
			rs_stringlist_free(elm->priority[i]);

		free(elm->type);
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

void rs_svcdeps_free(RS_SvcDepsList_T *list)
{
	int i;
	RS_SvcDeps_T *elm;

	if (!list)
		return;

	while (!SLIST_EMPTY(list)) {
		elm = SLIST_FIRST(list);
		for (i = 0; i < RS_DEPS_TYPE; i++)
			rs_stringlist_free(elm->deps[i]);

		free(elm->svc);
		SLIST_REMOVE_HEAD(list, entries);
		free(elm);
	}
	list = NULL;
}

