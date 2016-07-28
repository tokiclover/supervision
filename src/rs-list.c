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

#define SV_DEPGEN SV_LIBDIR "/sh/dep"
#define SV_TMPDIR_DEPS SV_TMPDIR "/deps"
#define SV_INIT_STAGE SV_LIBDIR "/sh/init-stage"
#define RS_DEPTREE_PRIO 16
/* safety net for broken cyclic dependency */
#define RS_DEPTREE_MAX  1024

RS_SvcDepsList_T *service_deplist;
static RS_SvcDeps_T **virtual_deplist;
static RS_DepTypeList_T *stage_deplist;
static RS_StringList_T  *stage_svclist;
static RS_StringList_T **deptree_list;
size_t rs_deptree_prio = 0;

static void rs_svcdeps_free(void);
static void rs_virtual_insert(RS_SvcDeps_T *elm);
static RS_SvcDeps_T *rs_virtual_find(const char *svc);
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
	int add, pri = prio+1;
	int p, t, r;

	if (s == NULL)
		return 0;
	/* add service to list if and only if, either a service is {use,need}ed or
	 * belongs to this particular init-stage
	 */
	if (type < RS_DEPS_USE && !rs_stringlist_find(stage_svclist, s))
		return -prio;
	/* insert the real service instead of a virtual one */
	if (!svc_deps && (svc_deps = rs_virtual_find(s))) {
		for (p = 0; p < rs_virtual_count; p++) {
			if (strcmp(virtual_deplist[p]->virt, s))
				continue;
			/* insert any provider included in the init-stage */
			if (rs_stringlist_find(stage_svclist, virtual_deplist[p]->svc)) {
				svc_deps = virtual_deplist[p];
				break;
			}
		}
		s = svc_deps->svc;
	}

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
				if (type)
					rs_deptree_add(type, prio, s);
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

static int rs_deptree_file_save(RS_StringList_T *deptree[])
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

	snprintf(deppath, sizeof(deppath), "%s/%d_deptree", SV_TMPDIR_DEPS,	rs_stage);
	if ((depfile = fopen(deppath, "wa+")) == NULL) {
		ERR("Failed to open %s\n", deppath);
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
	int p, t;
	RS_DepType_T *deptype[RS_DEPS_TYPE];
	RS_String_T *ent, *elm;
	RS_StringList_T **ptr;

	/* try to load the file if any */
	if ((ptr = rs_deptree_file_load())) {
		return ptr;
	}
	stage_deplist = rs_deplist_load();
	rs_svcdeps_load();

	/* initialize the list */
	rs_deptree_alloc();
	for (t = 0; t < RS_DEPS_TYPE; t++)
		deptype[t] = rs_deplist_find(stage_deplist, rs_deps_type[t]);

	/* handle high priority first to be sure to satisfy lower prio services.
	 * before type is more difficult to handle with the other types; so,
	 * handle it last; otherwise, a slip off by _one_ will be there.
	 */
	for (t = RS_DEPS_TYPE-1; t >= 0; t--)
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
	rs_stringlist_free(stage_svclist);

	return deptree_list;
}

RS_DepTypeList_T *rs_deplist_load(void)
{
	char cmd[256], path[256], dep[16];
	char *line = NULL, *ptr, *tmp, svc[128];
	FILE *file;
	size_t len, pos;
	int pri;

	/*
	 * get the priority dependency type lists for this stage
	 */
	snprintf(path, ARRAY_SIZE(path), "%s/%d_prio", SV_TMPDIR_DEPS, rs_stage);
	if (file_test(path, 0) <= 0) {
		snprintf(cmd, ARRAY_SIZE(cmd), "%s -%d", SV_DEPGEN, rs_stage);
		if (system(cmd)) {
			ERR("Failed to execute `%s': %s\n", cmd, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	if ((file = fopen(path, "r")) == NULL) {
		ERR("Failed to open %s: %s\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	stage_deplist = rs_deplist_new();
	RS_DepType_T *dlp;
	while (rs_getline(file, &line, &len) > 0) {
		/* get dependency type */
		ptr = strchr(line, '_');
		pos = ptr-line;
		pri = atoi(++ptr);
		memcpy(dep, line, pos);
		dep[pos] = '\0';
		ptr += 2;

		/* add a dependency type list */
		if (!pri)
			dlp = rs_deplist_add(stage_deplist, dep);
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
	fclose(file);

	/*
	 * get the service list for this stage
	 */
	snprintf(path, ARRAY_SIZE(path), "%s/%d_list", SV_TMPDIR_DEPS, rs_stage);
	if ((file = fopen(path, "r")) == NULL) {
		ERR("Failed to open %s: %s\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	stage_svclist = rs_stringlist_new();
	while (rs_getline(file, &line, &len) > 0) {
		ptr = line;
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
			rs_stringlist_add(stage_svclist, svc);
			ptr += pos+1;
		}
	}
	fclose(file);

	return stage_deplist;
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

static RS_SvcDeps_T *rs_virtual_find(const char *svc)
{
	int i;

	if (!svc)
		return NULL;

	for (i= 0; i < rs_virtual_count; i++)
		if (strcmp(svc, virtual_deplist[i]->virt) == 0)
			return virtual_deplist[i];

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

