#!/bin/sh
#
# $Id:  @(#) sv-init.sh    1.8 2018/07/22 21:09:26                    Exp $
# $C$:  Copyright (c) 2015-2018 tokiclover <tokiclover@gmail.com>     Exp $
# $L$:  2-clause/new/simplified BSD License                           Exp $
#

if [ -n "${ZSH_VERSION}" ]; then
	emulate sh
	NULLCMD=:
	setopt SH_WORD_SPLIT
	disable -r end
fi

:	${CONSOLE:=/dev/console}
svc_rescue_shell()
{
	SVC_DEBUG "function=svc_rescue_shell( ${@} )"
:	${SV_SHELL:=${SHELL:-/bin/sh}}
:	${NULL:=/dev/null}
:	${PATH:=/bin:/sbin:/usr/bin:/usr/sbin}

	if [ "${SV_UNAME}" = "Linux" ]; then
	:	${SULOGIN_TTY:=/dev/tty8}
		if [ "${SV_SHELL##*/}" = "sulogin" ]; then
			if [ -x ${SV_SHELL} ]; then
			exec ${SV_SHELL} -p ${SULOGIN_TTY}
			else
			SV_SHELL=
			fi
		fi
		SULOGIN="$(command -v sulogin 2>${NULL})"
		[ -n "${SULOGIN}" ] && [ -x "${SULOGIN}" ] && exec ${SULOGIN} -p ${SULOGIN_TTY}
	fi

	exec <${CONSOLE} >${CONSOLE}
	exec 2>&1

	for sh in ${SV_SHELL} sh bash zsh ksh csh tcsh ${SHELLS}; do
		sh=$(command -v ${sh} 2>${NULL})
		[ -n "${sh}" ] && [ -x "${sh}" ] && exec ${sh} -lim
	done

	echo "No functional shell found!!!" >&2
}

name="${0##*/}"
if [ "${name}" = "sv-init.sh" ]; then
	SV_LIBDIR="${0%/sh/*}"
else
	name="sv-init.sh"
:	${SV_LIBDIR:=/lib/sv}
fi
if ! . ${SV_LIBDIR}/sh/runscript-functions; then
	echo "${name}: error: Required file not found \`${SV_LIBDIR}/sh/runscript-functions'" >&2
	[ -n "${SV_OPTS}" ] && svc_rescue_shell || exit 1
fi
:	${SV_SVCDIR:=/etc/sv}
. ${SV_LIBDIR}/sh/SV-CONFIG
. ${SV_SVCDIR}.conf
umask 022

:	${SV_UNAME:=$(uname -s)}
:	${SV_UNAME_RELEASE:=$(uname -r)}
SV_CMD="${__SV_CMD__##*/}"
SV_PIDFILE=${SV_TMPDIR}/${SV_CMD}.pid
SVC_NAME="${SV_CMD}"

early_console()
{
	SVC_DEBUG "function=early_console( ${@} )"
	[ -d ${SV_LIBDIR}/cache ] || return

	local encoding='%@' args kbd_args=-a keymap consolefont
	if [ -e ${SV_LIBDIR}/cache/unicode ]; then
		encoding='%G' kbd_args=-u
	fi
	printf "\033${encoding}" >${CONSOLE}

	[ -c ${CONSOLE} ] && args="-C ${CONSOLE}"
	consolefont=${SV_LIBDIR}/cache/font+umap
	[ -s ${consolefont} ] && setfont ${args} ${consolefont} 2>${NULL}

	[ -n "${args}" ] && kbd_mode ${kbd_args} ${args}
	keymap=${SV_LIBDIR}/cache/keymap
	[ -s ${keymap} ] && loadkeys -q ${keymap} 2>${NULL}
}

svc_init()
{
	SVC_DEBUG "function=svc_init( ${@} )"
	local dir opt procfs
	[ -w /etc/mtab ] || opt=-n

	if   [ "${SV_UNAME}" = "GNU/kFreeBSD" ]; then
		procfs=linprocfs
	else
		procfs=proc
	fi

	if ! mountinfo --quiet /proc 2>${NULL}; then
		begin "Mounting /proc"
		if ! fstabinfo --mount /proc >${NULL} 2>&1; then
		mount ${opt} -t ${procfs} -o ${SYSFS_OPTS:-nodev} proc /proc
		fi
		end "${?}"
	fi

	if [ "${SV_UNAME}" = "Linux" ]; then
	if [ ! -d /run ]; then
		begin "Creating /run"
		mkdir -m 0755 -p /run
		end ${?}
	fi
	if ! mountinfo --quiet /run; then
		begin "Mounting /run"
		if ! fstabinfo --mount /run >${NULL} 2>&1; then
		mount ${opt} -t tmpfs -o nodev,mode=755,size=${SV_RUN_FS_SIZE:-1%} run /run
		fi
		end "${?}"
	fi
	fi # SV_UNAME=Linux

	#
	# XXX: Add some insurance to get a few services started early (e.g. lvm
	#   needs /run/lock/lvm which is started before miscfs.) Who need lvm
	#   for `/var'? Nothing will ever... well, it will with some hassle.
	#
	checkpath -d -m 0775 -o root:uucp /run/lock
	for dir in /var/run /var/lock; do
		[ -L "${dir}" ] || continue
		dir="$(readlink ${dir})"
		case "${dir}" in
			(/run*)
			begin "Creating ${dir}"
			mkdir -m 0755 -p "${dir}"
			end ${?}
			;;
		esac
	done

	if grep -Eq "[[:space:]]+xenfs$" /proc/filesystems; then
		begin "Mounting xenfs"
		if ! fstabinfo --mount /proc/xen; then
		mount ${opt} -t xenfs -o ${SYSFS_OPTS:-nosuid,nodev,noexec} xenfs /proc/xen
		fi
		end "${?}"
	fi

	[ -d "${SV_RUNDIR}" ] || svc_rundir
}

svc_rundir()
{
	SVC_DEBUG "function=svc_rundir( ${@} )"
	rm -fr ${SV_TMPDIR}
	[ -d ${SV_SVCDIR}/.env ] && cp -a ${SV_SVCDIR}/.env ${SV_TMPDIR}
	mkdir -p "${SV_TMPDIR}"/down "${SV_TMPDIR}"/deps "${SV_TMPDIR}"/fail \
		${SV_TMPDIR}/envs ${SV_TMPDIR}/opts \
		"${SV_TMPDIR}"/pids "${SV_TMPDIR}"/star "${SV_TMPDIR}"/wait
	#
	# Initialization
	#
	ENV_SVC SV_UNAME
	ENV_SVC SV_UNAME_RELEASE

	#
	# Set up CGroup
	#
	if yesno ${SV_CGROUP:-YES} && [ -z "${SV_SYSTEM}" ]; then
		if sv-run devfs start && sv-run sysfs start; then
		cgroup_start_sys && SV_CGROUP=Yes || SV_CGROUP=No
		else
		SV_CGROUP=FALSE
		fi
	else
		SV_CGROUP=DISABLE
	fi
	ENV_SVC SV_CGROUP
}

make_svscan_pidfile()
{
	SVC_DEBUG "function=make_svscan_pidfile( ${@} )"
	trap "rm -f ${SV_PIDFILE} ${SV_TMPDIR}/svscan.pid" INT QUIT TERM
	echo $$ >${SV_PIDFILE}
	ln -sf ${SV_CMD}.pid ${SV_TMPDIR}/svscan.pid
}
kill_svscan()
{
	local arg cmd dir pid
	SVC_DEBUG "function=kill_svscan( ${@} )"

	if [ "${SV_UNAME}" = "Linux" ]; then
		arg=-a
	else
		arg=-l
	fi
	pgrep ${arg} -f ${__SV_CMD__} | \
	while read pid cmd dir args; do
		if [ "${dir}" = "${SV_RUNDIR}" ]; then
			kill -TERM ${pid} 2>${NULL}
			return
		fi
	done
}

svc_init_default()
{
	local args cmd dir pid count
	SVC_DEBUG "function=svc_init_default( ${@} )"
	svc_wait 30 ${SV_PIDFILE}

	if [ "${SV_CMD}" = "runsvdir" ]; then
		if [ "${SV_UNAME}" = "Linux" ]; then
			args=-af
		else
			args=-lf
		fi
		if [ -e ${SV_PIDFILE} ]; then
			pgrep ${args} ${SV_CMD} | \
			while read pid cmd dir args; do
				if [ x${dir} = x${SV_RUNDIR} ]; then
					echo "${pid}" >${SV_PIDFILE}
					break
				fi
			done
		fi
	fi

	sv-rc default || sv-rc single || svc_rescue_shell
}

case ${SV_CMD} in
	(runsvdir)
:	${SV_OPTS:=log:...........................................................}
	;;
esac

svc_init_level()
{
	SVC_DEBUG "function=svc_init_level( ${@} )"
if   [ "${1}" = "--svscan" ]; then
	[ -d "${SV_RUNDIR}" ] || svc_rundir
	if [ -e ${SV_PIDFILE__} ]; then
		kill_svscan
	fi
	if [ "${2}" = "--foreground" ]; then
		ARGS=""  PRECMD="exec"
	else
		ARGS="&" PRECMD=""
	fi
	make_svscan_pidfile
	eval ${PRECMD} ${__SV_CMD__} ${SV_RUNDIR} ${SV_OPTS:+"$SV_OPTS"} ${ARGS}
elif [ "${1}" = "--sysinit" ]; then
	#
	# XXX: initialize temporary directory
	#
	if [ -n "${SV_SYSTEM}" ]; then
		[ -d "${SV_RUNDIR}" ] || svc_rundir
		return 0
	fi
	early_console
	svc_init
	[ "${2}" = "--background" ] || sv-rc sysinit
elif [ "${1}" = "--default" ]; then
	[ -d "${SV_RUNDIR}" ] || svc_rundir
	#
	# Set up CGroup
	#
	if yesno "${SV_CGROUP}" && [ "${SV_UNAME}" = "Linux" ]; then
		cgroup_add_service
	fi
	#
	# Bring up what left and stage-2 assuming `svcscan'
	# is ready when the following pidfile is present
	#
	if [ -e ${SV_PIDFILE} ]; then
		kill_svscan
		rm ${SV_PIDFILE}
	fi
	#
	# XXX: This hackery is necessary to get the real pid;
	# this is used to send SIGCONT to runsvdir.
	#
	sh -c ". ${SV_LIBDIR}/sh/sv-init.sh; svc_init_default;" &

	make_svscan_pidfile
	exec ${__SV_CMD__} ${SV_RUNDIR} ${SV_OPTS:+"${SV_OPTS}"}

	#
	# Drop into a rescue shell
	#
	svc_rescue_shell
elif [ "${1}" = "--shutdown" -o "${1}" = "--reboot" ]; then
	ACTION=${1}
	case "${ACTION}" in
		(--reboot|--shutdown)  ;;
		(*) ACTION="--shutdown";;
	esac
	sv-rc ${ACTION#--}
	if [ -n "${SV_SYSTEM}" ]; then
		return 0
	fi

	#
	# CGroup clean up
	#
	if yesno "${SV_CGROUP}" && [ "${SV_UNAME}" = "Linux" ]; then
		cgroup_remove_service
	fi
	sv-shutdown --force ${ACTION} now
fi
}

case "${1}" in
	(--default|--reboot|--sysinit|--shutdown|--svscan)
	begin "Starting ${1#--} level\n"
	svc_init_level "${@}"
	end "${?}"
	;;
esac

#
# vim:fenc=utf-8:ft=sh:ci:pi:sts=0:sw=4:ts=4:
#
