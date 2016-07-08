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
	RS_DepType_T *dlp, *pld;
	RS_String_T *ent, *tne;

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

	/* move up priority level for {after,before} and {use,need} */
	for (int i = 0; i <= 2; ) {
		dlp = rs_deplist_find(deplist, rs_deps_type[i++]);
		pld = rs_deplist_find(deplist, rs_deps_type[i++]);
		SLIST_FOREACH(ent, dlp->priority[2], entries)
			if ((tne = rs_stringlist_find(pld->priority[2], ent->str))) {
				rs_stringlist_mov(pld->priority[2], pld->priority[3], tne);
				rs_stringlist_mov(dlp->priority[2], dlp->priority[3], ent);
			}
	}

	return deplist;
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

	elm = err_malloc(sizeof(struct RS_String));
	elm->str = err_strdup(str);
	return elm;
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

	for (int i = 0; i < RS_DEP_PRIORITY; i++)
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
		for (i = 0; i < RS_DEP_PRIORITY; i++)
			rs_stringlist_free(elm->priority[i]);

		free(elm->type);
		SLIST_REMOVE_HEAD(list, entries);
		free(elm);
	}
	list = NULL;
}

RS_String_T *rs_deplist_add_svc(RS_DepType_T *list, const char *str, int index)
{
	return rs_stringlist_add(list->priority[index], str);
}

RS_String_T *rs_deplist_adu_svc(RS_DepType_T *list, const char *str, int index)
{
	RS_String_T *elm = rs_deplist_find_svc(list, str, index);
	if (elm)
		return elm;

	return rs_stringlist_add(list->priority[index], str);
}

int rs_deplist_del_svc(RS_DepType_T *list, const char *str, int index)
{
	return rs_stringlist_del(list->priority[index], str);
}

RS_String_T *rs_deplist_find_svc(RS_DepType_T *list, const char *str, int index)
{
	return rs_stringlist_find(list->priority[index], str);
}

