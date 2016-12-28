/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs-conf.c  0.12.6.4 2016/12/24
 */

#include "helper.h"
#include "sv.h"

#define SV_CONFIG_FILE SYSCONFDIR "/sv.conf"

/* global configuration array */
static const char **SV_CONFIG_ARRAY;

/* load configuration file as an environment list */
static int  rs_conf_load(void);
/* free an allocated configuration list */
static void rs_conf_free(void);

int rs_conf_yesno(const char *env) {
	return rs_yesno(rs_getconf(env));
}

const char *rs_getconf(const char *env)
{
	if (!SV_CONFIG_ARRAY)
		if (rs_conf_load())
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

static int rs_conf_load(void)
{
	FILE *fp;
	char *line = NULL, *env, *ptr;
	size_t count = 0, len, l, num = 32, pos, size = 1024;

	if ((fp = fopen(SV_CONFIG_FILE, "r")) == NULL) {
		WARN("Failed to open %s\n", SV_CONFIG_FILE);
		return -1;
	}

	SV_CONFIG_ARRAY = err_calloc(num, sizeof(void *));
	env = err_malloc(size);

	while (rs_getline(fp, &line, &len) > 0) {
		if (line[0] == '#')
			continue;

		/* get conf name */
		ptr = strchr(line, '=');
		pos = ptr-line;
		memcpy(env, line, pos);

		/* get conf value */
		ptr = shell_string_value(ptr);
		if (ptr == NULL)
			continue;

		/* append conf value if any */
		l = strlen(ptr);
		if (l++) {
			env = err_realloc(env, pos+l);
			memcpy(env+pos, ptr, l);
		}
		else
			continue;

		/* append conf string to list */
		SV_CONFIG_ARRAY[count++] = env;
		env = err_malloc(size);
		/* expand list if necessary */
		if (count == num) {
			num += 32;
			SV_CONFIG_ARRAY = err_realloc(SV_CONFIG_ARRAY, sizeof(void *)*num);
		}
	}
	fclose(fp);

	if (SV_CONFIG_ARRAY[count-1] != env)
		free(env);
	SV_CONFIG_ARRAY[count++] = NULL;
	SV_CONFIG_ARRAY = err_realloc(SV_CONFIG_ARRAY, sizeof(void *)*count);

	atexit(rs_conf_free);
	return 0;
}

static void rs_conf_free(void)
{
	int i = 0;
	while (SV_CONFIG_ARRAY[i])
		free((void *)SV_CONFIG_ARRAY[i++]);
	free(SV_CONFIG_ARRAY);
}

