/*
 * Copyright (c) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs-deps.c  0.13.0 2017/01/22
 */

#include <string.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <poll.h>
#include <unistd.h>
#include "sv-deps.h"

static const char *const sv_svcdeps_type[] = { "before", "after", "use", "need" };
const char *const sv_keywords[] = {
	NULL,
	"timeout", "shutdown", "supervision", "docker", "jail", "lxc", "openvz",
	"prefix", "systemd-nspawn", "uml", "vserver", "xen0", "xenu",
	NULL
};

struct SV_Services SERVICES = {
	.svclist      = NULL,
	.svcdeps      = NULL,
	.virt_svcdeps = NULL,
	.virt_count   = 0,
};

static int  sv_deptree_file_load(SV_DepTree_T *deptree);
static int  sv_deptree_file_save(SV_DepTree_T *deptree);
static void sv_svcdeps_free(void);
static SV_SvcDepsList_T *sv_svcdeps_new(void);
static SV_SvcDeps_T *sv_svcdeps_add (const char *svc);
static SV_SvcDeps_T *sv_svcdeps_adu (const char *svc);
static SV_SvcDeps_T *sv_svcdeps_find(const char *svc);
static SV_String_T *sv_stringlist_fid(SV_StringList_T *list, SV_String_T *ent);
/* load generate service dependency */
static int           sv_svcdeps_gen(const char *svc);
static void sv_virtsvc_insert(SV_SvcDeps_T *elm);

static void sv_runlevel_migrate(void);

static void sv_deptree_alloc(SV_DepTree_T *deptree)
{
	int p;

	deptree->size += SV_DEPTREE_INC;
	deptree->tree = err_realloc(deptree->tree, deptree->size*sizeof(void*));
	for (p = deptree->size-SV_DEPTREE_INC; p < deptree->size; p++)
		deptree->tree[p] = sv_stringlist_new();
}

void sv_deptree_free(SV_DepTree_T *deptree)
{
	int i;
	for (i = 0; i < deptree->size; i++)
		sv_stringlist_free(&deptree->tree[i]);
	deptree->size = 0;
	deptree->tree = 0;
	deptree->prio = 0;
}

static int sv_deptree_add(int type, int prio, SV_String_T *svc, SV_DepTree_T *deptree)
{
	char *s = svc->str;
	SV_SvcDeps_T *d = svc->data;
	SV_String_T *ent;
	int add, pri;
	int p, t, r;

	/* add service to list if and only if, either a service is {use,need}ed or
	 * belongs to this particular init-stage/runlevel */
	if (type < SV_SVCDEPS_USE && !sv_stringlist_find(deptree->list, s))
		return -prio;

	if (!d) svc->data = d = sv_svcdeps_find(s);
	/* insert the real service instead of a virtual one */
	if (!d && (d = sv_virtsvc_find(deptree->list, s)))
		svc->data = d;
	if (!d) return -1;
	if (d->virt) s = d->svc;
	if (prio < 0) {
		if (d->deps[SV_SVCDEPS_BEFORE]) prio = 1;
		else prio = 0;
	}
	pri = prio+1;

	/* expand the list when needed */
	if (pri > deptree->size && deptree->size < SV_DEPTREE_MAX)
		sv_deptree_alloc(deptree);

	if (pri < SV_DEPTREE_MAX) {
		/* handle {after,use,need} type  which insert dependencies above */
		if (type) {
			for (t = SV_SVCDEPS_AFTER; t <= SV_SVCDEPS_NEED; t++)
			TAILQ_FOREACH(ent, d->deps[t], entries) {
				add = 1;
				for (p = pri; p < deptree->size; p++)
					if (sv_stringlist_fid(deptree->tree[p], ent)) {
						add = 0; break;
					}
				if (add)
					sv_deptree_add(t, pri, ent, deptree);
			}
		}
		else {
			/* handle before type which incerts dependencies below */
			TAILQ_FOREACH(ent, d->deps[type], entries) {
				add = 1;
				for (p = 0; p < prio; p++)
					if (sv_stringlist_fid(deptree->tree[p], ent)) {
						add = 0; break;
					}
				/* issue here is to add everything nicely */
				if (add) {
					/* prio level should be precisely handled here; so, the
					 * follow up is required to get before along with the others
					 */
					r = sv_deptree_add(type, prio, ent, deptree);
					if (r < 0)
						continue;
					r = sv_deptree_add(SV_SVCDEPS_AFTER, r, ent, deptree);
					if (r < SV_DEPTREE_MAX && r > 0)
						prio = ++r > prio ? r : prio;
				}
			}
		}
	}

	/* move up anything found before anything else */
	for (p = 0; p < prio; p++)
		if ((ent = sv_stringlist_find(deptree->tree[p], s))) {
			if (prio < SV_DEPTREE_MAX) {
				sv_stringlist_mov(deptree->tree[p], deptree->tree[prio], ent);
				sv_deptree_add(SV_SVCDEPS_AFTER, prio, svc, deptree);
			}
			return prio;
		}
	/* add only if necessary */
	for (p = prio; p < deptree->size; p++)
		if (sv_stringlist_find(deptree->tree[p], s))
			return p;
	prio = prio > SV_DEPTREE_MAX ? SV_DEPTREE_MAX-1 : prio;
	ent = sv_stringlist_add(deptree->tree[prio], s);
	ent->data = d;
	return prio;
}

static int sv_deptree_file_load(SV_DepTree_T *deptree)
{
	int p;
	char path[PATH_MAX];
	char *line = NULL, *ptr, *tmp;
	FILE *fp;
	size_t len = 0;

	snprintf(path, ARRAY_SIZE(path), "%s/%s", SV_TMPDIR_DEPS, sv_runlevel[sv_stage]);
	if (access(path, F_OK))
		return -1;
	if ((fp = fopen(path, "r+")) == NULL) {
		ERR("Failed to open %s: %s\n", path, strerror(errno));
		return -1;
	}

	while (getline(&line, &len, fp) > 0) {
		ptr = strchr(line, '_')+1;
		p = atoi(ptr);
		ptr = strchr(line, '=')+1;

		if (!(ptr = shell_string_value(ptr)))
			continue;
		while (p >= deptree->size && deptree->size < SV_DEPTREE_MAX)
			sv_deptree_alloc(deptree);
		if (p >= SV_DEPTREE_MAX)
			p = SV_DEPTREE_MAX-1;

		/* append service list */
		while ((tmp = strsep(&ptr, " ")))
			sv_stringlist_add(deptree->tree[p], tmp);
	}
	if (len) free(line);
	fclose(fp);

	return 0;
}

static int sv_deptree_file_save(SV_DepTree_T *deptree)
{
	SV_String_T *ent;
	int p;
	char path[PATH_MAX];
	FILE *fp;

	if (!deptree)
		return -ENOENT;

	snprintf(path, ARRAY_SIZE(path), "%s/%s", SV_TMPDIR_DEPS, sv_runlevel[sv_stage]);
	if ((fp = fopen(path, "w+")) == NULL) {
		ERR("Failed to open `%s': %s\n", path, strerror(errno));
		return -1;
	}

	for (p = 0; p < deptree->size; p++) {
		fprintf(fp, "dep_%04d=\"", p);
		TAILQ_FOREACH(ent, deptree->tree[p], entries)
			fprintf(fp, "%s ", ent->str);
		fseek(fp, -1L, SEEK_CUR);
		if (getc(fp) != '"') fseek(fp, -1L, SEEK_CUR);
		fprintf(fp, "\"\n");
	}
	fflush(fp);
	fclose(fp);

	return 0;
}

void svc_deptree_load(SV_DepTree_T *deptree)
{
	SV_String_T *ent;
	sv_deptree_alloc(deptree);

	TAILQ_FOREACH(ent, deptree->list, entries)
		sv_deptree_add(SV_SVCDEPS_USE, -1, ent, deptree);
}

void sv_deptree_load(SV_DepTree_T *deptree)
{
	SV_String_T *ent;
	SV_String_T *elm;
	SV_SvcDeps_T *dep;
	int pri;

	/* load previous deptree file if any, or initialize a new list */
	if (sv_deptree_file_load(deptree))
		sv_deptree_alloc(deptree);
	if (!deptree->list)
		deptree->list = sv_svclist_load(NULL);
	sv_svcdeps_load(NULL);

	/* XXX: handle {after,use,need} first */
	TAILQ_FOREACH(ent, deptree->list, entries)
		sv_deptree_add(SV_SVCDEPS_AFTER, -1, ent, deptree);
	TAILQ_FOREACH(ent, deptree->list, entries)
		sv_deptree_add(SV_SVCDEPS_BEFORE, 0, ent, deptree);

	for (pri = deptree->size-1; pri > 0; pri--)
		if (!TAILQ_EMPTY(deptree->tree[pri])) {
			deptree->prio = pri;
			break;
		}

	/* handle SVC_{AFTER,BEFORE}="*" */
	TAILQ_FOREACH(ent, deptree->list, entries) {
		if ((dep = ent->data) &&
			(elm = TAILQ_FIRST(dep->deps[SV_SVCDEPS_AFTER])) &&
			(*elm->str == '*')) {
			for (pri = 0; pri < deptree->prio; pri++) {
				if ((elm = sv_stringlist_fid(deptree->tree[pri], ent))) {
					sv_stringlist_mov(deptree->tree[pri], deptree->tree[0], elm);
					break;
				}
			}
		}

		if ((dep = ent->data) &&
			(elm = TAILQ_FIRST(dep->deps[SV_SVCDEPS_BEFORE])) &&
			(*elm->str == '*')) {
			for (pri = deptree->prio; pri > 0; pri--) {
				if ((elm = sv_stringlist_fid(deptree->tree[pri], ent))) {
					if (deptree->prio >= deptree->size-1)
						sv_stringlist_mov(deptree->tree[pri],
								deptree->tree[deptree->prio], elm);
					else
						sv_stringlist_mov(deptree->tree[pri],
								deptree->tree[++deptree->prio], elm);
					break;
				}
			}
		}
	}

	/* save everything to a file */
	sv_deptree_file_save(deptree);
}

static void sv_runlevel_migrate(void)
{
	char op[PATH_MAX], np[PATH_MAX];
	DIR *nd, *od;
	int i, ofd, nfd;
	struct dirent *ent;

	switch (sv_stage) {
	case SV_SYSINIT_LEVEL:
		i = 0; break;
	case SV_SYSBOOT_LEVEL:
		i = 1; break;
	case SV_DEFAULT_LEVEL:
		i = 2; break;
	case SV_SHUTDOWN_LEVEL:
		i = 3; break;
	default: return; }

	snprintf(op, ARRAY_SIZE(op), "%s/.stage-%d", SV_SVCDIR, i);
	if (access(op, F_OK))
		return;
	od = opendir(op);
	snprintf(np, ARRAY_SIZE(np), "%s/.%s", SV_SVCDIR, sv_runlevel[sv_stage]);
	nd = opendir(np);
	if (!od || !nd)
		return;
	ofd = dirfd(od);
	nfd = dirfd(nd);
	if (ofd < 0 || nfd < 0)
		return;

	while ((ent = readdir(od))) {
		if (*ent->d_name == '.')
			continue;
		renameat(ofd, ent->d_name, nfd, ent->d_name);
	}
	closedir(od);
	closedir(nd);
	rmdir(op);
}

SV_StringList_T *sv_svclist_load(char *dir_path)
{
	char path[PATH_MAX], *ptr;
	DIR *dir;
	struct dirent *ent;
	SV_StringList_T *svclist;

	/*
	 * get the service list for this stage
	 */
	if (dir_path)
		ptr = dir_path;
	else {
		ptr = path;
		sv_runlevel_migrate();
		snprintf(path, ARRAY_SIZE(path), "%s/.%s", SV_SVCDIR, sv_runlevel[sv_stage]);
	}
	if ((dir = opendir(ptr)) == NULL) {
		ERR("Failed to open `%s' directory: %s\n", ptr, strerror(errno));
		exit(EXIT_FAILURE);
	}

	svclist = sv_stringlist_new();
	while ((ent = readdir(dir))) {
#ifdef _DIRENT_HAVE_D_TYPE
		switch (ent->d_type) {
		case DT_DIR:
		case DT_LNK:
		case DT_REG:
			break;
		default:
			continue;
			break;
		}
#endif
		if (ent->d_name[0] != '.')
			sv_stringlist_add(svclist, ent->d_name);
	}
	closedir(dir);

	return svclist;
}

static int sv_svcdeps_gen(const char *svc)
{
	int retval;
	char cmd[1024], *ptr;

	if (svc) {
		ptr = cmd;
		snprintf(cmd, ARRAY_SIZE(cmd), "%s %s", SV_DEPGEN, svc);
	}
	else
		ptr = SV_DEPGEN;

	retval = system(ptr);
	if (retval)
		ERR("Failed to execute `%s': %s\n", cmd, strerror(errno));
	return retval;
}

#define WAIT_SVSCAN                                            \
	if (sv_level != SV_SYSINIT_LEVEL) { /* SVSCAN */           \
		snprintf(cmd, sizeof(cmd), "%s/svscan.pid", SV_TMPDIR);\
		do {                                                   \
			if (!access(cmd, F_OK)) break;                     \
			poll(NULL, 0, 100); /* milisecond sleep */         \
			t++;                                               \
		} while (t < 600);                                     \
		if (t >= 600) {                                        \
			ERR("Timed out waiting for `%s'\n", SV_INIT_STAGE);\
			exit(EXIT_FAILURE);                                \
		}                                                      \
	} else {                                                   \
		do { r = waitpid(p, &t, 0);                            \
			if (r < 0) {                                       \
				if (errno != ECHILD) continue;                 \
				else ERROR("%s: waitpid()", __func__);         \
			}                                                  \
		} while (!WIFEXITED(t));                               \
		if (!file_test(SV_TMPDIR_DEPS, 'd'))                   \
			ERROR("`%s' failed to setup `%s", SV_INIT_STAGE, SV_TMPDIR); \
	}

#define ARG_OFFSET 32
#define EXEC_SVSCAN                                            \
	if (sv_level != SV_SYSINIT_LEVEL)                          \
		setsid();                                              \
	execl(SV_INIT_STAGE, strrchr(SV_INIT_STAGE, '/'), cmd, arg, NULL); \
	ERROR("Failed to execl(%s ...)", SV_INIT_STAGE);

SV_SvcDeps_T *sv_svcdeps_load(const char *service)
{
	char cmd[128], *ptr, *svc, *type, *line = NULL;
	char *arg;
	FILE *fp;
	size_t len = 0, l = 0;
	int r, t = 0;
	pid_t p;
	SV_SvcDeps_T *deps = NULL;

	/* create a new list only when not updating the list */
	if (SERVICES.svcdeps) {
		if (service)
			deps = sv_svcdeps_find(service);
		else
			return NULL;
		if (deps)
			return deps;
		if (sv_svcdeps_gen(service))
			return NULL;
		l = strlen(service);
	}
	else
		SERVICES.svcdeps = sv_svcdeps_new();

	/* initialize SV_RUNDIR and start _SVSCAN_ if necessary */
	if (!file_test(SV_TMPDIR_DEPS, 'd')) {
		if (sv_level == SV_SYSINIT_LEVEL) {
			arg = "--background";
			ptr = (char*)sv_runlevel[sv_level];
		}
		else {
			arg = "--foreground";
			ptr = "svscan";
		}
		snprintf(cmd, ARRAY_SIZE(cmd), "--%s", ptr);
		if ((p = fork()) < 0)
			ERROR("%s: Failed to fork()", __func__);
		if (p) {
			WAIT_SVSCAN;
		}
		else {
			EXEC_SVSCAN;
		}
	}

	/* get dependency list file */
	if (access(SV_SVCDEPS_FILE, F_OK))
		if (sv_svcdeps_gen(NULL))
			return NULL;
	if ((fp = fopen(SV_SVCDEPS_FILE, "r")) == NULL) {
		ERR("Failed to open %s: %s\n", SV_SVCDEPS_FILE, strerror(errno));
		return NULL;
	}

	while (getline(&line, &len, fp) > 0) {
		if (service) {
			/* break the loop when updating the list */
			if (strncmp(line, service, l)) {
				if (deps) {
					free(line);
					fclose(fp);
					return deps;
				}
				continue;
			}
			else if (!deps)
				deps = sv_svcdeps_add(service);
		}

		/* get service name */
		svc = line;
		ptr = strchr(line, ':');
		*ptr++ = '\0';
		/* get dependency type */
		type = ptr;
		ptr = strchr(ptr, '=');
		*ptr++ = '\0';

		if (!deps || strcmp(svc, deps->svc))
			deps = sv_svcdeps_add(svc);
		if (!(ptr = shell_string_value(ptr)))
			continue;

		if (strcmp(type, "provide") == 0) {
			deps->virt = err_strdup(ptr);
			sv_virtsvc_insert(deps);
			continue;
		}
		else if (strcmp(type, "nohang") == 0) {
			if (sv_yesno(ptr))
				SV_SVCOPTS_SET(deps, SV_SVCOPTS_NOHANG);
			continue;
		}
		else if (strcmp(type, "timeout") == 0) {
			errno = 0;
			deps->timeout = (int)strtol(ptr, NULL, 10);
			if (errno == ERANGE) deps->timeout = 0;
			continue;
		}
		else if (strcmp(type, "keyword") == 0) {
			if (!(ptr = strtok_r(ptr, " \t", &type)))
				continue;
			do {
				for (t = 1; sv_keywords[t]; t++)
					if (strcmp(ptr, sv_keywords[t]) == 0) {
						SV_KEYWORD_SET(deps, t);
						break;
					}
			} while ((ptr = strtok_r(NULL, " \t", &type)));
			continue;
		}

		for (t = 0; strcmp(type, sv_svcdeps_type[t]); t++)
			;
		if (t >= SV_SVCDEPS_TYPE)
			continue;
		/* append service list */
		if (!(svc = strtok_r(ptr, " \t", &type)))
			continue;
		do {
			sv_stringlist_add(deps->deps[t], svc);
		} while ((svc = strtok_r(NULL, " \t", &type)));
	}
	fclose(fp);
	if (len) free(line);
	if (service)
		return deps;

#ifdef DEBUG
	atexit(sv_svcdeps_free);
#endif
	return deps;
}
#undef EXEC_SVSCAN
#undef WAIT_SVSCAN
#undef ARG_OFFSET

static SV_SvcDepsList_T *sv_svcdeps_new(void)
{
	SV_SvcDepsList_T *list = err_malloc(sizeof(SV_SvcDepsList_T));
	TAILQ_INIT(list);
	return list;
}

static SV_SvcDeps_T *sv_svcdeps_add(const char *svc)
{
	static unsigned int id;
	SV_SvcDeps_T *elm = err_malloc(sizeof(SV_SvcDeps_T));
	elm->did = ++id;
	elm->svc = err_strdup(svc);
	elm->virt = NULL;
	elm->keyword = 0;
	elm->timeout = 0;
	elm->options = 0;

	for (int i = 0; i < SV_SVCDEPS_TYPE; i++)
		elm->deps[i] = sv_stringlist_new();
	TAILQ_INSERT_TAIL(SERVICES.svcdeps, elm, entries);

	return elm;
}

static SV_SvcDeps_T *sv_svcdeps_adu(const char *svc)
{
	SV_SvcDeps_T *elm = sv_svcdeps_find(svc);
	if (elm)
		return elm;

	return sv_svcdeps_add(svc);
}

static SV_String_T *sv_stringlist_fid(SV_StringList_T *list, SV_String_T *ent)
{
	SV_String_T *elm;
	SV_SvcDeps_T *d, *D = ent->data;
	TAILQ_FOREACH(elm, list, entries) {
		d = elm->data;
		if (D && d) {
			if (d->did == D->did) return elm;
		}
		else if (!strcmp(ent->str, elm->str))
			return elm;
	}
	return NULL;
}

static SV_SvcDeps_T *sv_svcdeps_find(const char *svc)
{
	SV_SvcDeps_T *elm;

	TAILQ_FOREACH(elm, SERVICES.svcdeps, entries)
		if (strcmp(elm->svc, svc) == 0)
			return elm;
	return NULL;
}

SV_SvcDeps_T *sv_virtsvc_find(SV_StringList_T *svclist, const char *svc)
{
	int i;
	SV_SvcDeps_T *d = NULL;

	if (!svc || !SERVICES.virt_svcdeps)
		return NULL;

	for (i = 0; i < SERVICES.virt_count; i++) {
		if (strcmp(svc, SERVICES.virt_svcdeps[i]->virt))
			continue;
		d = SERVICES.virt_svcdeps[i];
		if (!svclist)
			break;
		/* insert any provider included in the init-stage */
		if (sv_stringlist_find(svclist, d->svc))
			break;
	}

	return d;
}

static void sv_virtsvc_insert(SV_SvcDeps_T *elm)
{
	static size_t num;

	if (SERVICES.virt_count == num) {
		num += 8;
		SERVICES.virt_svcdeps = err_realloc(SERVICES.virt_svcdeps,
				num*sizeof(void*));
	}
	if (elm)
		SERVICES.virt_svcdeps[SERVICES.virt_count++] = elm;
}

static void sv_svcdeps_free(void)
{
	int i;
	SV_SvcDeps_T *elm;

	if (!SERVICES.svcdeps)
		return;

	TAILQ_FOREACH(elm, SERVICES.svcdeps, entries) {
		TAILQ_REMOVE(SERVICES.svcdeps, elm, entries);

		for (i = 0; i < SV_SVCDEPS_TYPE; i++)
			sv_stringlist_free(&(elm->deps[i]));
		free(elm->svc);
		if (elm->virt)
			free(elm->virt);
		free(elm);
	}
	free(SERVICES.svcdeps);
	free(SERVICES.virt_svcdeps);
	SERVICES.svcdeps      = NULL;
	SERVICES.virt_svcdeps = NULL;
	SERVICES.virt_count   = 0;
}
