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
functions,--and an optional [OpenRC][7] support for service configuration files
and init-system support.
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
dependencies. A special system initialization level (stage-0), with system
boot (stage-1), running state (stage-2) and shutdown (stage-3) levels are
supported.

A new init-system is available since 0.12.0_alpha. (It's almost only stage-[01]
services plus tweakings... *almost* is not all there is to it; there are much
more bits tweaking and (re-)writing taking a whole third, +4900 lines addtion
(mainly for init-system services +3550 lines), of this package.)

*STATS*:

A comlete system, Linux in this test bed,--with rootfs on LVM(2) on top of
dm-crypt LUKS, ZFS on top of dm-crypt LUKS, /(var/)tmp on ZRAM backed devices,
a few union filesystem (OverlayFS+SquashFS),--with 52 system services for short,
can boot or shut down in 11 seconds thanks to supervision parallel service
management! Now if _udev_ is used, add 5 seconds to boot up... or it could be
even worse depending on the number of devices.

INSTALLATION
------------

`./configure --build=x86_64-pc-linux-gnu`

    - add `--enable-sysvinit` to get an additional SysVinit compatibility service;
    - add `--enable-runit` to get Runit init-stage-[123]; and likewise
	- add `--enable-s6` for S6 init-stage-[123]; and then
	- (other standard options are available, see `--help`);

`make -j2` to build; and finaly

`make DESTDIR=/tmp install-all` or use only `install` (without supervision init
script for OpenRC) would suffice.

And do not forget to run `sv/.lib/bin/sv-config -S BACKEND` afterwards!
or `${LIBDIR}/sv/bin/sv-config -S BACKEND` after installation.

DOCUMENTATION/USAGE
-------------------

The recommanded way to use this package for service management is to use
`sv/.lib/sh/init-stage -(1|2|3)` to start particular stage. And then use
`rs [OPTIONS] SERVICE COMMAND [ARGUMENTS]` to manage particular services.
Or else, use the magic `-0` command line argument to set up `/service/` and
`svscan`, and then use `rs -2 stage start|stop` to start/stop daemons.
This will ensure proper service dependency scheduling.

And then... a bit more, new supervision services can be easily added by
running `/lib/sv/bin/sv-config [--log] SERVICE new` (`--log` argument
would add a *log* directory for the service.)

See supervision(5) and or rs(8) man page for more information.

REQUIREMENTS
------------

Supervision Scripts Framework requires standard GNU coreutils, grep, procps,
sed and a (POSIX) shell. Optional SysVinit for compatibilty with SysVinit's init;

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
