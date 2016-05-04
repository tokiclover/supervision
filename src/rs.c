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
const char *rs_stage_type[] = { "rs", "sv" };
const char *rs_stage_name[] = { "sysinit", "boot", "default", "shutdown" };
const char *rs_deps_type[] = { "after", "before", "use", "need" };
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
static char *rs_svc_cmd[] = { "stop", "start",
	"add", "del", "remove", "restart", "status", "zap"
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
static const char const *longopts_help[] = {
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

struct RS_Service_State {
	int state;
	const char *name;
};

/*static struct RS_Service_State rs_service_state = {
	{ RS_SVC_STARTED,  "star" },
	{ RS_SVC_STOPPED,  "stop" },
	{ RS_SVC_STARTING, "wait" },
	{ RS_SVC_STOPPING, "wait" },
	{ RS_SVC_UP,       "down" },
	{ RS_SVC_DOWN,     "down" },
	{ RS_SVC_FAILED,   "fail" },
	{ 0, NULL}
};*/

__NORETURN__ void rs_help_message(int exit_val);
void svc_stage(const char *cmd);
__NORETURN__ int svc_exec(int argc, char *argv[]);
int rs_svc_exec_list(RS_StringList_T *list, const char *argv[], const char *envp[]);

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

int svc_exec(int argc, char *argv[]) {
	char opt[8], *args[argc+3], *ptr, *envp[2], env[32];
	int i;
	args[0] = "runscript";

	for (i = 1; i < argc; i++)
		args[i] = argv[i-1];
	if (ptr = getenv("RS_TYPE")) {
		snprintf(opt, 8, "--%s", ptr);
		args[argc+1] = opt;
		args[argc+2] = (char *)0;
	}
	else
		args[argc+1] = (char *)0;

	snprintf(env, 32, "PRINT_COL=%d", get_term_cols());
	envp[1] = (char *)0;

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
	const char *envp[2], *argv[6] = { "runscript" };
	char opt[8], env[32];
	int i, j, k, type = 1;

	if (RS_STAGE.type == NULL) /* -r|-v passed ? */
		type = 0;
	if (command == NULL) /* start|stop passed ? */
		command = rs_svc_cmd[RS_SVC_CMD_START];

	snprintf(env, 32, "PRINT_COL=%d", get_term_cols());
	envp[0] = env, envp[1] = (char *)0;
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

	char *cmd = NULL, *opts, *svc;
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
		for (i = 0; i < ARRAY_SIZE(rs_svc_cmd); i++)
			if (strcmp(rs_svc_cmd[i], argv[optind+1]) == 0) {
				cmd = rs_svc_cmd[i];
				break;
			}
		if (cmd == NULL) {
			ERR("Invalid command.", NULL);
			exit(EXIT_FAILURE);
		}
		svc_exec(argc-optind, argv+optind);
	}

	exit(EXIT_SUCCESS);
}
