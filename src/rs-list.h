/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs-list.h  0.12.6.4 2016/12/24
 */

#ifndef RS_LIST_H
#define RS_LIST_H

#include "queue.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* singly-linked list using queue(3) */
typedef struct RS_String {
	char *str;
	void *data;
	TAILQ_ENTRY(RS_String) entries;
} RS_String_T;
typedef TAILQ_HEAD(RS_StringList, RS_String) RS_StringList_T;

/* string list helpers to manage string list using queue(3) */
extern RS_StringList_T *rs_stringlist_new(void);
extern RS_String_T *rs_stringlist_add (RS_StringList_T *list, const char *str);
extern RS_String_T *rs_stringlist_adu (RS_StringList_T *list, const char *str);
extern RS_String_T *rs_stringlist_find(RS_StringList_T *list, const char *str);
extern int          rs_stringlist_del (RS_StringList_T *list, const char *str);
extern int          rs_stringlist_rem (RS_StringList_T *list, RS_String_T *elm);
extern size_t       rs_stringlist_len (RS_StringList_T *list);
extern int          rs_stringlist_mov (RS_StringList_T *src, RS_StringList_T *dst, RS_String_T *ent);
extern void         rs_stringlist_free(RS_StringList_T **list);

#ifdef __cplusplus
}
#endif

#endif /* RS_LIST_H */
