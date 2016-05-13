Supervision Framework, initialy inspired by [supervision-scripts][1] collection,
supporting [Daemontools][3], [Daemontools-Encore][4], [Runit][5] or [S6][6]
supervision backends providing an easy and efficient way to supervision
by using a shell,--to be able to switch between backend & share a few helpers,--and
optional [OpenRC][7] support, to not have to write system boot/halt from scratch.
(A Gentoo [ebuild][2] is available.)

Note: [busybox](http://www.busybox.net/) has an integrated runit suite which has
the same command line options as `runsvdir/sv`; so support for this variant is
almost guaranted to function as expected.

---

INTRODUCTION
-----------

Full-featured and yet simple and efficient Supervision Service Management
Framework with service dependencies, LSB and extra service commands, virtual
service & service instances and read-only rootfs support to name a few.

Runscript services are also supported with `{after,before,need,use}` service
dependencies. A special system initialization level (stage-0), with system
boot (stage-1), running state (stage-2) and shutdown (stage-3) levels are
supported.

INSTALLATION
------------

`make CFLAGS=-O2` to build (or add `SYSVINIT=1` to get an additional
SysVinit compatibility binary/service); and then
`make DESTDIR=/tmp PREFIX=/usr/local RUNIT=1 S6=1 install-all`
or only `install` (without supervision init script) would suffice.
(Remove `RUNIT/S6` to avoid installing related Init-Stage-[123].)
Add STATIC=1 for a static `/service/`. **WARN:** Read-only rootfs
is not supported with this because `/etc/service` should be writable;
and copy service directory to make new instances, e.g.
`cp -a /etc/sv/getty /etc/service/getty-ttyS2` instead of
`ln -s /etc/sv/getty /etc/service/getty-ttyS2`.

And do not forget to run `sv/.lib/bin/sv-config -S BACKEND` afterwards!
or `${DESTDIR}/lib/sv/bin/sv-config -S BACKEND` after installation.

DOCUMENTATION/USAGE
-------------

The recommanded way to use this package for service management is using
`sv/.lib/sh/init-stage -(0|1|2|3)` to start a runlevel. And then use
`rs [OPTIONS] SERVICE COMMAND [ARGUMENTS]` to manage particular services.
This will ensure proper service dependency scheduling. Otherwise, just set up
`SV_RUNDIR` (see `sv/.lib/sh/init-stage` for the actual steps); execute a
supervision daemon, e.g. `svcscan ${SV_RUNDIR}` for daemontools*, and bring
up a particular runlevel by using `rs -(1|2[3) stage` (sysinit runlevel or
stage-0 is unnecessary); or else, start/stop (or other supported LSB commands)
with `rs SERVICE start|stop`, e.g. `rs acpid start`.

See supervision(1) man page for more information.

REQUIREMENTS
------------

Supervision Scripts Framework requires standard GNU coreutils, grep, procps,
sed & a (POSIX) shell (BusyBox/ash *might* suffice.)
Init-Stage-[123] & supervision init script service was written mainly for OpenRC
to alleviate the burden of writting system boot/shutdown scripts (from scratch.)

CONTRIBUTORS
------------

Thanks to:

Avery Payne (original supervision-scripts);
And others.

LICENSE
-------

Distributed under the 2-clause/new/simplifed BSD License

---

[1]:https://github.com/apayne/supervision-scripts
[2]:https://github.com/tokiclover/bar-overlay
[3]:http://cr.yp.to/daemontools.html
[4]:http://untroubled.org/daemontools-encore/
[5]:http://smarden.org/runit/
[6]:http://www.skarnet.org/software/s6/
[7]:https://github.com/OpenRC/openrc
