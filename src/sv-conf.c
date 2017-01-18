/*
 * Copyright (c) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-conf.c  0.13.0 2016/12/28
 */

#include "helper.h"
#include "sv.h"

#define SV_CONFIG_FILE SYSCONFDIR "/sv.conf"

/* global configuration array */
static const char **SV_CONFIG_ARRAY;

/* load configuration file as an environment list */
static int  sv_conf_load(void);
/* free an allocated configuration list */
static void sv_conf_free(void);

int sv_conf_yesno(const char *env) {
	return sv_yesno(sv_getconf(env));
}

const char *sv_getconf(const char *env)
{
	if (!SV_CONFIG_ARRAY)
		if (sv_conf_load())
			return NULL;
	if (!env)
		return NULL;

	const char *ptr;
	size_t len = strlen(env);
	int i = 0;

	while ((ptr = SV_CONFIG_ARRAY[i]))
		if (strncmp(SV_CONFIG_ARRAY[i++], env, len) == 0)
			break;

	if (!ptr)
		return NULL;

	ptr = strchr(ptr, '=');
	if (ptr)
		return ptr+1;

	return NULL;
}

static int sv_conf_load(void)
{
	FILE *fp;
	char *line = NULL, *ptr;
	size_t count = 0, len = 0, l, num = 16, pos;

	if ((fp = fopen(SV_CONFIG_FILE, "r")) == NULL) {
		WARN("Failed to open %s\n", SV_CONFIG_FILE);
		return -1;
	}
	SV_CONFIG_ARRAY = err_calloc(num, sizeof(void *));

	while (getline(&line, &len, fp) > 0) {
		if (line[0] == '#')
			continue;
		/* get conf name */
		ptr = strchr(line, '=')+1;
		pos = ptr-line;

		/* get conf value */
		ptr = shell_string_value(ptr);
		if (ptr == NULL)
			continue;

		/* append conf value if any */
		l = strlen(ptr);
		if (++l) {
			memmove(line+pos, ptr, l);
			SV_CONFIG_ARRAY[count++] = err_strdup(line);
		}
		else
			continue;

		/* expand list if necessary */
		if (count == num) {
			num += 16;
			SV_CONFIG_ARRAY = err_realloc(SV_CONFIG_ARRAY, sizeof(void *)*num);
		}
	}
	fclose(fp);

	SV_CONFIG_ARRAY[count++] = NULL;
	SV_CONFIG_ARRAY = err_realloc(SV_CONFIG_ARRAY, sizeof(void *)*count);

	atexit(sv_conf_free);
	return 0;
}

static void sv_conf_free(void)
{
	int i = 0;
	while (SV_CONFIG_ARRAY[i])
		free((void *)SV_CONFIG_ARRAY[i++]);
	free(SV_CONFIG_ARRAY);
}

