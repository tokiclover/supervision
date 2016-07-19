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

#define SV_CONF_FILE SV_SVCDIR "/.opt/sv.conf"

/* global configuration list */
static const char **sv_conf;

/* load configuration file as an environment list */
static void rs_conf_load(void);
/* free an allocated configuration list */
static void rs_conf_free(void);

int rs_conf_yesno(const char *env) {
	return rs_yesno(rs_getconf(env));
}

const char *rs_getconf(const char *env)
{
	if (!sv_conf)
		rs_conf_load();
	if (!env)
		return NULL;

	const char *ptr;
	size_t len = strlen(env);
	int i = 0;

	while ((ptr = sv_conf[i]))
		if (strncmp(sv_conf[i++], env, len) == 0)
			break;

	if (!ptr)
		return NULL;

	ptr = strchr(ptr, '=');
	if (ptr)
		return ptr+1;

	return NULL;
}

void rs_conf_load(void)
{
	FILE *fp;
	char *line = NULL, *env, *p, *ptr;
	size_t count = 0, len, l, num = 32, pos, size = 1024;

	if ((fp = fopen(SV_CONF_FILE, "r")) == NULL) {
		fprintf(stderr, "%s: Failed to open %s\n", prgname, SV_CONF_FILE);
		exit(EXIT_FAILURE);
	}

	sv_conf = err_calloc(num, sizeof(void *));
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
		/* remove trailing comments/white spaces */
		p = strchr(ptr, '#');
		if (p)
			while (*p == ' ')
				*p-- = '\0';

		/* append conf value if any */
		l = strlen(ptr);
		if (l++) {
			env = err_realloc(env, pos+l);
			memcpy(env+pos, ptr, l);
		}
		else
			continue;

		/* append conf string to list */
		sv_conf[count++] = env;
		env = err_malloc(size);
		/* expand list if necessary */
		if (count == (num-1)) {
			num += 32;
			sv_conf = err_realloc(sv_conf, sizeof(void *)*num);
		}
	}
	fclose(fp);

	if (sv_conf[count-1] != env)
		free(env);
	sv_conf[count++] = NULL;
	sv_conf = err_realloc(sv_conf, sizeof(void *)*count);

	atexit(rs_conf_free);
}

void rs_conf_free(void)
{
	int i = 0;
	while (sv_conf[i])
		free((void *)sv_conf[i++]);
	free(sv_conf);
}

