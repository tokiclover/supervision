#!/bin/sh

SV_LIBDIR=lib
:	${SVC_NAME:=atd}
 . $SV_LIBDIR/sh/runscript-functions

svc_status -f
retval="$?"
echo "retval=$retval"
if [ "$retval" = "0" ]; then
	svc_status --del -f
	svc_status -f
	retval="$?"
	echo "retval=$retval"
fi
svc_status -s
retval="$?"
echo "retval=$retval"
if [ "$retval" = "0" ]; then
	svc_status --del -s
	svc_status -s
	retval="$?"
	echo "retval=$retval"
fi
svc_status -d
retval="$?"
if [ "$retval" = "0" ]; then
	svc_status --del -d
	svc_status -d
	retval="$?"
	echo "retval=$retval"
fi
svc_status -p
retval="$?"
echo "retval=$retval"
retval="$?"
svc_status -w
echo "retval=$retval"


