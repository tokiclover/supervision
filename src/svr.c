/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)svr.c  0.15.0 2019/03/14
 */

#include <dirent.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include "config.h"
#include "error.h"
#include "helper.h"
#include "sv-copyright.h"
#include "supervision.h"
#include "timespec.h"
#include "svd.h"

#define SV_VERSION "0.15.0"

#define ST_PRINT(ST, COLOR) printf("%s" ST "%s: ", print_color(COLOR, COLOR_FG), print_color(COLOR_RST, COLOR_RST))

/*enum {
	SW_DOWN   = 0x00,
#define SW_DOWN SW_DOWN
	SW_PAUSE  = 0x01,
#define SW_PAUSE SW_PAUSE
	SW_TERM   = 0x02,
#define SW_TERM SW_TERM
	SW_RUN    = 0x04,
#define SW_RUN SW_RUN
	SW_FINISH = 0x08,
#define SW_FINISH SW_FINISH
};

static struct sdent {
	int sd_pid, sd_stat, sd_ctrl, sd_want, sd_ret;
	int fd_lock, fd_ctrl, fd_status, sv_run, sv_log;
	void *fp_lock, *fp_ctrl, *fp_status;
	char *sv_pid, *sv_stat, *sv_status, *sv_control, *sv_lock, *sv_ok;
	struct timespec sv_time;
} sde[2] = {
	[0] = {
		.sd_want = SW_RUN,
		.sd_ctrl = SW_RUN,
		.sv_control = "supervise/control",
		.sv_pid     = "supervise/pid",
		.sv_status  = "supervise/status",
		.sv_stat    = "supervise/stat",
		.sv_lock    = "supervise/lock",
		.sv_ok      = "supervise/ok",
	},
	[1] = {
		.sd_ctrl = SW_RUN,
		.sv_control = "log/supervise/control",
		.sv_pid     = "log/supervise/pid",
		.sv_stat    = "log/supervise/stat",
		.sv_status  = "log/supervise/status",
		.sv_lock    = "log/supervise/lock",
		.sv_ok      = "log/supervise/ok",
	}
}, *sd_svc = &sde[0], *sd_log = &sde[1];*/

static void svd(void);
static void svr_sighandler(int sig);
__attribute__((format(printf,2,3))) void err_print(int priority, const char *fmt, ...);
__attribute__((__unused__)) static int svr_stat(struct sdent *restrict sd);
__attribute__((__unused__)) static int svr_status_print(struct sdent *restrict sd);
__attribute__((__unused__)) static int svr_ctrl (char *restrict s);
__attribute__((__unused__)) static int svr_check(char *restrict s);
__attribute__((__unused__)) static int svr_status   (__attribute__((__unused__)) char *s);
__attribute__((__unused__)) static int svr_check_usr(__attribute__((__unused__)) char *restrict s);

extern char **environ;
const char *progname;
static int (*sv_cmd) (char *cmd), (*sv_check)(char *cmd);
static char *command, *cmd, *svc;
static struct timespec ts_now, ts_cmd, ts_res, ts_rem;
static int opt, sid;
static time_t t_time;
static long int t_res;
static int fd;

static const char *shortopts = "dhsvw:";
static const struct option longopts[] = {
	{ "wait"   , 0, NULL, 'w' },
	{ "debug"  , 0, NULL, 'd' },
	{ "sid"    , 0, NULL, 's' },
	{ "help"   , 0, NULL, 'h' },
	{ "version", 0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"Set timeout value",
	"Enable debug mode",
	"Set session identity",
	"Print help message",
	"Print version string",
	NULL
};

__attribute__((__noreturn__)) static void help_message(int retval)
{
	int i = 1;
	printf("Usage: %s [-s|--sid] [-w|--wait SECONDS] COMMAND SERVICE[S]\n", progname);
	printf("    -%c, --%-8s SEC   %s\n", longopts[0].val, longopts[0].name,
			longopts_help[0]);
	for ( ; longopts_help[i]; i++)
		printf("    -%c, --%-14s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);
	exit(retval);
}

__attribute__((format(printf,2,3))) void err_print(int priority, const char *fmt, ...)
{
	va_list ap;

	switch (priority) {
	case LOG_EMERG:
	case LOG_ALERT:
	case LOG_CRIT:
	case LOG_ERR:
		fprintf(stderr, "%s: %serror%s: ", progname,
				print_color(COLOR_RED, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST));
		break;
	case LOG_WARNING:
 		fprintf(stderr, "%s: %swarning%s: ", progname, 
				print_color(COLOR_YLW, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST));
		break;
	case LOG_NOTICE:
	case LOG_INFO:
 		fprintf(stderr, "%s: %sinfo%s: ", progname, 
				print_color(COLOR_BLU, COLOR_FG),
				print_color(COLOR_RST, COLOR_RST));
		break;
	case  LOG_DEBUG:
		if (!ERR_debug) return;
		fprintf(stderr, "%s: debug: ", progname);
		break;
	}
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

static void svr_sighandler(int sig)
{
	int serrno = errno;

	err_print(LOG_ERR, "caught unhandled `%s' signal", strsignal(sig));
	exit(EXIT_FAILURE);

	errno = serrno;
}

__attribute__((__unused__)) static int svr_check_usr(__attribute__((__unused__)) char *restrict s)
{
	char *argv[4] = { "./check", s, NULL };
	int child, rv, status;

	if (access(*argv+2LU, R_OK | X_OK)) {
		if (errno == ENOENT) return 0;
		ERR("%s: cannot stat `%s': %s\n", progname, svc, strerror(errno));
		return -ENOENT;
	}

	child = fork();
	if (child == -1) {
		err_print(LOG_DEBUG, "%s: cannot fork: %s", strerror(errno));
		return -1;
	}
	/*parent*/
	if (child) {
		rv = waitpid(child, &status, 0);
		if (rv == -1) {
			ERR("%s: cannot wait (pid=%d) child: %s\n", svc, child, strerror(errno));
			return 1;
		}
		return WEXITSTATUS(status);
	}
	/*child*/
	(void)close(STDOUT_FILENO);
	execve(*argv, argv, environ);
	ERROR("Failed to execute `%s'", argv[1]);
}

__attribute__((__unused__)) static int svr_check(char *restrict s)
{
	register int i = 0, r = svr_stat(sd_svc);

	switch (r)
	{
	case  1:
		if (*s == 'x') return 0;
	default: if (r < 0) return r;
	}

	while (s[i++])
	{
		switch (s[i])
		{
		case 'x': return 1;
		case 'u':
			if (!sd_svc->sd_pid || !(sd_svc->sd_stat == SW_RUN)) return 1;
			return svr_check_usr(s);
		case 'd':
			if (sd_svc->sd_pid || sd_svc->sd_stat) return 1;
			break;
		case 'C':
			if (sd_svc->sd_pid) return svr_check_usr(s);
			break;
		case 't':
		case 'k':
			if (!sd_svc->sd_pid && !sd_svc->sd_stat) break;
			if (!sd_svc->sd_pid || (ts_cmd.tv_sec > sd_svc->sv_time.tv_sec) || st_buf[18] ||
					svr_check_usr(s))
				return 1;
			break;
		case 'o':
			if ((!sd_svc->sd_pid && (ts_cmd.tv_sec > sd_svc->sv_time.tv_sec)) ||
				(sd_svc->sd_pid && !sd_svc->sd_stat))
				return 1;
			break;
		case 'p':
			if (sd_svc->sd_pid && !st_buf[16]) return 1;
			break;
		case 'c':
			if (sd_svc->sd_pid &&  st_buf[16]) return 1;
			break;
		}
	}

	ST_PRINT("ok", COLOR_BLU);
	svr_status(s);

	return 0;
}

__attribute__((__unused__)) static void svd(void)
{
	int rv;
	char bf[32] = "../" SV_FIFO;
	char bp[32] = "../" SV_PIDFILE;
	char *pt, *pc;
	FILE *fp;
	char *argv[4] = { SVD };

	if (!access(sd_svc->sv_lock, F_OK) || !access(sd_svc->sv_status, F_OK))
		return;

	if (sid) {
		argv[1] = "-s"; argv[2] = svc; argv[3] = NULL;
	}
	else {
		argv[1] = svc; argv[2] = NULL;
	}

	fd = -1;
	if (!access(bf, F_OK)) {
		if (!(pt = strrchr(svc, '/'))) pt = svc;
		if ((fd = open(bf, O_WRONLY | O_NDELAY | O_CLOEXEC | O_NONBLOCK)) != -1) {
			pc = pt;
			if (sid) {
				pc = err_malloc(strlen(pt)+4LU);
				sprintf(pc, "%s:%s", pt, argv[1]);
			}
			err_write(fd, pc, bf);
		}
	}
	if ((fd == -1) && !access(bp, F_OK)) {
		if ((fd = open(bp, O_RDONLY | O_NDELAY | O_CLOEXEC)) != -1) {
			fp = fdopen(fd, "r");
			if (fscanf(fp, "%d", &rv)) {
				if (sid) kill(rv, SIGUSR1);
				else kill(rv, SIGUSR2);
			}
		}
	}
	else {
		rv = fork();
		if (!rv) {
			execve(*argv, argv, environ);
			ERROR("Failed to execute `%s'", SVD);
		}
		if (rv == -1) ERR("cannot `fork': %s\n", strerror(errno));
	}
}

__attribute__((__unused__)) static int svr_ctrl(char *restrict s)
{
	int rv = svr_stat(sd_svc);

	switch (*s)
	{
	case 'u': if (rv) svd();
	default:  if (rv) return rv;
	}

	if (st_buf[17] == *s)
		if ((*s != 'd') || (st_buf[18] == 1)) return 0;
	if ((fd = open(sd_svc->sv_control, O_WRONLY | O_NDELAY, 0600)) == -1) {
		if (errno == ENOENT)
			ERR("%s: cannot open `%s'!\n", svc, sd_svc->sv_control);
		else {
			if (*s == 'x') ST_PRINT("ok", COLOR_BLU);
			else           ST_PRINT("no", COLOR_RED);
			printf("\n");
			ERR("%s: `svd' is not running\n", svc);
		}
		return -1;
	}

	rv = write(fd, s, strlen(s));
	(void)close(fd);
	if (rv < 1) {
		ERR("%s: cannot write `%s' command.\n", svc, s);
		return -1;
	}

	return 0;
}

__attribute__((__unused__)) static int svr_stat(struct sdent *restrict sd)
{
	register unsigned int o, l;
	int rv, pid;
	FILE *fp;

	if (sd->fd_status) return 0;

	if ((fd = open(sd->sv_lock, O_RDONLY | O_NDELAY)) == -1) {
svd_err:
		printf("%s[%s%s%s]%s `%s' is not running!\n",
				print_color(COLOR_RED, COLOR_FG), print_color(COLOR_RST, COLOR_RST),
				sd == sd_log ? "log" : svc,
				print_color(COLOR_RED, COLOR_FG), print_color(COLOR_RST, COLOR_RST),
				sd == sd_log ? "svl" : "svd");
		return 1;
	}
	fp = fdopen(fd, "r");
	if (!fscanf(fp, "%d", &pid)) goto svd_err;
	if (kill(pid, 0)) goto svd_err;
	(void)close(fd);

	if ((sd->fd_status = open(sd->sv_status, O_RDONLY | O_NDELAY)) == -1) {
		ERR("%s: cannot open `%s': %s", svc, sd_svc->sv_status, strerror(errno));
		return -1;
	}
	rv = read(sd->fd_status, st_buf, sizeof(st_buf));
	if (rv == -1) {
		ERR("%s: cannot read `%s': %s", svc, sd->sv_status, strerror(errno));
		return -1;
	}
	if (rv != sizeof(st_buf)) {
		ERR("%s: cannot read `%s': erronous lenght", svc, sd->sv_status);
		return 1;
	}

	/*supervise(1) compatibility*/
	sd->sv_time.tv_sec = timespec_unpack_sec(st_buf); 
	l = o = TIMESPEC_SIZE;
	l += 4LU; /*sizeof(pid_t)*/
	sd->sd_pid = (unsigned char)st_buf[o];
	while (++o < l) sd->sd_pid <<= CHAR_BIT, sd->sd_pid += (unsigned char)st_buf[o];

	if (st_buf[ST_OFF_PAUSE]) sd->sd_ctrl &= SW_PAUSE;
	sd->sd_want = st_buf[ST_OFF_UP] ? SW_RUN : SW_DOWN;
	if (st_buf[ST_OFF_TERM ]) sd->sd_ctrl &= SW_TERM;
	switch (st_buf[ST_OFF_RUN]) {
	case ST_RUN   : sd->sd_stat = SW_RUN   ; break;
	case ST_FINISH: sd->sd_stat = SW_FINISH; break;
	default       : sd->sd_stat = SW_DOWN  ;
	}

	return 0;
}

__attribute__((__unused__)) static int svr_status_print(struct sdent *restrict sd)
{
	register int up = 0;

	if (access("down", F_OK)) {
		if (errno != ENOENT) {
			ERR("%s: cannot stat `%s': %s\n", svc, "down", strerror(errno));
			return 0;
		}
		up++;
	}

	     if (sd->sd_stat & SW_RUN)    ST_PRINT("run"   , COLOR_GRN);
	else if (sd->sd_stat & SW_FINISH) ST_PRINT("finish", COLOR_BLU);
	else ST_PRINT("down", COLOR_RED);
	if (sd->sv_log)
		printf(" %s[%slog%s]%s: ",
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_RST, COLOR_RST),
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_RST, COLOR_RST));
	else
		printf("%-32s ", svc);
	if (st_buf[ST_OFF_RUN])
		printf("%s{%spid=%-4d %8lds%s}%s",
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_RST, COLOR_RST),
				sd->sd_pid, ts_now.tv_sec < sd->sv_time.tv_sec ? 0LU : (ts_now.tv_sec - sd->sv_time.tv_sec),
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_RST, COLOR_RST));
	if (sd->sd_pid && !up)
		printf("%s(%sdown%s)%s ",
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_YLW, COLOR_RST),
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_RST, COLOR_RST));
	if (!sd->sd_pid && up)
		printf("%s(%sup%s)%s   ",
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_YLW, COLOR_RST),
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_RST, COLOR_RST));
	if (sd->sd_pid && st_buf[ST_OFF_PAUSE])
		printf("%s(%spause%s)%s",
				print_color(COLOR_MAG, COLOR_FG), print_color(COLOR_MAG, COLOR_FG),
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_RST, COLOR_RST));
	if (!sd->sd_pid && st_buf[ST_OFF_UP] == ST_WANT_UP  )
		printf(" %s[%swant up%s]%s",
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_YLW, COLOR_FG),
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_RST, COLOR_RST));
	if ( sd->sd_pid && st_buf[ST_OFF_UP] == ST_WANT_DOWN)
		printf(" %s[%swant down%s]%s",
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_YLW, COLOR_FG),
				print_color(COLOR_MAG, COLOR_FG), print_color(COLOR_YLW, COLOR_FG));
	if (sd->sd_pid && st_buf[ST_OFF_TERM])
		printf(" %s[%swant term%s]%s",
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_RED, COLOR_FG),
				print_color(COLOR_CYN, COLOR_FG), print_color(COLOR_RST, COLOR_RST));

	return sd->sd_pid ? 0 : 1;
}


__attribute__((__unused__)) static int svr_status(__attribute__((__unused__)) char *s)
{
	int rc = svr_stat(sd_svc);
	if (rc) return rc;

	rc = svr_status_print(sd_svc);
	if (chdir("log")) {
		if (errno != ENOENT)
			fprintf(stderr, "%serror%s: %s: cannot chdir(`%s'): %s",
				print_color(COLOR_RED, COLOR_FG), print_color(COLOR_RST, COLOR_RST),
				svc, "log", strerror(errno));
	}
	else {
		printf(" ");
		if (!svr_stat(sd_log)) rc += svr_status_print(sd_log);
	}
	printf("\n"); fflush(stdout);

	return rc;
}

int main(int argc, char *argv[])
{
	int rc, rv, i;
	int *sig = (int []) { SIGHUP, SIGINT, SIGTERM, SIGQUIT, SIGUSR1, SIGUSR2, SIGALRM, 0 };
	struct sigaction action;

	progname = strrchr(argv[0], '/');
	if (!progname)
		progname = argv[0];
	else
		progname++;

	if (argc < 2)
		help_message(1);

	/* Parse options */
	while ((rv = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
	{
		switch (rv)
		{
			case 'd': ERR_debug++; break;
			case 's': sid++      ; break;
			case 'w':
				errno = 0;
				t_time = strtoul(optarg, NULL, 10);
				if (errno == ERANGE) {
					ERR("invalide `%s' timeout!\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
			case 'v':
				printf("%s version %s\n\n", progname, SV_VERSION);
				puts(SV_COPYRIGHT);
				exit(EXIT_SUCCESS);
			case 'h':
				help_message(EXIT_SUCCESS);
				break;
			default:
				help_message(EXIT_FAILURE);
				break;
		}
	}
	argc -= optind, argv += optind;

	if (argc < 2) {
		ERR("missing `COMMAND SERVICE' arguments\n", NULL);
		exit(EXIT_FAILURE);
	}

	/* set up signal handler */
	memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_handler = svr_sighandler;
	action.sa_flags = SA_RESTART;
	for ( ; *sig; sig++)
	{
		if (sigaction(*sig, &action, NULL))
			err_print(LOG_ERR, "Failed to register `%s' signal: %s",
					strsignal(*sig), strerror(errno));
	}

	cmd = *argv; argc--; argv++;
	sv_cmd = svr_ctrl;
	switch (*cmd)
	{
	case 'x': case 'e':     command = "x"; break;
	case 'X': case 'E':     command = "x";
	case 'D': if (!command) command = "d";
	case 'T': if (!command) command = "tc";
		opt--; sv_check = svr_check; break;
	case 't':
		if (!strcmp(cmd, "try-restart")) {
			command = "tc"; sv_check = svr_check;
		}
		else {
			command = cmd; cmd[1] = '\0';
		}
		break;
	case 'c':
		command = cmd[1] == 'o' ? "c" : "C";
		break;
	case 'a': case 'u': case 'd': case 'o': case 'p': case 'h':
	case 'i': case 'k': case 'q': case '1': case '2':
		command = cmd; cmd[1] = '\0';
		break;
	case 's':
		if (!t_time) t_time += 10U;
		sv_check = svr_check;
		     if (!strcmp(cmd, "start"   )) command = "u";
		else if (!strcmp(cmd, "stop"    )) command = "d";
		else if (!strcmp(cmd, "status"  )) {
			command = "S"; sv_cmd = svr_status; sv_check = NULL;
		}
		else if (!strcmp(cmd, "shutdown")) command = "x";
		else goto invalid_command;
		break;
	case 'r':
		     if (!strcmp(cmd, "reload"  )) command = "r";
		else if (!strcmp(cmd, "restatrt")) command = "tcu";
		else goto invalid_command;
		sv_check = svr_check;
		break;
	case 'f':
		     if (!strcmp(cmd, "force-reload"  )) command = "tc" , opt++;
		else if (!strcmp(cmd, "force-restart" )) command = "tcu", opt++;
		else if (!strcmp(cmd, "force-shutdown")) command = "x"  , opt--;
		else if (!strcmp(cmd, "force-stop"    )) command = "d"  , opt--;
		else goto invalid_command;
		if (!t_time) t_time += 10U;
		sv_check = svr_check;
		break;
	default: goto invalid_command;
	}
	if (!command) goto invalid_command;

	rc = 0;
	TIMESPEC(&ts_now);
	/* service command */
	for (i = 0; i < argc; i++)
	{
		svc = argv[i];
		if (*svc == '.'      ) goto invalid_service;
		if (strlen(svc) < 1LU) goto invalid_service;

		if (*svc == '/') {
			if (chdir(svc))
				ERROR("cannot change current directory to `%s'", svc);
		}
		else {
			if (chdir(SV_RUNDIR))
				ERROR("cannot change current directory to `%s'", SV_RUNDIR);
		}

		TIMESPEC(&ts_cmd);
		sv_cmd(command);
	}
	if (!sv_check) goto return_status;

	/* sleep resolution */
	ts_rem = ts_cmd;
	clock_getres(CLOCK_REALTIME, &ts_res);
	t_res = 250000000LU / ts_res.tv_nsec;
	ts_res.tv_nsec *= t_res;
	if (t_time) ts_rem.tv_sec += t_time;

	/* check service */
	for (i = 0; i < argc; i++)
	{
		if (sd_svc->fd_status) {
			(void)close(sd_svc->fd_status);
			sd_svc->fd_status = 0;
		}
		svc = argv[i];
		ts_cmd = ts_now;

		if (*svc == '/') {
			if (chdir(svc))
				ERROR("cannot change current directory to `%s'", svc);
		}
		else {
			if (chdir(SV_RUNDIR))
				ERROR("cannot change current directory to `%s'", SV_RUNDIR);
		}

		while ((rv = sv_check(command)))
		{
			if (timespec_cmp(&ts_rem, &ts_cmd) < 1) break;
			nanosleep(&ts_res, NULL);
			TIMESPEC(&ts_now);
			timespec_add(&ts_cmd, &ts_now, &ts_res);
		}
			
		if (rv) {
			rc++;
			if (!svr_stat(sd_svc)) {
				svr_status_print(sd_svc);
				printf("\n"); fflush(stdout);
			}
			if (opt < 0) svr_ctrl("k");
		}
	}

return_status:
	return rc > 255 ? 255 : rc;

invalid_command:
	ERR("invalid `%s' command!\n", cmd);
	exit(EXIT_FAILURE);
invalid_service:
	ERR("invalid `%s' service!", svc);
	exit(EXIT_FAILURE);
}
