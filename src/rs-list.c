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

#include "rs-list.h"

RS_StringList_T *rs_stringlist_new(void)
{
	RS_StringList_T *list = err_malloc(sizeof(*list));
	TAILQ_INIT(list);
	return list;
}

RS_String_T *rs_stringlist_add(RS_StringList_T *list, const char *str)
{
	static unsigned id;
	RS_String_T *elm = err_malloc(sizeof(RS_String_T));
	elm->str = err_strdup(str);
	elm->data = NULL;
	TAILQ_INSERT_TAIL(list, elm, entries);
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
		TAILQ_REMOVE(list, elm, entries);
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
		TAILQ_REMOVE(list, elm, entries);
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
		TAILQ_FOREACH(elm, list, entries)
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
	TAILQ_REMOVE(src, ent, entries);
	TAILQ_INSERT_TAIL(dst, ent, entries);
	return 0;
}

void rs_stringlist_free(RS_StringList_T **list)
{
	RS_String_T *elm;

	if (!list)
		return;
	TAILQ_FOREACH(elm, *list, entries) {
		TAILQ_REMOVE(*list, elm, entries);
		free(elm->str);
		free(elm);
	}
	free(*list);
	*list = NULL;
}

