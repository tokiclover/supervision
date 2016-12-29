/*
 * Copyright (c) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-list.c  0.13.0 2016/12/28
 */

#include "sv-list.h"

SV_StringList_T *sv_stringlist_new(void)
{
	SV_StringList_T *list = err_malloc(sizeof(*list));
	TAILQ_INIT(list);
	return list;
}

SV_String_T *sv_stringlist_add(SV_StringList_T *list, const char *str)
{
	SV_String_T *elm = err_malloc(sizeof(SV_String_T));
	elm->str = err_strdup(str);
	elm->data = NULL;
	TAILQ_INSERT_TAIL(list, elm, entries);
	return elm;
}

SV_String_T *sv_stringlist_adu(SV_StringList_T *list, const char *str)
{
	SV_String_T *elm = sv_stringlist_find(list, str);
	if (elm)
		return elm;

	return sv_stringlist_add(list, str);
}

int sv_stringlist_del(SV_StringList_T *list, const char *str)
{
	SV_String_T *elm = sv_stringlist_find(list, str);

	if (elm) {
		TAILQ_REMOVE(list, elm, entries);
		free(elm->str);
		free(elm);
		return 0;
	}
	errno = EINVAL;
	return -1;
}

int sv_stringlist_rem(SV_StringList_T *list, SV_String_T *elm)
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

SV_String_T *sv_stringlist_find(SV_StringList_T *list, const char *str)
{
	SV_String_T *elm;

	if (list)
		TAILQ_FOREACH(elm, list, entries)
			if (strcmp(elm->str, str) == 0)
				return elm;
	return NULL;
}

size_t sv_stringlist_len(SV_StringList_T *list)
{
	SV_String_T *elm;
	size_t len = 0;

	TAILQ_FOREACH(elm, list, entries)
		len++;
	return len;
}

int sv_stringlist_mov(SV_StringList_T *src, SV_StringList_T *dst, SV_String_T *ent)
{
	if (src == NULL || dst == NULL) {
		errno = EINVAL;
		return -1;
	}
	TAILQ_REMOVE(src, ent, entries);
	TAILQ_INSERT_TAIL(dst, ent, entries);
	return 0;
}

void sv_stringlist_free(SV_StringList_T **list)
{
	SV_String_T *elm;

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

