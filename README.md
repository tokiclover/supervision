Supervision - init-system and service-manager

Full fledged service-manager and init-system capable of replacing a SystemV or
BSD like rc init-system like OpenRC. Yet, it's still simple and efficient as ever!

**Note:** [busybox](http://www.busybox.net/) has an integrated runit suite which has
the same command line options as `runsvdir,sv,runsv,...`; so support for this is
almost guaranted to function as expected. Just install busybox symlinks, and voila!

---

DESCRIPTION
-----------

Full-featured and yet simple and efficient init-system and service-manager
with service dependencies, standard service commands and extra service commands,
virtual service and service instances and read-only rootfs support to name a few.

*BOOT AND SHUTDOWN STATS*:

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

`make DESTDIR=/tmp installl` to install.

And *do not forget* to run `${LIBDIR}/sv/sbin/sv-config --config ARG` afterwards!
Or, if updating an old installation *do not forget* to run
`${LIBDIR}/sv/sbin/sv-config --update` afterwards!

APPLICATION USAGE
-----------------

The recommanded way to use this package for service management is to use
`${LIBDIR}/sv/sh/sv-rc.sh --(sysinit|default|shutdown)` to start particular init
stage or runlevel when using a supported supervisor. Or else, `sv-rc(8)` like
any SystemV or BSD like system `rc(8)`.

And then use
`sv-run [OPTIONS] SERVICE COMMAND [ARGUMENTS]` to manage particular services;
or rather use `${LIBDIR}/sv/sbin/service [OPTIONS] SERVICE COMMAND` for
SystemV compatibility.

*NOTE:* That symlink can be copied or moved to `${EXEC_PREFIX}/sbin` if
necessary to ease administration and if there is no other SystemV binary
installed in the system.

Or else, use the magic `svscan` mode to supervise `/service/` and then
set `SV_SYSTEM="supervision"` in `/etc/sv.conf` configuration file and finaly use
`sv-rc default` to supervise daemons.

Support for containrization solutions or subsystems is available via _keywords_
usage (see __KEYWORDS__ subsection in `supervision(5)` and `/etc/sv.conf` for more
information) for docker, LXC, jail, systemd-nspawn, UREFIX installation,
supervision, UML, VServer and XEN.

Either the subsystem will be auto detected or use `/etc/sv.conf` to set a particular
subsystem with `SV_SYSTEM="${SUBSYSTEM}"` configuration variable...
`SV_SYSTEM="supervision"` for daemon supervision only;
`SV_SYSTEM="prefix"` for an isolated chrooted environment;
`SV_SYSTEM="docker"` for docker containers et al.
Services that have the subsystem keyword will not be started in that particular
subsystem environment.

To have the supervisor `({damontools[-encore],runit,s6})` executed as __PID 1__...
Just setup the container or subsystem; once done, use something like
the following for docker: `docker run [OPTIONS] --env container=docker --tmpfs /run
IMAGE /lib64/sv/sh/sv-init.sh --default` or; and voila! the supervisor will be executed
as `PID 1` and another process will handle service management to setup the container.

And then... a bit more, new supervision services can be easily added by
running `${LIBDIR}/sv/sbin/sv-config [--log] ${SERVICE} new` (`--log` argument
would add a *log* directory for the service); and then edit
`/etc/sv/${SERVICE}/OPTIONS` before launching the service.

See `supervision(5)`, `sv-rc(8)`, `sv-shutdown(8)` and or `sv-run(8)` man page
for more information.

REQUIREMENTS
------------

Supervision requires standard _coreutils_, _grep_, _procps_,
sed and a (POSIX) shell.
POSIX shell compliance is not enforced because it should not be especially when
using [OpenBSD](http://openbsd.org) version of [ksh](http://www.kornshell.com/),
see `builtins(8)` manual page for more information. Otherwise, everything was
tested with [bash](http://tiswww.case.edu/php/chet/bash/bashtop.html) and
[zsh](http://www.zsh.org/) and work as expected.

Optional SysVinit for compatibilty with SysVinit's utilities is supported.
Else, `sv-shutdown(8)` has a BSD like `shutdown(8)`, `reboot(8)`, `halt(8)` and
`poweroff(8)` behaviour than the SysVinit utilities.

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
