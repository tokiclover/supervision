/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision (Scripts Framework).
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 */

#include "error.h"
#include "helper.h"
#include "rs.h"
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>

#define RS_RUNSCRIPT SV_LIBDIR "/sh/runscript"

/* !!! order matter (defined constant/enumeration) !!! */
const char *const rs_stage_type[] = { "rs", "sv" };
const char *const rs_stage_name[] = { "sysinit", "boot", "default", "shutdown" };
const char *const rs_deps_type[] = { "after", "before", "use", "need" };
const char *prgname;

enum {
	RS_SVC_CMD_STOP,
	RS_SVC_CMD_START,
	RS_SVC_CMD_ADD,
	RS_SVC_CMD_DEL,
	RS_SVC_CMD_REMOVE,
	RS_SVC_CMD_RESTART,
	RS_SVC_CMD_STATUS,
	RS_SVC_CMD_ZAP
};
/* !!! likewise !!! */
static const char *const rs_svc_cmd[] = { "stop", "start",
	"add", "del", "desc", "remove", "restart", "status", "zap"
};

static const char *shortopts = "Dg0123rvh";
static const struct option longopts[] = {
	{ "nodeps",   0, NULL, 'D' },
	{ "debug",    0, NULL, 'g' },
	{ "sysinit",  0, NULL, '0' },
	{ "boot",     0, NULL, '1' },
	{ "default",  0, NULL, '2' },
	{ "shutdown", 0, NULL, '3' },
	{ "rs",       0, NULL, 'r' },
	{ "sv",       0, NULL, 'v' },
	{ "help",     0, NULL, 'h' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Disable dependencies",
	"Enable debug mode",
	"Select stage-0 run level",
	"Select stage-1 run level",
	"Select stage-2 run level",
	"Select stage-3 run level",
	"Select runscript backend",
	"Select supervision backend",
	"Show help and exit",
	NULL
};

static const char *const env_list[] = {
	"PATH", "SHELL", "SHLVL", "USER", "HOME", "TERM", "TMP", "TMPDIR",
	"LANG", "LC_ALL", "LC_ADDRESS", "LC_COLLATE", "LC_CTYPE", "LC_NUMERIC",
	"LC_MEASUREMENT", "LC_MONETARY", "LC_MESSAGES", "LC_NAME", "LC_PAPER",
	"LC_IDENTIFICATION", "LC_TELEPHONE", "LC_TIME",
	"COLUMNS", "LINES", "SVC_DEPS", "RS_DEBUG",
	NULL
};

__NORETURN__ void rs_help_message(int exit_val);
/*
 * bring system to a named level or stage
 * @cmd (start|stop|NULL); NULL is the default command
 */
void svc_stage(const char *cmd);
/*
 * execute a service with the appended arguments
 */
__NORETURN__ int svc_exec(int argc, char *argv[]);
/*
 * execute a service list (called from svc_stage())
 * @return 0 on success or number of failed services
 */
int rs_svc_exec_list(RS_StringList_T *list, const char *argv[], const char *envp[]);
/*
 * find a service
 * @return NULL if nothing found or service path (dir/file)
 */
char *rs_svc_find(const char *svc);
/*
 * generate a default environment for service
 */
const char **rs_svc_env(void);

__NORETURN__ void rs_help_message(int exit_val)
{
	int i;

	printf("usage: rs [OPTIONS] SERVICE COMMAND\n");
	printf("  COMMAND: add|del|remove|restart|start|stop|status|zap\n");
	printf("  OPTIONS: [OPTS] SERVICE COMMAND\n");
	for ( i = 0; i < 2; i++)
		printf("    -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);
	printf("  OPTIONS: [OPTS] stage [start|stop]\n");
	for ( i = 2; longopts_help[i]; i++)
		printf("    -%c, --%-12s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);

	exit(exit_val);
}

const char **rs_svc_env(void)
{
	const char **envp;
	size_t size = 1024;
	char *env = err_malloc(8);
	int i = 0, j;
	envp = err_calloc(ARRAY_SIZE(env_list), sizeof(void *));

	if (!getenv("COLUMNS")) {
		snprintf(env, sizeof(env), "%d", get_term_cols());
		setenv("COLUMNS", env, 1);
	}
	free(env);

	for (j = 0; env_list[j]; j++)
		if (getenv(env_list[j])) {
			env = err_malloc(size);
			snprintf(env, size, "%s=%s", env_list[j], getenv(env_list[j]));
			envp[i++] = err_realloc(env, strlen(env)+1);
		}

	envp[i] = (char *)0;
	return envp;
}

char *rs_svc_find(const char *svc)
{
	size_t size = 512;
	char *buf = err_malloc(size);
	int err = errno;

	if (!svc)
		return NULL;

	snprintf(buf, size, "%s/%s", RS_SVCDIR, svc);
	if (file_test(buf, 'x'))
		return err_realloc(buf, strlen(buf)+1);
	errno = 0;

	snprintf(buf, size, "%s/%s", SV_SVCDIR, svc);
	if (file_test(buf, 'd'))
		return err_realloc(buf, strlen(buf)+1);
	errno = err;

	return NULL;
}

__NORETURN__ int svc_exec(int argc, char *argv[]) {
	char opt[8];
	const char **envp;
	const char *args[argc+3], *ptr;
	int i = 0, j;
	args[i++] = "runscript";

	if (argv[0][0] == '/')
		ptr = rs_stage_type[RS_STAGE_RUNSCRIPT];
	else
		ptr = getenv("RS_TYPE");

	if (ptr == NULL) {
		ptr = rs_svc_find(argv[0]);
		if (ptr == NULL) {
			ERR("Invalid service name `%s'\n", argv[0]);
			exit(EXIT_FAILURE);
		}
		else {
			argc--, argv++;
			args[i++] = opt;
			args[i++] = ptr;
			if (strstr(ptr, RS_SVCDIR))
				ptr = rs_stage_type[RS_STAGE_RUNSCRIPT];
			else
				ptr = rs_stage_type[RS_STAGE_SUPERVISION];
			snprintf(opt, sizeof(opt), "--%s", ptr);
		}
	}

	for ( j = 0; j < argc; j++)
		args[i++] = argv[j];
	args[i] = (char *)0;
	envp = rs_svc_env();

	execve(RS_RUNSCRIPT, (char *const*)args, (char *const*)envp);
	exit(127);
}

int rs_svc_exec_list(RS_StringList_T *list, const char *argv[], const char *envp[])
{
	RS_String_T *svc;
	pid_t pid, *pidlist = NULL;
	int count = 0, status, retval = 0, i;
	static int sigsetup = 0;
	static struct sigaction sa_ign, sa_int, sa_quit;
	static sigset_t ss_child, ss_save;

	if (list == NULL) {
		errno = ENOENT;
		return -1;
	}
	if (argv == NULL || envp == NULL) {
		errno = ENOENT;
		return -1;
	}

	if (!sigsetup) {
		/* ignore SIGINT and SIGQUIT */
		sa_ign.sa_handler = SIG_IGN;
		sa_ign.sa_flags = 0;
		sigemptyset(&sa_ign.sa_mask);

		if (sigaction(SIGINT, &sa_ign, &sa_int) < 0) {
			ERROR("sigaction(SIGINT)", NULL);
			exit(EXIT_FAILURE);
		}
		if (sigaction(SIGQUIT, &sa_ign, &sa_quit) < 0) {
			ERROR("sigaction(SIGQUIT)", NULL);
			exit(EXIT_FAILURE);
		}
		sigemptyset(&ss_child);
		sigaddset(&ss_child, SIGCHLD);
		sigsetup = 1;

		/* block SIGCHLD */
		if (sigprocmask(SIG_BLOCK, &ss_child, &ss_save) < 0) {
			ERROR("sigprocmask(SIG_BLOCK)", NULL);
			exit(EXIT_FAILURE);
		}
	}

	SLIST_FOREACH(svc, list, entries) {
		count += 1;
		pidlist = err_realloc(pidlist, sizeof(pid_t) * count);
		pid = fork();

		if (pid > 0) /* parent */
			pidlist[count-1] = pid;
		else if (pid == 0) { /* child */
			/* restore previous signal actions and mask */
			sigaction(SIGINT, &sa_int, NULL);
			sigaction(SIGQUIT, &sa_quit, NULL);
			sigprocmask(SIG_SETMASK, &ss_save, NULL);

			argv[2] = svc->str;
			execve(RS_RUNSCRIPT, (char *const*)argv, (char *const*)envp);
			_exit(127);
		}
		else
			ERROR("fork", NULL);
	}

	for (i = 0; i < count; i++) {
		waitpid(pidlist[i], &status, 0);
		if (!WIFEXITED(status))
			retval += 1;
	}
	free(pidlist);

	return count;
}

void svc_stage(const char *cmd)
{
	RS_STAGE.level = atoi(getenv("RS_STAGE"));
	RS_STAGE.type  = getenv("RS_TYPE");
	RS_DepTypeList_T *depends;
	RS_DepType_T *elm, *nil = NULL;
	RS_String_T *svc;
	const char *command = cmd;
	const char **envp;
	const char *argv[6] = { "runscript" };
	char opt[8];
	int i, j, k, type = 1;

	if (RS_STAGE.type == NULL) /* -r|-v passed ? */
		type = 0;
	if (command == NULL) /* start|stop passed ? */
		command = rs_svc_cmd[RS_SVC_CMD_START];

	envp = rs_svc_env();
	argv[1] = opt, argv[4] = (char *)0, argv[3] = command;

	for (k = 0; k < ARRAY_SIZE(rs_stage_type); k++) {
		if (!type)
			RS_STAGE.type = rs_stage_type[k];
		snprintf(opt, 8, "--%s", RS_STAGE.type);
		depends = rs_deplist_load();

		if (strcmp(command, rs_svc_cmd[RS_SVC_CMD_START]) == 0) { /* start */
			for (i = RS_DEPS_TYPE-1; i > 0; i--)
				if ((elm = rs_deplist_find(depends, rs_deps_type[i])) != NULL) {
					if (nil == NULL)
						nil = elm;
					for (j = RS_DEP_PRIORITY-1; j > 0; j--) /* high prio only */
						rs_svc_exec_list(elm->priority[j], argv, envp);
				}
			rs_svc_exec_list(nil->priority[0], argv, envp);
		}
		else if (strcmp(command, rs_svc_cmd[RS_SVC_CMD_STOP]) == 0) { /* stop */
			for (i = 0; i < RS_DEPS_TYPE; i++)
				if ((elm = rs_deplist_find(depends, rs_deps_type[i])) != NULL) {
					if (nil == NULL)
						nil = elm;
					for (j = 1; j < RS_DEP_PRIORITY; j++)
						rs_svc_exec_list(elm->priority[j], argv, envp);
				}
			rs_svc_exec_list(nil->priority[0], argv, envp);
		}
		rs_deplist_free(depends);

		if (type)
			break;
	}
}

int main(int argc, char *argv[])
{
	prgname = argv[0]+strlen(argv[0])-1;
	for ( ; *prgname != '/'; prgname--)
		;
	prgname++;

	const char *cmd = NULL;
	char *opts;
	int i, opt;

	/* Show help if insufficient args */
	if (argc < 2) {
		rs_help_message(1);
	}

	/* Parse options */
	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
	{
		switch (opt) {
			case 'D':
				setenv("SVC_DEPS", "0", 1);
				break;
			case 'g':
				setenv("RS_DEBUG", "1", 1);
				break;
			case '0':
			case '1':
			case '2':
			case '3':
				setenv("RS_STAGE", argv[optind-1]+1, 1);
				break;
			case 'r':
				setenv("RS_TYPE", rs_stage_type[RS_STAGE_RUNSCRIPT], 1);
				break;
			case 'v':
				setenv("RS_TYPE", rs_stage_type[RS_STAGE_SUPERVISION], 1);
				break;
			case '?':
			case 'h':
				rs_help_message(0);
				break;
			default:
				rs_help_message(1);
				break;
		}
	}

	if (strcmp(argv[optind], "stage") == 0) {
		if (getenv("RS_STAGE"))
			svc_stage(argv[optind+1]);
		else {
			fprintf(stderr, "Usage: %s -(0|1|2|3) [-r|-v] stage [start|stop]"
					"(level argument required)\n", prgname);
			exit(EXIT_FAILURE);
		}
	}
	else {
		if ((argc-optind) < 2) {
			fprintf(stderr, "Usage: %s [OPTIONS] SERVICE COMMAND [ARGS]\n",
					prgname);
			exit(EXIT_FAILURE);
		}

		for (i = 0; i < ARRAY_SIZE(rs_svc_cmd); i++)
			if (strcmp(rs_svc_cmd[i], argv[optind+1]) == 0) {
				cmd = rs_svc_cmd[i];
				break;
			}

		if (cmd == NULL) {
			ERR("Invalid command.\n", NULL);
			exit(EXIT_FAILURE);
		}

		svc_exec(argc-optind, argv+optind);
	}

	exit(EXIT_SUCCESS);
}
