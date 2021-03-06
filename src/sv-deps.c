/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)rs-deps.c  0.14.0 2019/01/12
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

static void sv_init_level_migrate(void);

static void sv_deptree_alloc(SV_DepTree_T *deptree)
{
	int p;
#ifdef DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, deptree);
#endif

	deptree->size += SV_DEPTREE_INC;
	deptree->tree = err_realloc(deptree->tree, deptree->size*sizeof(void*));
	for (p = deptree->size-SV_DEPTREE_INC; p < deptree->size; p++)
		deptree->tree[p] = sv_stringlist_new();
}

void sv_deptree_free(SV_DepTree_T *deptree)
{
	int i;
#ifdef DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, deptree);
#endif
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
	unsigned int l;
	static unsigned int *DID;
	static size_t SIZE;

#ifdef DEBUG
	if (sv_debug) DBG("%s(type=%d, prio=%d, svc=%s, deptree=%p)\n", __func__, type, prio, svc->str, deptree);
#endif

	if (*svc->str == '*') return -1;
	if (!d) svc->data = d = sv_svcdeps_find(s);
	/* insert the real service instead of a virtual one */
	if (!d) {
		if ((d = sv_virtsvc_find(deptree->list, s)))
			s = d->svc;
		else return -1;
	}

	/* add service to list if and only if, either a service is {use,need}ed; or
	 * belongs to this particular init level/run level; or is already added;
	 */
	if (type < SV_SVCDEPS_USE) {
		if (!sv_stringlist_find(deptree->list, s)) {
			for (l = 0U; l < SIZE; l++)
				if (d->did == *(DID+l)) break;
			if (l == SIZE) return -1;
		}
	}

	if (prio < 0) {
		if (d->deps[SV_SVCDEPS_BEFORE]) prio = 1;
		else prio = 0;
	}
	pri = prio+1;
#ifdef DEBUG
	if (sv_debug) DBG("t=%-1d p=%-4d d=%p svc=%-8s s=%-8s v=%-8s\n", type, prio, d,
			svc->str, s, d->virt);
#endif

	/* expand the list when needed */
	if (pri > deptree->size && deptree->size < SV_DEPTREE_MAX) {
		if (!deptree->size) {
			SIZE = 128LU;
			DID = err_realloc(DID, SIZE*sizeof(int));
			memset(DID, 0, SIZE*sizeof(int));
		}
		sv_deptree_alloc(deptree);
	}

	if (pri < SV_DEPTREE_MAX) {
		/* handle {after,use,need} type  which insert dependencies above */
		if (type) {
			for (t = SV_SVCDEPS_AFTER; t <= SV_SVCDEPS_NEED; t++)
			TAILQ_FOREACH(ent, d->deps[t], entries) {
				if (*ent->str == '*') continue;
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
				if (*ent->str == '*') continue;
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
#ifdef DEBUG
			if (sv_debug) DBG("move : p=%-4d s=%-8s\n", prio, s);
#endif
			return prio;
		}
	/* add only if necessary */
	for (p = prio; p < deptree->size; p++)
		if (sv_stringlist_find(deptree->tree[p], s)) {
#ifdef DEBUG
			if (sv_debug) DBG("found: p=%-4d s=%-8s\n", p, s);
#endif
			return p;
		}
	prio = prio > SV_DEPTREE_MAX ? SV_DEPTREE_MAX-1 : prio;
	ent = sv_stringlist_add(deptree->tree[prio], s);
	ent->data = d;
#ifdef DEBUG
	if (sv_debug) DBG("add  : p=%-4d s=%-8s d=%p\n", prio, s, d);
#endif

	/* add the new entry to the DID table */
	for (l = 0U; l < SIZE; l++)
		if (!*(DID+l)) break;
	if (SIZE == l) {
		SIZE += 128U;
		DID = err_realloc(DID, sizeof(int)*SIZE);
		memset(DID+SIZE-128LU, 0, 128LU);
	}
	*(DID+l) = d->did;

	return prio;
}

static int sv_deptree_file_load(SV_DepTree_T *deptree)
{
	int p;
	char path[PATH_MAX];
	char *line = NULL, *ptr, *tmp;
	FILE *fp;
	size_t len = 0;

#ifdef DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, deptree);
#endif

	snprintf(path, ARRAY_SIZE(path), "%s/%s", SV_TMPDIR_DEPS, sv_init_level[sv_init]);
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

#ifdef DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, deptree);
#endif

	if (!deptree)
		return -ENOENT;

	snprintf(path, ARRAY_SIZE(path), "%s/%s", SV_TMPDIR_DEPS, sv_init_level[sv_init]);
	if ((fp = fopen(path, "w+")) == NULL) {
		ERR("Failed to open `%s': %s\n", path, strerror(errno));
		return -1;
	}

	for (p = 0; p <= deptree->prio; p++) {
		if (TAILQ_EMPTY(deptree->tree[p])) continue;
		fprintf(fp, "deps-prio_%04d=\"", p);
		TAILQ_FOREACH(ent, deptree->tree[p], entries)
			fprintf(fp, "%s ", ent->str);
		fseek(fp, -1L, SEEK_CUR);
		fprintf(fp, "\"\n");
	}
	fflush(fp);
	fclose(fp);

	return 0;
}

void svc_deptree_load(SV_DepTree_T *deptree)
{
	SV_String_T *ent;
#ifdef DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, deptree);
#endif
	TAILQ_FOREACH(ent, deptree->list, entries)
		sv_deptree_add(SV_SVCDEPS_USE, -1, ent, deptree);
}

void sv_deptree_load(SV_DepTree_T *deptree)
{
	SV_String_T *ent;
	SV_String_T *elm;
	SV_SvcDeps_T *dep;
	SV_StringList_T *list;
	int pri;

#ifdef DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, deptree);
#endif

	/* load previous deptree file if any, or initialize a new list */
	sv_deptree_file_load(deptree);
	if (!deptree->list)
		deptree->list = sv_svclist_load(NULL);
	sv_svcdeps_load(NULL);

	/* XXX: handle {after,use,need} first */
	TAILQ_FOREACH(ent, deptree->list, entries)
		sv_deptree_add(SV_SVCDEPS_USE, -1, ent, deptree);

	for (pri = deptree->size-1; pri > 0; pri--)
		if (!TAILQ_EMPTY(deptree->tree[pri])) {
			deptree->prio = pri;
			break;
		}

	/* handle SVC_{AFTER,BEFORE}="*" */
	TAILQ_FOREACH(ent, deptree->list, entries) {
		if (!(dep = ent->data)) continue;
		if ((elm = TAILQ_FIRST(dep->deps[SV_SVCDEPS_AFTER])) && (*elm->str == '*')) {
			for (pri = 0; pri < deptree->prio; pri++) {
				if ((elm = sv_stringlist_fid(deptree->tree[pri], ent))) {
					if (!pri) break;
					sv_stringlist_mov(deptree->tree[pri], deptree->tree[0], elm);
					break;
				}
			}
		}
		else if ((elm = TAILQ_FIRST(dep->deps[SV_SVCDEPS_BEFORE])) && (*elm->str == '*')) {
			for (pri = deptree->prio; pri > 0; pri--) {
				if ((elm = sv_stringlist_fid(deptree->tree[pri], ent))) {
					if ((pri == deptree->prio) && (deptree->prio == (deptree->size-1LU))) break;
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

static void sv_init_level_migrate(void)
{
	char b[128];
	char c[128] = SV_LIBDIR "/sbin/sv-config --update";
	int i, r;
	pid_t p;
	size_t l = strlen(c);
#ifdef DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif

	switch (sv_init) {
	case SV_SYSINIT_LEVEL : i = 0; break;
	case SV_SYSBOOT_LEVEL : i = 1; break;
	case SV_DEFAULT_LEVEL : i = 2; break;
	case SV_SHUTDOWN_LEVEL: i = 3; break;
	default: return;
	}

	snprintf(b, sizeof(b), "%s/.stage-%d", SV_SVCDIR, i);
	if ((r = access(b, F_OK))) {
		snprintf(b, sizeof(b), "%s/.%s", SV_SVCDIR, sv_init_level[sv_init]);
		i = access(b, F_OK);
	}
	if (!i) {
		if ((p = fork()) > 0) { /* parent */
			do {
				i = waitpid(p, &r, 0);
				if (i < 0) {
					if (errno != ECHILD) continue;
					else ERROR("%s: waitpid()", __func__);
				}
			} while (!WIFEXITED(r));
			if (WEXITSTATUS(r))
				ERR("Failed to execute `%s'\n", c);
		}
		else if (!p) { /* child */
			*(c-8U) = '\0';
			if (execl(c, strrchr(c, '/')+1, c+l-7U, NULL))
				ERROR("Failed to execute `%s'", c);
		}
		else ERROR("%s: failed to fork()", __func__);
	}
}

SV_StringList_T *sv_svclist_load(char *dir_path)
{
	char path[PATH_MAX], *ptr;
	DIR *dir;
	struct dirent *ent;
	SV_StringList_T *svclist;
#ifdef DEBUG
	if (sv_debug) DBG("%s(%s)\n", __func__, dir_path);
#endif

	/*
	 * get the service list for this stage
	 */
	if (dir_path)
		ptr = dir_path;
	else {
		ptr = path;
		sv_init_level_migrate();
		snprintf(path, sizeof(path), "%s.init.d/%s", SV_SVCDIR, sv_init_level[sv_init]);
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
	int retval, status;
	pid_t pid;
#ifdef DEBUG
	if (sv_debug) DBG("%s(%s)\n", __func__, svc);
#endif

	if ((pid = fork()) > 0) { /* parent */
		do {
			retval = waitpid(pid, &status, WUNTRACED);
			if (retval < 0) {
				if (errno == EINTR) continue;
				ERROR("%s: failed to fork()", __func__);
			}
			if (WIFSTOPPED(status)) {
				kill(pid, SIGCONT);
				continue;
			}
		} while (!WIFEXITED(status));
	}
	else if (!pid) { /* child */
		setenv("SVCDEPS_UPDATE", "1", 1);
		if (execl(SV_DEPS_SH, strrchr(SV_DEPS_SH, '/')+1, svc, NULL))
			ERROR("Failed to execute `%s'", SV_DEPS_SH);
	}
	else ERROR("%s: failed to fork()", __func__);
	return WEXITSTATUS(status);
}

#define WAIT_SVSCAN                                                      \
	if (sv_level != SV_SYSINIT_LEVEL) { /* SVSCAN */                     \
		do {                                                             \
			if (!access(pidfile, F_OK)) break;                           \
			poll(NULL, 0, 100); /* milisecond sleep */                   \
			t++;                                                         \
		} while (t < 600);                                               \
		if (t >= 600) {                                                  \
			ERR("Timed out waiting for `%s'\n", SV_INIT_SH);             \
			exit(EXIT_FAILURE);                                          \
		}                                                                \
	} else {                                                             \
		do { r = waitpid(p, &t, 0);                                      \
			if (r < 0) {                                                 \
				if (errno != ECHILD) continue;                           \
				else ERROR("%s: waitpid()", __func__);                   \
			}                                                            \
		} while (!WIFEXITED(t));                                         \
		if (!file_test(SV_TMPDIR_DEPS, 'd'))                             \
			ERROR("`%s' failed to setup `%s", SV_INIT_SH, SV_TMPDIR);    \
	}

#define ARG_OFFSET 32
#define EXEC_SVSCAN                              \
	if (sv_level != SV_SYSINIT_LEVEL)            \
		setsid();                                \
	execv(SV_INIT_SH, argv);                     \
	ERROR("Failed to execl(%s ...)", SV_INIT_SH);

SV_SvcDeps_T *sv_svcdeps_load(const char *service)
{
	char pidfile[128] =  SV_TMPDIR "/svscan.pid";
	char *ptr, *svc, *type, *line = NULL;
	char *argv[4] = { strrchr(SV_INIT_SH, '/')+1, [3] = NULL };
	FILE *fp;
	size_t len = 0, l = 0;
	int r, t = 0;
	pid_t p;
	SV_SvcDeps_T *deps = NULL;
#ifdef DEBUG
	if (sv_debug) DBG("%s(%s)\n", __func__, service);
#endif

	/* create a new list only when not updating the list */
	if (SERVICES.svcdeps) {
		if (service) {
			if ((deps = sv_svcdeps_find(service)))
				return deps;
			if (sv_svcdeps_gen(service))
				return NULL;
			l = strlen(service);
		}
		else
			return NULL;
	}
	else
		SERVICES.svcdeps = sv_svcdeps_new();

	/* initialize SV_RUNDIR and start _SVSCAN_ if necessary */
	if (access(pidfile, F_OK)) {
svscan:
		if (sv_level == SV_SYSINIT_LEVEL)
			argv[1] = "background", argv[2] = (char*)sv_init_level[sv_level];
		else
			argv[1] = "foreground", argv[2] = "svscan";
		if ((p = fork()) < 0)
			ERROR("%s: Failed to fork()", __func__);
		if (p) {
			WAIT_SVSCAN;
		}
		else {
			EXEC_SVSCAN;
		}
	}
	else {
		if ((fp = fopen(pidfile, "r"))) {
			if (fscanf(fp, "%d", &p)) {
				if (kill(p, 0)) goto svscan;
			}
			else {
#ifdef DEBUG
				if (sv_debug) DBG("Failed to read svscan pid from `%s'\n", pidfile);
#endif
				goto svscan;
			}
			fclose(fp);
		}
		else {
#ifdef DEBUG
			if (sv_debug) DBG("Failed to open svscan pidfile `%s'\n", pidfile);
#endif
			goto svscan;
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
			break;
		}
		else if (strcmp(type, "timeout") == 0) {
			errno = 0;
			deps->timeout = (int)strtol(ptr, NULL, 10);
			if (errno == ERANGE) deps->timeout = 0;
			continue;
		}
		else if (strcmp(type, "keyword") == 0) {
			if (!(ptr = strtok_r(ptr, " \t\n", &type)))
				continue;
			do {
				for (t = 1; sv_keywords[t]; t++)
					if (strcmp(ptr, sv_keywords[t]) == 0) {
						SV_KEYWORD_SET(deps, t);
						break;
					}
			} while ((ptr = strtok_r(NULL, " \t\n", &type)));
			continue;
		}

		for (t = 0; strcmp(type, sv_svcdeps_type[t]); t++)
			;
		if (t >= SV_SVCDEPS_TYPE)
			continue;
		/* append service list */
		if (!(svc = strtok_r(ptr, " \t\n", &type)))
			continue;
		do {
			sv_stringlist_add(deps->deps[t], svc);
		} while ((svc = strtok_r(NULL, " \t\n", &type)));
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
#ifdef DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif
	SV_SvcDepsList_T *list = err_malloc(sizeof(SV_SvcDepsList_T));
	TAILQ_INIT(list);
	return list;
}

static SV_SvcDeps_T *sv_svcdeps_add(const char *svc)
{
#ifdef DEBUG
	if (sv_debug) DBG("%s(%s)\n", __func__, svc);
#endif
	static unsigned int id;
	SV_SvcDeps_T *elm = err_malloc(sizeof(SV_SvcDeps_T));
	memset(elm, 0, sizeof(SV_SvcDeps_T));
	elm->did = ++id;
	elm->svc = err_strdup(svc);

	for (int i = 0; i < SV_SVCDEPS_TYPE; i++)
		elm->deps[i] = sv_stringlist_new();
	TAILQ_INSERT_TAIL(SERVICES.svcdeps, elm, entries);

	return elm;
}

static SV_SvcDeps_T *sv_svcdeps_adu(const char *svc)
{
#ifdef DEBUG
	if (sv_debug) DBG("%s(%s)\n", __func__, svc);
#endif
	SV_SvcDeps_T *elm = sv_svcdeps_find(svc);
	if (elm)
		return elm;

	return sv_svcdeps_add(svc);
}

static SV_String_T *sv_stringlist_fid(SV_StringList_T *list, SV_String_T *ent)
{
#ifdef DEBUG
	if (sv_debug) DBG("%s(%p, %p[%s])\n", __func__, list, ent, ent->str);
#endif
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
#ifdef DEBUG
	if (sv_debug) DBG("%s(%s)\n", __func__, svc);
#endif

	TAILQ_FOREACH(elm, SERVICES.svcdeps, entries)
		if (strcmp(elm->svc, svc) == 0)
			return elm;
	return NULL;
}

SV_SvcDeps_T *sv_virtsvc_find(SV_StringList_T *svclist, const char *svc)
{
	int i;
	SV_SvcDeps_T *d = NULL;
#ifdef DEBUG
	if (sv_debug) DBG("%s(%p, %s)\n", __func__, svclist, svc);
#endif

	if (!svc || !SERVICES.virt_svcdeps)
		return NULL;

	for (i = 0; i < SERVICES.virt_count; i++) {
		if (strcmp(svc, SERVICES.virt_svcdeps[i]->virt))
			continue;
		d = SERVICES.virt_svcdeps[i];
		if (!svclist)
			break;
		/* insert any provider included in the init level */
		if (sv_stringlist_find(svclist, d->svc))
			break;
	}

	return d;
}

static void sv_virtsvc_insert(SV_SvcDeps_T *elm)
{
	static size_t num;
#ifdef DEBUG
	if (sv_debug) DBG("%s(%p)\n", __func__, elm);
#endif

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
#ifdef DEBUG
	if (sv_debug) DBG("%s(void)\n", __func__);
#endif

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
