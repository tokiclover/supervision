Supervision Scripts Framework, inspired by [supervision-scripts][1] collection,
supporting [Daemontools][3], [Daemontools-Encore][4], [Runit][5] or [S6][6]
supervision backends providing an easy and efficient way to supervision
by using a shell,--to be able to switch between backend & share a few helpers,--and
[OpenRC][7],--to not have to write system boot/halt from scratch.
(A Gentoo [ebuild][2] is available.)

---

INTRODUCTION
-----------

Full-featured and yet simple and efficient Supervision Service Management
Framework with service dependencies, LSB and extra service commands, virtual
service & service instances and read-only rootfs support to name a few.

While `envdir` variant is supported, this framework prefer using a *single*
file instead: `OPTIONS` file for each service along with a default file
`SVC_OPTIONS` to not wear the boot disk with the boot read/write burst
(parallele starting `x` restart-per-second for failed service `x` global+local
*envdir* `x` number-of-service...); So that an almost empty directory with a
`{finish,run}` symlinked to `sv/.opt/cmd` would work out of the box!!!
And so does `log/{finish,run}` directory!

Runscript services are also supported with `{after,before,need,use}` service
dependencies. A special system initialization level (stage-0), with system
boot (stage-1), running state (stage-2) and shutdown (stage-3) levels are
supported.

INSTALLATION
------------

`make DESTDIR=/tmp PREFIX=/usr/local RUNIT=1 S6=1 install-all`
or only `install` (without supervision init script) would suffice.
(Remove `RUNIT/S6` to avoid installing related Init-Stage-[123].)
Add STATIC=1 for a static `/service/`. **WARN:** Read-only rootfs
is not supported with this because `/etc/service` should be writable;
and copy service directory to make new instances, e.g.
`cp -a /etc/sv/getty /etc/service/getty-ttyS2` instead of
`ln -s /etc/sv/getty /etc/service/getty-ttyS2`.

And do not forget to run `sv/.bin/sp -S BACKEND` afterwards!

DOCUMENTATION
-------------

See supervision(1) man page.

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
