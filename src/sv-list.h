/*
 * Copyright (c) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-list.h  0.13.0 2016/12/28
 */

#ifndef SV_LIST_H
#define SV_LIST_H

#include "queue.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* singly-linked list using queue(3) */
typedef struct SV_String {
	char *str;
	void *data;
	TAILQ_ENTRY(SV_String) entries;
} SV_String_T;
typedef TAILQ_HEAD(SV_StringList, SV_String) SV_StringList_T;

/* string list helpers to manage string list using queue(3) */
extern SV_StringList_T *sv_stringlist_new(void);
extern SV_String_T *sv_stringlist_add (SV_StringList_T *list, const char *str);
extern SV_String_T *sv_stringlist_adu (SV_StringList_T *list, const char *str);
extern SV_String_T *sv_stringlist_find(SV_StringList_T *list, const char *str);
extern int          sv_stringlist_del (SV_StringList_T *list, const char *str);
extern int          sv_stringlist_rem (SV_StringList_T *list, SV_String_T *elm);
extern size_t       sv_stringlist_len (SV_StringList_T *list);
extern int          sv_stringlist_mov (SV_StringList_T *src, SV_StringList_T *dst, SV_String_T *ent);
extern void         sv_stringlist_free(SV_StringList_T **list);

#ifdef __cplusplus
}
#endif

#endif /* SV_LIST_H */
