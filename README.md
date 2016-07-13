Supervision Framework, initialy inspired by [supervision-scripts][1] collection,
supporting [Daemontools][3], [Daemontools-Encore][4], [Runit][5] or [S6][6]
supervision backends providing an easy and efficient way to supervision
by using a shell,--to be able to switch between backend & share a few helpers,--and
optional [OpenRC][7] support for service configuration files and init-system support.
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

A new init-system is available since 0.12.0_alpha. (It's almost only stage-[01]
services plus tweakings... *almost* is not all there is to it.)

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
or `${EXEC_PREFIX}${DESTDIR}/lib/sv/bin/sv-config -S BACKEND` after installation.

DOCUMENTATION/USAGE
-------------

The recommanded way to use this package for service management is to use
`sv/.lib/sh/init-stage -(1|2|3)` to start particular stage. And then use
`rs [OPTIONS] SERVICE COMMAND [ARGUMENTS]` to manage particular services.
Or else, use the magic `-0` command line argument to set up `/service/` and
`svscan`, and then use `rs --sv -2 stage start|stop` to start/stop daemons.
This will ensure proper service dependency scheduling.

See supervision(1) and or rs(8) man page for more information.

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
