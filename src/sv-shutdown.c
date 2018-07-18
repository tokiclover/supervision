/*
 * Utility providing an interface {halt,reboot,shutdown,poweroff}
 * per supervision backend.
 *
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-shutdown.c  0.14.0 2018/07/10
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <utmpx.h>
#include <getopt.h>
#include <fcntl.h>
#include <paths.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/time.h>
#include "error.h"
#include "helper.h"
#ifdef HAVE_POSIX_ASYNCHRONOUS_IO
# include <aio.h>
static int aiocb_count;
#endif

#if defined (__linux__) || (defined(__FreeBSD_kernel__) && \
		defined(__GLIBC__)) || defined(__GNU__)
# define RA_HALT     RB_HALT_SYSTEM
# define RA_POWEROFF RB_POWER_OFF
#elif defined(__DragonFly__) || defined(__FreeBSD__) || \
	defined(__OpenBSD__) || defined(__NetBSD__) || defined(BSD)
# define RA_HALT     RB_HALT
# define RA_POWEROFF RB_POWEROFF
#else
# error "Unsupported Operating System"
#endif

#ifndef UT_LINESIZE
# ifdef __UT_LINESIZE
#  define UT_LINESIZE __UT_LINESIZE
# else
#  define UT_LINESIZE 32
# endif
#endif

#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX MAXHOSTNAMELEN
#endif

#define VERSION "0.13.0"

#ifndef LIBDIR
# define LIBDIR "/lib"
#endif
#define SV_CONFIG LIBDIR "/sv/sh/SV-CONFIG"
#define SD_PIDFILE _PATH_VARRUN "sv-shutdown.pid"

#ifdef SV_DEBUG
# undef  _PATH_NOLOGIN
# define _PATH_NOLOGIN "./nologin"
# define BOOTFILE "./fastboot"
# define FSCKFILE "./forcefsck"
#else
# define BOOTFILE "/fastboot"
# define FSCKFILE "/forcefsck"
#endif

#define SD_POWEROFF 0
#define SD_SINGLE   1
#define SD_REBOOT   6
#define SD_CANCEL   7
#define SD_MESSAGE  8

#define SD_ACTION_SHUTDOWN 0
#define SD_ACTION_SINGLE   1
#define SD_ACTION_HALT     2
#define SD_ACTION_POWEROFF 3
#define SD_ACTION_REBOOT   4

#define H       *60L*60L
#define M           *60L
#define NOLOGIN_TIME 5L*60L
static struct timeinterval {
	long unsigned int ti_left, ti_wait;
} timelist[] = {
	{ 10L H,  5L H },
	{  5L H,  3L H },
	{  2L H,  1L H },
	{  1L H, 30L M },
	{ 30L M, 10L M },
	{ 20L M, 10L M },
	{ 10L M,  5L M },
	{  5L M,  3L M },
	{  2L M,  1L M },
	{  1L M, 30L   },
	{ 30L  , 30L   },
	{  0L  ,  0L   },
};
#undef H
#undef M

__attribute__((__noreturn__)) static void help_message(int status);
static void sighandler(int sig, siginfo_t *info, void *ctx __attribute__((__unused__)));
static int sigsetup(void);
static int sv_nologin(void);
static void sv_timewarn(unsigned long int timeleft);

extern char **environ;

const char *progname;
static char message[BUFSIZ];
static int reboot_action;
static int reboot_force;
static int reboot_sync = 1;
static int boot_flag, fsck_flag, slog_flag;
static int shutdown_action = -1;
static char hostname[HOST_NAME_MAX+1];
static char *whom;
static int ai;
static const char *action[] = { "shutdown", "single", "halt", "poweroff", "reboot" };
static const char signame[][8] = { "SIGINT", "SIGTERM", "SIGQUIT", "SIGUSR1",
	"SIGUSR2", "SIGALRM" };
static time_t offtime, shuttime;
#define TIMER_LEN 12U
static size_t timer_pos, message_len;
static jmp_buf alarmbuf;

static char *restricted_environ[] = {
	"PATH=" _PATH_STDPATH ":" LIBDIR "/sv/bin:" LIBDIR "/sv/sbin",
	NULL
};

static const char *shortopts = "06crshpfFHPlmnquv";
static const struct option longopts[] = {
	{ "reboot",   0, NULL, 'r' },
	{ "shutdown", 0, NULL, 's' },
	{ "halt",     0, NULL, 'h' },
	{ "poweroff", 0, NULL, 'p' },
	{ "fast",     0, NULL, 'f' },
	{ "fsck",     0, NULL, 'F' },
	{ "force",    0, NULL, 'q' },
	{ "nosync",   0, NULL, 'n' },
	{ "cancel",   0, NULL, 'c' },
	{ "syslog",   0, NULL, 'l' },
	{ "message",  0, NULL, 'm' },
	{ "usage",    0, NULL, 'u' },
	{ "version",  0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};
static const char *longopts_help[] = {
	"System reboot",
	"System shutdown (-0 alias)",
	"System halt",
	"System poweroff (-0 alias)",
	"Fast boot, skip fsck(8) on reboot",
	"Force fsck(8) on reboot",
	"Force halt/reboot/poweroff",
	"Disable filesystem synchronizations",
	"Cancel a waiting shutdown process",
	"Log shutdown action to system log",
	"Broadcast message only",
	"Print help massage",
	"Print version string",
	NULL
};

__attribute__((__noreturn__)) static void usage_message(unsigned int index)
{
	ERR("invalid %s usage -- %s\n", action[SD_ACTION_SHUTDOWN], action[ai]);
	printf("Usage: %s [-q] [-p] [-n] [time] [message]\n", action[index]);
	exit(EXIT_FAILURE);
}
__attribute__((__noreturn__)) static void help_message(int status)
{
	int i = 0;

	printf("Usage: %s [OPTIONS] (ACTION) (TIME) [MESSAGE]\n", progname);
	printf("    -6, -%c, --%-16s %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	printf("    -0, -%c, --%-16s %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	printf("    -H, -%c, --%-16s %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	printf("    -P, -%c, --%-16s %s\n", longopts[i].val, longopts[i].name,
		longopts_help[i]);
	i++;
	for ( ; longopts_help[i]; i++)
		printf("        -%c, --%-16s %s\n", longopts[i].val, longopts[i].name,
				longopts_help[i]);

	exit(status);
}

static void sighandler(int sig, siginfo_t *si, void *ctx __attribute__((__unused__)))
{
#ifdef SV_DEBUG
	DBG("%s(%d, %p, %p)\n", __func__, sig, si, ctx);
#endif
	int i = -1;
	int serrno = errno;
#ifdef HAVE_POSIX_ASYNCHRONOUS_IO
	struct aiocb *acp;
#endif

	switch(sig) {
	case SIGALRM:
		longjmp(alarmbuf, 1);
		break;
	case SIGINT:
		i = 0;
	case SIGTERM:
		if (i < 0) i = 1;
	case SIGQUIT:
		if (i < 0) i = 2;
	case SIGUSR1:
		if (i < 0) i = 3;
		WARN("Caught %s signal...\n", signame[i]);
		if (!access(BOOTFILE     , F_OK)) unlink(BOOTFILE);
		if (!access(FSCKFILE     , F_OK)) unlink(FSCKFILE);
		if (!access(_PATH_NOLOGIN, F_OK)) unlink(_PATH_NOLOGIN);
		if (!access(SD_PIDFILE   , F_OK)) unlink(SD_PIDFILE);
		fprintf(stderr, "%s: cancelling system %s\n", progname, action[ai]);
		exit(EXIT_FAILURE);
		break;
	case SIGUSR2:
		;
#ifdef HAVE_POSIX_ASYNCHRONOUS_IO
		acp = si->si_value.sival_ptr;
		if (si->si_code == SI_ASYNCIO && acp && acp->aio_fildes > 0) {
			(void)close(acp->aio_fildes);
			acp->aio_fildes = aio_return(acp);
			if (acp->aio_fildes > 0)
				acp->aio_fildes = 0;
			else
				acp->aio_fildes = -1;
			acp->aio_lio_opcode = LIO_NOP;
			aiocb_count--;
		}
#endif
		break;
	default:
		WARN("Caught unhandled signal %d ...\n", sig);
	}
	errno = serrno;
}
static int sigsetup(void)
{
	struct sigaction act;
	int sigvalue[] = { SIGINT, SIGTERM, SIGQUIT, SIGUSR1, SIGUSR2, SIGALRM, 0 };
	int i;
#ifdef SV_DEBUG
	DBG("%s(void)\n", __func__);
#endif

	act.sa_sigaction = sighandler;
	act.sa_flags = SA_SIGINFO | SA_RESTART;
	sigemptyset(&act.sa_mask);
	for (i = 0; sigvalue[i]; i++)
		if (sigaction(sigvalue[i], &act, NULL) < 0)
			ERROR("sigaction(%s, ...)\n", signame[i]);
	return 0;
}

static int sv_wall(void)
{
#ifdef SV_DEBUG
	DBG("%s(void)\n", __func__);
#endif
	int i;
#ifdef _PATH_WALL
# if defined(__DragonFly__) || defined(__FreeBSD__)
#  define WALL_CMD _PATH_WALL " -n" /* undocumented option to suppress banner */
# endif
	FILE *fp;
	if ((fp = popen(WALL_CMD, "w"))) {
		i = fwrite((const void*)message, sizeof(*message), message_len, fp);
		fclose(fp);
		return i;
	}
	else
		ERR("Failed to open `%s': %s\n", _PATH_WALL, strerror(errno));
#undef WALL_CMD
#endif
	struct utmpx *ut;
	char dev[UT_LINESIZE+8];
	int fd, rw;
#ifdef HAVE_POSIX_ASYNCHRONOUS_IO
	static struct aiocb **aiocb_array;
	static struct sigevent sigevb;
	static size_t aiocb_len, siz = 8;

	if (!aiocb_len) {
		sigevb.sigev_notify = SIGEV_SIGNAL;
		sigevb.sigev_signo  = SIGUSR2;
		aiocb_array = err_malloc(siz*sizeof(void*));
	}
#endif
	i = 0;

	setutxent();
	while ((ut = getutxent())) {
		if (ut->ut_type != USER_PROCESS)
			continue;

		snprintf(dev, sizeof(dev), "%s%s", _PATH_DEV, ut->ut_line);
		do {
			fd = open(dev, O_WRONLY|O_APPEND|O_DSYNC|O_NOCTTY, 0664);
			if (fd > 0)
				break;
			if (errno == EINTR)
				continue;
#ifdef HAVE_POSIX_ASYNCHRONOUS_IO
			if (errno == ENFILE && aiocb_count) {
				do {
					rw = aio_suspend((const struct aiocb *const*)aiocb_array,
						aiocb_len-1, NULL);
					if (rw && errno != EINTR) {
						ERR("Failed to write aio_suspend(): %s\n", strerror(errno));
						goto wait_lio;
					}
				} while (rw);
				continue;
			}
#endif
			ERR("Failed to open `%s': %s\n", dev, strerror(errno));
			break;
		} while (fd < 0);
		if (fd < 0)
			continue;

#ifdef HAVE_POSIX_ASYNCHRONOUS_IO
		if (!aiocb_len || !aiocb_array[i]) {
			if (i == siz) {
				siz += 8;
				aiocb_array = err_realloc(aiocb_array, siz*sizeof(void*));
				memset(*aiocb_array+i, 0, (siz-i)*sizeof(void*));
			}
			aiocb_array[i] = err_malloc(sizeof(struct aiocb));
			aiocb_array[i]->aio_offset = 0;
			aiocb_array[i]->aio_buf    = message;
			aiocb_array[i]->aio_nbytes = message_len;
			aiocb_array[i]->aio_reqprio = 0;
			memcpy((void*)&aiocb_array[i]->aio_sigevent,
					(void*)&sigevb, sizeof(struct sigevent));
			aiocb_array[i]->aio_sigevent.sigev_value.sival_ptr = aiocb_array[i];
			if (aiocb_len) aiocb_len++;
		}
		aiocb_array[i]->aio_fildes = fd;
		aiocb_array[i]->aio_lio_opcode = LIO_WRITE;
		if (aio_write(aiocb_array[i])) {
			err_write(fd, message, dev);
			aiocb_array[i]->aio_fildes = -1;
			aiocb_array[i]->aio_lio_opcode = LIO_NOP;
		}
		else aiocb_count++;
		i++;
#else
		err_write(fd, message, dev);
#endif
	}
	endutxent();

#ifdef HAVE_POSIX_ASYNCHRONOUS_IO
wait_lio:
	if (!aiocb_len) aiocb_len = i;
	while (aiocb_count) {
		for (i = 0; i < aiocb_len; i++)
			if (aiocb_array[i]->aio_lio_opcode == LIO_WRITE)
				break;
		aio_suspend((const struct aiocb *const*)&aiocb_array[i], 1, NULL);
	}
	return aiocb_count;
#else
	return 0;
#endif
}

__attribute__((__noreturn__)) static void sv_shutdown(void)
{
	FILE *fp;
	size_t len = 0;
	char *line = NULL, *ptr = NULL;
	char *argv[8], arg[32];
	const char ent[] = "__SV_NAM__";
	struct timespec ts = { .tv_sec = 0L, .tv_nsec = 0L };
	struct timeinterval *ti = timelist;
	struct utmpx ut;
#ifdef SV_DEBUG
	DBG("%s(void)\n", __func__);
#endif

	argv[0] = "sv-stage", argv[1] = arg, argv[2] = NULL;
	if (shutdown_action == SD_REBOOT)
		snprintf(arg, sizeof(arg), "--%s", action[SD_ACTION_REBOOT]);
	else if (shutdown_action == SD_POWEROFF)
		snprintf(arg, sizeof(arg), "--%s", action[SD_ACTION_SHUTDOWN]);
	else if (shutdown_action == SD_SINGLE) {
		snprintf(arg, sizeof(arg), "--%s", action[SD_ACTION_SINGLE]);
		goto shutdown;
	}

	if ((fp = fopen(SV_CONFIG, "r"))) {
		while (getline(&line, &len, fp) > 0)
			if (strncmp(line, ent, sizeof(ent)-1) == 0) {
				ptr = shell_string_value(line+sizeof(ent));
				ptr = err_strdup(ptr);
				free(line);
				fclose(fp);
				break;
			}
	}
	else {
		ERR("Failed to open `%s': %s\n", SV_CONFIG, strerror(errno));
		sighandler(SIGUSR1, NULL, NULL);
	}

	if (ptr) {
		if (strcmp(ptr, "runit") == 0) {
			snprintf(arg, sizeof(arg), "%d", shutdown_action);
			argv[0] = "runit-init";
		}
		else if (strcmp(ptr, "s6") == 0) {
			snprintf(arg, sizeof(arg), "-%d", shutdown_action);
			argv[0] = "s6-svscanctl";
		}
		else if (strncmp(ptr, "daemontools", 11) == 0)
			;
		else {
			ERR("Invalid supervision backend -- %s\n", ptr);
			sighandler(SIGUSR1, NULL, NULL);
		}
		free(ptr);
	}
	else {
		ERR("Failed to get `%s' value\n", ent);
		sighandler(SIGUSR1, NULL, NULL);
	}

shutdown:
	if (offtime <= NOLOGIN_TIME && shutdown_action != SD_MESSAGE)
		sv_nologin();
	if (ti->ti_left < offtime)
		ts.tv_sec = offtime - ti->ti_left;
	else {
		for (; ti->ti_left && ti->ti_left > offtime; ti++)
			;
		ts.tv_sec = offtime - ti->ti_left;
	}

	for (; ti->ti_left; ti++) {
		sv_timewarn(ti->ti_left);
		if (ti->ti_left <= NOLOGIN_TIME && shutdown_action != SD_MESSAGE)
			sv_nologin();

		while (nanosleep(&ts, &ts)) {
			if (errno == EINTR)
				continue;
			else {
				ERR("Failed to nanosleep: %s\n", strerror(errno));
				sighandler(SIGUSR1, NULL, NULL);
			}
		}
		ts.tv_sec = ti->ti_wait;
	}
	sv_timewarn(0U);
	unlink(SD_PIDFILE);
	unlink(_PATH_NOLOGIN);
	(void)printf("\r\n\007System %s time has arrived\007\r\n", action[ai]);

#ifdef SV_DEBUG
	if (reboot_force)
		(void)printf("reboot(%X)\n", reboot_action);
	else
		(void)printf("execlp(%s, %s, NULL)\n", *argv, argv[1]);
	exit(EXIT_SUCCESS);
#else
	if (slog_flag) {
	openlog(progname, LOG_PID | LOG_CONS, LOG_AUTH);
	if (shutdown_action == SD_SINGLE)
		syslog(LOG_CRIT, "%s user mode runlevel (by %s@%s)",
				action[SD_ACTION_SINGLE], whom, hostname);
	else
		syslog(LOG_CRIT, "system %s (by %s@%s)", action[ai], whom, hostname);
	closelog();
	}

	if (shutdown_action == SD_MESSAGE)
		exit(EXIT_SUCCESS);

	if (reboot_sync)
		sync();
	if (reboot_force)
		exit(reboot(reboot_action));

	/* write utmp record */
	memset(&ut, 0, sizeof(ut));
	strncpy(ut.ut_user, action[ai], sizeof(ut.ut_user));
	memcpy(ut.ut_id, "~", 2);
	ut.ut_type = EMPTY;
	ut.ut_tv.tv_sec = shuttime;
	(void)pututxline((const struct utmpx*)&ut);

	execvp(*argv, argv);
	ERR("Failed to execvp(%s, %s): %s\n", *argv, argv[1], strerror(errno));
	sighandler(SIGUSR1, NULL, NULL);
	exit(EXIT_FAILURE);
#endif
}

int main(int argc, char *argv[])
{
	int i;
	int j;
	struct tm *lt;
	time_t now;
	char *ptr;
	size_t len;
	struct passwd *pw;
	FILE *fp;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = *argv;
	else
		progname++;
	now = time(&shuttime);

	/* Parse options */
	while ((i = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (i) {
		case '0':
		case 'P':
		case 'p':
		case 's':
			shutdown_action = SD_POWEROFF;
			  reboot_action = RA_POWEROFF;
			ai = SD_ACTION_SHUTDOWN;
			break;
		case 'H':
		case 'h':
			shutdown_action = SD_POWEROFF;
			  reboot_action = RA_HALT;
			ai = SD_ACTION_HALT;
			break;
		case '6':
		case 'r':
			shutdown_action = SD_REBOOT;
			  reboot_action = RB_AUTOBOOT;
			ai = SD_ACTION_REBOOT;
			break;
		case 'q':
			reboot_force = 1;
			break;
		case 'n':
			reboot_sync = 0;
			break;
		case 'c':
			shutdown_action = SD_CANCEL;
			break;
		case 'f':
			boot_flag = 1;
			break;
		case 'F':
			fsck_flag = 1;
			break;
		case 'l':
			slog_flag = 1;
			break;
		case 'm':
			shutdown_action = SD_MESSAGE;
			break;
		case 'v':
			printf("%s version %s\n", progname, VERSION);
			exit(EXIT_SUCCESS);
		case 'u':
			help_message(EXIT_SUCCESS);
		default:
			help_message(EXIT_FAILURE);
		}
	}

	if (strcmp(progname, action[SD_ACTION_REBOOT]) == 0) {
		if (shutdown_action > 0 && ai != SD_ACTION_REBOOT)
			usage_message(SD_ACTION_REBOOT);
		reboot_action   = RB_AUTOBOOT;
		shutdown_action = SD_REBOOT;
	}
	else if (strcmp(progname, action[SD_ACTION_HALT]) == 0) {
		if (shutdown_action > 0 && ai != SD_ACTION_HALT)
			usage_message(SD_ACTION_HALT);
		reboot_action   = RA_HALT;
		shutdown_action = SD_POWEROFF;
	}
	else if (strcmp(progname, action[SD_ACTION_POWEROFF]) == 0) {
		if (shutdown_action > 0 && ai != SD_ACTION_POWEROFF)
			usage_message(SD_ACTION_POWEROFF);
		reboot_action   = RA_POWEROFF;
		shutdown_action = SD_POWEROFF;
	}
	else if (strcmp(progname, action[SD_ACTION_SHUTDOWN]) == 0) {
		if (shutdown_action < 0)
		shutdown_action = SD_SINGLE;
		ai = SD_ACTION_SINGLE;
	}
	else if (shutdown_action < 0) {
		fprintf(stderr, "Usage: %s -c | -h | -p | -r | -m [time] [message]\n",
			progname);
		exit(EXIT_FAILURE);
	}
	argc -= optind, argv += optind;

	if (shutdown_action == SD_CANCEL) {
		if (!access(SD_PIDFILE, F_OK)) {
			if ((fp = fopen(SD_PIDFILE, "r"))) {
				if (fscanf(fp, "%d", &i) && i > 0) {
					if (kill(i, SIGINT)) {
						ERR("Failed to send %s signal to %s\n", signame[0],
								progname);
						exit(EXIT_FAILURE);
					}
				}
				else {
					ERR("Failed to read `%s': %s\n", SD_PIDFILE, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			else {
				ERR("Failed to open `%s': %s\n", SD_PIDFILE, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		else {
			ERR("no `%s' pidfile found\n", SD_PIDFILE);
			if (reboot_force) {
				ERR("Trying to send %s to `%s'\n", signame[3], action[SD_ACTION_SHUTDOWN]);
				sprintf(message, "-%s", signame[3]);
				return execlp("pkill", "pkill", message, action[SD_ACTION_SHUTDOWN]);
			}
			else exit(EXIT_FAILURE);
		}
	}

	if (strcmp(progname, action[SD_ACTION_SHUTDOWN]) == 0 && argc < 1) {
		fprintf(stderr, "Usage: %s [ -c | -h | -m | -p | -r ] TIME [MESSAGE] "
				"(time argument required)\n", progname);
		exit(EXIT_FAILURE);
	}
	if(!argc)
		goto message;

#define ATOI(p) (p[0] - '0')*10 + (p[1] - '0'); p += 2;
	if (strncasecmp(*argv, "now", 3) == 0)
		offtime = 0L, argc--, argv++;
	else if (**argv == '+') {
		ptr = *argv, ptr++;
		if (!isdigit(*ptr))
			goto time_error;
		offtime = strtol(ptr, NULL, 0)*60L;
		if (errno == ERANGE)
			goto time_error;
		shuttime += offtime;
		argc--, argv++;
	}
	else if (isalnum(**argv) && isdigit(**argv)) {
		ptr = *argv;
		/* handle [h]h:mm by getting rid of the colon */
		if (strlen(ptr) == 4 && ptr[1] == ':')
			ptr[1] = ptr[0], ptr[0] = '0';
		else if (strlen(ptr) == 5 && ptr[2] == ':')
			ptr[2] = ptr[3], ptr[3] = ptr[4], ptr[4] = '\0';

		unsetenv("TZ");
		lt = localtime(&now);
		switch(strlen(ptr)) {
		case 10:
			j = lt->tm_year;
			lt->tm_year = ATOI(ptr);
			/* check whether the specfied year is in the nex cnetury.
			 * allow for one year of error as many people will enter
			 * n - 1 as the start of the year n.
			 */
			if (lt->tm_year < (j % 100)-1)
				lt->tm_year += 100;
			/* adjust for the year 2000 and beyond */
			lt->tm_year += j-(j % 100);
			/* FALLTHROUGH */
		case  8:
			lt->tm_mon = ATOI(ptr);
			if (lt->tm_mon < 0 || lt->tm_hour > 11)
				goto time_error;
			/* FALLTHROUGH */
		case  6:
			lt->tm_mday = ATOI(ptr);
			if (lt->tm_mday < 1 || lt->tm_mday > 31)
				goto time_error;
			/* FALLTHROUGH */
		case  4:
			lt->tm_hour = ATOI(ptr);
			if (lt->tm_hour < 0 || lt->tm_hour > 23)
				goto time_error;
			lt->tm_min = ATOI(ptr);
			if (lt->tm_min < 0 || lt->tm_min > 59)
				goto time_error;
			lt->tm_sec = 0;
			break;
		default:
			goto time_error;
		}
		if ((shuttime = mktime(lt)) == -1L)
			goto time_error;
		if ((offtime = shuttime-now) < 0L) {
			ERR("The specified time is already past: `%ld'\n", shuttime);
			exit(EXIT_FAILURE);
		}
		argc--, argv++;
	}
#undef ATOI

message:
	if (!(whom = getlogin()))
		whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";
	(void)gethostname(hostname, sizeof(hostname));

	sprintf(message, "\007*** System %s message from %s@%s ***\007\n", action[ai],
			whom, hostname);
	timer_pos = strlen(message);
	ptr = message+timer_pos;
	sprintf(ptr, "\007-*- System going down at %8.8s    -*-\007\n\n",
			ctime(&shuttime)+11);
	timer_pos = strlen(message);
	ptr = message+timer_pos;
	message_len = sizeof(message)-timer_pos;
	/* set up this len to be able to update the timer warning */
	timer_pos -= (TIMER_LEN+6U);

	if (argc) {
		for (; *argv; argv++) {
			len = strlen(*argv);
			if (message_len-len <= 2)
				break;
			message_len -= len;
			*++ptr = '\n';
			memmove(ptr, *argv, len);
			ptr += len;
		}
		*++ptr = '\n';
		*++ptr = '\0';
		message_len = ptr-message;
	}

#if !defined(SV_DEBUG)
	if (geteuid()) {
		errno = EPERM;
		ERROR("superuser privilege required to proceed", NULL);
	}
	/* support setuid to a special group */
	if (setuid(geteuid()))
		ERROR("failed to setuid(geteuid())", NULL);
	if ((i = fork()) > 0) {
		fprintf(stderr, "%s [pid=%d]\n", progname, i);
		exit(EXIT_SUCCESS);
	}
	if (i < 0)
		ERROR("Failed to fork()", NULL);
	setsid();
	(void)setpriority(PRIO_PROCESS, 0, PRIO_MIN);
	/* enable logging to system log by default */
	if (shutdown_action != SD_MESSAGE)
		slog_flag++;
#endif
	/* setup pidfile */
	if ((j = open(SD_PIDFILE, O_CREAT|O_WRONLY|O_CLOEXEC, 0644)) < 0)
		ERROR("Failed to open `%s'", SD_PIDFILE);
	if (flock(j, LOCK_EX|LOCK_NB) < 0)
		ERROR("Failed to lock `%s'", SD_PIDFILE);
	if ((fp = fdopen(j, "w")))
		fprintf(fp, "%d", i);
	else
		ERROR("Failed to fdopen(`%s')", SD_PIDFILE);
	/* setup signal */
	sigsetup();

	if (boot_flag)
		if (fclose(fopen(BOOTFILE, "w")))
			ERROR("Failed to open `%s'", BOOTFILE);
	if (fsck_flag)
		if (fclose(fopen(FSCKFILE, "w")))
			ERROR("Failed to open `%s'", FSCKFILE);

	if (offtime)
		(void)printf("System %s scheduled at %.24s.\n", action[ai], ctime(&shuttime));
	else
		(void)printf("System %s NOW!!!\n", action[ai]);

	environ = restricted_environ;
	sv_shutdown();
	return EXIT_SUCCESS;

time_error:
	if (errno)
		ERROR("invalid time argument -- `%s'", *argv);
	else
		(void)ERR("invalid time argument -- `%s'\n", *argv);
	return EXIT_FAILURE;
}

static int sv_nologin(void)
{
	int fd;
#ifdef SV_DEBUG
	DBG("%s(void)\n", __func__);
#endif

	if (!access(_PATH_NOLOGIN, F_OK))
		unlink(_PATH_NOLOGIN);
	if ((fd = open(_PATH_NOLOGIN, O_WRONLY|O_CREAT|O_TRUNC, 0664)) > 0)
		return err_write(fd, message, _PATH_NOLOGIN);
}

static void sv_timewarn(unsigned long int timeleft)
{
	static char *ptr;
#ifdef SV_DEBUG
	DBG("%s(%lu)\n", __func__, timeleft);
#endif
	if (!ptr) ptr = message+timer_pos;

	/* just in case sv_wall() does not return */
	if (setjmp(alarmbuf)){
		WARN("%s: sv_wall did not return!!!\n", __func__);
		return;
	}
	else alarm(30U);

	if (timeleft > 600L)
		(void)snprintf(ptr, TIMER_LEN, "%8.8s", ctime(&shuttime)+11);
	else if (timeleft > 59L)
		(void)snprintf(ptr-3U, TIMER_LEN+3U, "in %lu minutes", timeleft/60L);
	else if (timeleft)
		(void)snprintf(ptr-3U, TIMER_LEN+3U, "in %lu seconds", timeleft);
	else
		(void)snprintf(ptr-3U, TIMER_LEN+3U, "IMMEDIATELY!");

	sv_wall();
	alarm(0U);
}
