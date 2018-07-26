Supervision Framework - init-system and service-manager

Full fledged service-manager and init-system capable of replacing a
rc init-system like OpenRC. Yet, it's still simple and efficient as ever!

---

INTRODUCTION
------------

This project was initialy inspired by [supervision-scripts][1] collection,
supporting [Daemontools][3], [Daemontools-Encore][4], [Runit][5] or [S6][6]
supervision backends providing an easy and efficient way to supervision
by using a shell,--to be able to switch between backend and share a few shell
functions,--and an optional [OpenRC][7] support for service configuration files.
(A Gentoo [ebuild][2] is available.)

Note: [busybox](http://www.busybox.net/) has an integrated runit suite which has
the same command line options as `runsvdir/sv`; so support for this variant is
almost guaranted to function as expected.

DESCRIPTION
-----------

Full-featured and yet simple and efficient init-system and service-manager
with service dependencies, LSB and extra service commands, virtual
service and service instances and read-only rootfs support to name a few.

Runscript services are also supported with `{after,before,need,use}` service
dependencies.

*STATS*:

A comlete system, Linux in this test bed,--with rootfs on LVM(2) on top of
dm-crypt LUKS, ZFS on top of dm-crypt LUKS, /(var/)tmp on ZRAM backed devices,
a few union filesystem (OverlayFS+SquashFS),--with 52 system services for short,
can boot or shut down in 11 seconds thanks to supervision parallel service
management! Now if _udev_ is used, add 5 seconds to boot up... or it could be
even worse depending on the number of devices.
dependencies.

INSTALLATION
------------

`./configure --build=x86_64-pc-linux-gnu --libdir=/lib64`

    - add `--enable-sysvinit` to get an additional sysvinit compatibility service;
    - add `--enable-runit` to get Runit init-stage-[123]; and likewise
    - add `--enable-s6` for S6 init-stage-[123]; and then
    - (other standard options are available, see `--help`);
    - WARNING: this package should be istalled in a non prefixed installation, so,
    avoid using `--libdir=/usr/lib64` or similar to have system utilities in rootfs;

`make -j2` to build; and finaly

`make DESTDIR=/tmp install-all` or use only `install` (without supervision init
script for OpenRC) would suffice.

And do not forget to run `${LIBDIR}/sv/sbin/sv-config --config ARG` afterwards!
or `${LIBDIR}/sv/sbin/sv-config --config ARG` after installation.

APPLICATION USAGE
-----------------

The recommanded way to use this package for service management is to use
`${LIBDIR}/sv/sh/sv-init.sh --(sysinit|default|shutdown)` to start particular stage. And then use
`sv-run [OPTIONS] SERVICE COMMAND [ARGUMENTS]` to manage particular services;
or rather use `${LIBDIR}/sv/sbin/service [OPTIONS] SERVICE COMMAND` for
SystemV compatibility. *NOTE:* That symlink can be copied to `/sbin` if
necessary to ease administration and if there is no other SystemV binary
installed in the system.

Or else, use the magic `--svscan` command line argument to set up `/service/` and
`svscan`, set `SV_SYSTEM="supervision"` in `/etc/sv.conf` and then use
`sv-init --default [start|stop]` to start/stop daemons.
This will ensure proper service dependency scheduling.

Support for containrization solutions or subsystems is available via _keywords_
usage (see __KEYWORDS__ subsection in `supervision(5)` and `sv.conf` for more
information) for docker, LXC, jail, systemd-nspawn, prefix, supervision, UML,
VServer and XEN.
Either the subsystem will be auto detected or use sv.conf to set a particular
subsystem with `SV_SYSTEM="${SUBSYSTEM}"` configuration variable...
`SV_SYSTEM="supervision"` for daemon supervision only;
`SV_SYSTEM="prefix"` for an isolated chrooted environment;
`SV_SYSTEM="docker"` for docker containers et al.
Services that have the subsystem keyword will not be started in that particular
subsystem environment.

To have the supervisor `({damontools[-encore],runit,s6})` executed as __PID 1__...
Just setup the container or subsystem; once done, use something like
the following for docker: `docker run [OPTIONS] --env container=docker --tmpfs /run
IMAGE /lib/sv/sh/sv-init.sh --default`; and voila! the supervisor will be executed
as `PID 1` and another process will handle service management to setup the container.

And then... a bit more, new supervision services can be easily added by
running `${LIBDIR}/sv/sbin/sv-config [--log] SERVICE new` (`--log` argument
would add a *log* directory for the service.)

See `supervision(5)`, `sv-init(8)`, `sv-shutdown(8)` and or `sv-run(8)` man page
for more information.

REQUIREMENTS
------------

Supervision Scripts Framework requires standard _coreutils_, _grep_, _procps_,
sed and a (POSIX) shell.
POSIX shell compliance is not enforced because it should not be especially when
using [OpenBSD](http://openbsd.org) version of [ksh](http://www.kornshell.com/),
see `builtins(8)` manual page for more information. Otherwise, everything was
tested with [bash](http://tiswww.case.edu/php/chet/bash/bashtop.html) and
[zsh](http://www.zsh.org/) and work as expected.
Optional SysVinit for compatibilty with SysVinit's utilities.

CONTRIBUTORS
------------

Thanks to:

Avery Payne (original supervision-scripts);
And others.

LICENSE
-------

Distributed under the 2-clause/new/simplifed BSD License

---

[1]:https://gitlab.com/apayne/supervision-scripts
[2]:https://gitlab.com/tokiclover/bar-overlay
[3]:http://cr.yp.to/daemontools.html
[4]:http://untroubled.org/daemontools-encore/
[5]:http://smarden.org/runit/
[6]:http://www.skarnet.org/software/s6/
[7]:https://github.com/OpenRC/openrc
