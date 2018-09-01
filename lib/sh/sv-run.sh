#!/bin/sh
#
# $Id:  @(#) sv-run.sh    0.8 2018/07/22 21:09:26                     Exp $
# $C$:  Copyright (c) 2015-2018 tokiclover <tokiclover@gmail.com>     Exp $
# $L$:  2-clause/new/simplified BSD License                           Exp $
#

#
# ZSH compatibility
#
if [ -n "${ZSH_VERSION}" ]; then
	emulate sh
	NULLCMD=:
	setopt NO_GLOB_SUBST SH_WORD_SPLIT
	disable -r end
fi

help_message() {
	cat <<-EOH
 usage: ${0##*/} [OPTIONS] SERVICE COMMAND [ARGUMENTS]
   COMMAND: add|del|desc|remove|reload|restart|start|stop|status|zap|cgroup_remove_service
   OPTIONS: [OPTIONS] SERVICE COMMAND [ARGUMENTS]
     -D, --nodeps     Disable dependencies1
     -d, --debug      Enable debug mode
     -x, --trace      Enable shell trace
     -r, --rs         Select runscript service type
     -s, --sv         Select supervision service type
     -h, --help       Print help and exit
EOH
${1+exit ${1}}
}

while true; do
	case "${1}" in
		(-D|--node*) SVC_DEPS=0 ;;
		(-p|--deps*) SVC_DEPS=1 ;;
		(-r|--rs) SV_TYPE="rs" ;;
		(-s|--sv) SV_TYPE="sv" ;;
		(-x|--trace) set -x ;;
		(-d|--debug) SVC_DEBUG=true;;
		(-h|--help) help_message 0 ;;
		(*) break ;;
	esac
	shift
done
if ! [ ${#} -ge 2 ]; then
	echo "${0##*/}: error: Insufficient number of arguments" >&2
	exit 1
fi

:	${SV_LIBDIR:=${0%/sh/*}}
:	${SVC_NAME:=${1##*/}}
name="${SVC_NAME}"
readonly __cmd__="${2}"
readonly __cmd_args__="${@#*$2}"
readonly __svc__="${1}"
readonly __av0__="${0}"
if [ -d "${__svc__}" ]; then
:	${SV_TYPE:=sv}
else
:	${SV_TYPE:=rs}
fi
case "${1}" in
	(/etc/sv/*)
:	${SV_SVCDIR:=${1%/*}}
	;;
esac
shift
readonly SVC_NAME SV_TYPE

if ! . ${SV_LIBDIR}/sh/runscript-functions; then
	echo "${0##*/}: error: Required file not found \`${SV_LIBDIR}/sh/runscript-functions'" >&2
	rm -f ${SV_TMPDIR}/wait/${SVC_NAME}
	exit 1
fi

svc_cleanup()
{
	local ret="$?" last="$_"
	rm -f ${SV_TMPDIR}/wait/${SVC_NAME} ${SV_TMPDIR}/envs/${SVC_NAME}
	_="${last}"
	return ${ret}
}
trap svc_cleanup INT TERM QUIT

#
# Handle service command
#
if   [ "${SV_TYPE}" = "rs" ]; then
	rs_cmd "${@}"
elif [ "${SV_TYPE}" = "sv" ]; then
	#
	# Save environment file to handle false start failures
	#
	if [ "${__cmd__}" = "start" -o "${__cmd__}" = "stop" ] || \
	   [ "${__cmd__}" = "up"    -o "${__cmd__}" = "down" ]; then
	while read line; do
		echo "${line}"
	done <${SV_TMPDIR}/environ >${SV_TMPDIR}/envs/${SVC_NAME}
	ENV_SET SVC_DEPS
	else
	rm -f ${SV_TMPDIR}/envs/${SVC_NAME}
	fi

	svc_cmd "${@}"
else
	error "nothing to do -- invalid usage"
fi

retval="${?}"
rm -f ${SV_TMPDIR}/wait/${SVC_NAME}
exit ${retval}

#
# vim:fenc=utf-8:ft=sh:ci:pi:sts=0:sw=4:ts=4:
#
