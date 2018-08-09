#!/bin/sh
#
# $Id:  @(#) sv-deps.sh     2.0 2018/08/07 21:09:26                   Exp $
# $C$:  Copyright (c) 2015-2017 tokiclover <tokiclover@gmail.com>     Exp $
# $L$:  2-clause/new/simplified BSD License                           Exp $
#

#
# ZSH compatibility
#
if [ -n "${ZSH_VERSION}" ]; then
	emulate sh
	NULLCMD=:
	setopt NULLGLOB SH_WORD_SPLIT
	disable -r end
elif [ -n "${BASH_VERSION}" ]; then
	shopt -qs nullglob
fi

SV_CGROUP=0
name="${0##*/}"
SV_LIBDIR="${0%/sh/*}"
export LC_ALL=C LANG=C
if ! . ${SV_LIBDIR}/sh/runscript-functions; then
	echo "${name}: error: Required file not found \`${SV_LIBDIR}/sh/runscript-functions'" >&2
	exit 1
fi
SV_DEPDIR="${SV_TMPDIR}/deps"

__svc_deps__()
{
	local RC_OPTS SVC_DEBUG SVC_NAME SVC_NEED SVC_USE SVC_BEFORE SVC_AFTER name
	local SVC_KEYWORD SVC_PROVIDE SVC_TIMEOUT SVC_PIDFILE SV_SERVICE RC_SERVICE
	local dep

	if   [ -d "${__svc__}" ]; then
		SV_TYPE=sv
	elif [ -f "${__svc__}" ]; then
		SV_TYPE=rs
	else
		return
	fi
	SVC_NAME="${__svc__##*/}"
	if isin ${SVC_NAME} ${SVCLIST}; then
		${append} || return 0
	else
		SVCLIST="${SVCLIST} ${SVC_NAME}"
	fi
	. ${SV_LIBDIR}/sh/runscript-functions ||
		{ echo "Required file not found." >&2; exit 1; }

	[ -n "${SVC_KEYWORD}" ] && echo "${SVC_NAME}:keyword=\"${SVC_KEYWORD}\"" >&4
	[ -n "${SVC_PROVIDE}" ] && echo "${SVC_NAME}:provide=\"${SVC_PROVIDE}\"" >&4
	[ -n "${SVC_TIMEOUT}" ] && echo "${SVC_NAME}:timeout=\"${SVC_TIMEOUT}\"" >&4
	for dep in ${__SV_DEPS_ORDER__}; do
		eval echo "\"${SVC_NAME}:${dep%:*}=\\\"\$SVC_${dep#*:}\\\"\"" >&4
	done
}

SVCDEPS_UPDATE=${SVCDEPS_UPDATE:+true}
:	${SVCDEPS_UPDATE:=false}
SVCLIST=
svcdeps=${SV_DEPDIR}/svcdeps
svclist=${SV_DEPDIR}/svclist
svcdirs="${SV_SVCDIR}"
if [ -n "${__SV_PREFIX__}" -a -d "${__SV_PREFIX__}" -a "${__SV_PREFIX__}" != "/usr" ]; then
	svcdirs="${svcdirs} ${__SV_PREFIX__}${SV_SVCDIR}"
fi
if [ -n "${SV_PREFIX}" -a -d "${SV_PREFIX}" ]; then
	svcdirs="${svcdirs} ${SV_PREFIX}${SV_SVCDIR}"
fi

append=false
if [ "${#}" = 0 ]; then
	exec 4>${svcdeps}
	for dir in ${svcdirs}; do
		for __svc__ in ${dir}/*; do
			__svc_deps__
		done
	done
	printf "${SVCLIST## }" >${svclist}
else
	exec 4>${SV_DEPDIR}/tmp
	printf ""   >>${svclist}
	read SVCLIST <${svclist}
	TMPLIST= one=true
	for SVC_NAME; do
		if ${SVCDEPS_UPDATE} && isin "${SVC_NAME}" ${SVCLIST}; then
			sed "/^${SVC_NAME}:/d" -i ${svcdeps}
			append=true
		else
			append=false
		fi
		for dir in ${svcdirs}; do
			__svc__=${dir}/${SVC_NAME}
			if __svc_deps__; then
				${append} || TMPLIST="${TMPLIST} ${SVC_NAME}"
				break
			fi
		done
	done
	cat ${SV_DEPDIR}/tmp >>${svcdeps}
	rm -f ${SV_DEPDIR}/tmp
	printf "${TMPLIST}" >>${svclist}
fi
exec 4>&-

#
# vim:fenc=utf-8:ft=sh:ci:pi:sts=0:sw=4:ts=4:
#
