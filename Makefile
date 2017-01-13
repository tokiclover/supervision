-include config.mak
PACKAGE    ?= supervision
VERSION     = $(shell sed -nre '3s/(.*):/\1/p' ChangeLog)

SUBDIRS    := src

PREFIX     ?= /usr/local
SYSCONFDIR ?= /etc
SBINDIR    ?= $(EXEC_PREFIX)/sbin
LIBDIR     ?= /lib
RC_CONFDIR ?= $(SYSCONFDIR)/conf.d
RC_INITDIR ?= $(SYSCONFDIR)/init.d
DATADIR    ?= $(PREFIX)/share
DOCDIR     ?= $(DATADIR)/doc/$(PACKAGE)-$(VERSION)
MANDIR     ?= $(DATADIR)/man
VIMDIR     ?= $(DATADIR)/vim/vimfiles
RUNDIR     ?= /var/run
SV_LIBDIR   = $(LIBDIR)/sv
SV_SVCDIR   = $(SYSCONFDIR)/sv

INSTALL    ?= install
install_SCRIPT = $(INSTALL) -m 755
install_DATA   = $(INSTALL) -m 644
MKDIR_P    ?= mkdir -p
LN_S       ?= ln -s

dist_EXTRA  = \
	$(DIST_EXTRA) \
	AUTHORS \
	COPYING \
	README.md \
	TODO \
	BUGS.md \
	ChangeLog
dist_SH_OPTS = \
	OPTIONS.in \
	SVC_OPTIONS \
	SVC_BACKEND
dist_SH_BINS  = \
	sv/.lib/sh/tmpfiles \
	sv/.lib/sh/runscript \
	sv/.lib/sh/init-stage \
	sv/.lib/sh/cgroup-release-agent \
	sv/.lib/sh/dep
dist_SH_SBINS = \
	sv/.lib/bin/sv-config
dist_SH_LIBS  = \
	sv/.lib/sh/cgroup-functions \
	sv/.lib/sh/functions \
	sv/.lib/sh/runscript-functions \
	sv/.lib/sh/supervision-functions
dist_SV_BINS  = \
	src/checkpath \
	src/fstabinfo \
	src/mountinfo \
	src/waitfile
dist_SV_RUNS  =
dist_SCRIPTS  =
dist_SV_SVCS  = \
	$(EXTRA_SUPERVISION_SERVICES) \
	apache2 \
	atd \
	cron \
	cupsd \
	cups-browsed \
	dhcp \
	dhcpd \
	dhcrelay \
	dbus \
	dns \
	getty-tty1 \
	git-daemon \
	gpm \
	hostapd \
	libvirtd \
	mysql \
	pcscd \
	php-fpm \
	postgresql \
	rrdcached \
	rsync-daemon \
	saned \
	spawn-fcgi.nginx \
	snmpd \
	snmptrapd \
	syslog \
	inetd \
	httpd \
	nagios \
	npcd \
	ntp \
	sshd \
	virtlockd \
	virtlogd \
	wpa_supplicant \
	xdm \
	zed
dist_SVC_INSTANCES  = \
	$(EXTRA_SERVICE_INSTANCES) \
	getty-tty6:getty-tty1 getty-tty5:getty-tty1 getty-tty4:getty-tty1 \
	getty-tty3:getty-tty1 getty-ttyS0:getty-tty1 getty-ttyS1:getty-tty1 \
	getty-tty2:getty-tty1 \
	tmpfiles.setup:tmpfiles.dev \
	rrdcached.nagios:rrdcached \
	spawn-fcgi.lighttpd:spawn-fcgi.nginx \
	swapfiles:swaps \
	networkfs:localfs \
	dhcp.wlan0:dhcp wpa_supplicant.wlan0:wpa_supplicant \
	socklog-inet:syslog socklog-ucspi:syslog socklog-unix:syslog
dist_SV_OPTS  = \
	dns/OPTIONS.dnsmasq \
	dhcp/OPTIONS.dhcpcd \
	cron/OPTIONS.cronie cron/OPTIONS.dcron \
	cron/OPTIONS.fcron cron/OPTIONS.vixie-cron \
	httpd/OPTIONS.busybox-httpd httpd/OPTIONS.lighttpd \
	inetd/OPTIONS.ipsvd inetd/OPTIONS.xinetd \
	ntp/OPTIONS.busybox-ntpd ntp/OPTIONS.ntpd \
	php-fpm/OPTIONS.php-fpm5.6 \
	rrdcached/OPTIONS.nagios \
	spawn-fcgi.nginx/OPTIONS.lighttpd spawn-fcgi.nginx/OPTIONS.nginx \
	syslog/OPTIONS.rsyslog syslog/OPTIONS.socklog syslog/OPTIONS.syslog-ng

dist_RS_SVCS = \
	$(EXTRA_RUNSCRIPT_SERVICES) \
	checkfs \
	clock \
	devfs \
	dmesg \
	hostname \
	kmod-static-nodes \
	libvirt-guests \
	localfs \
	loopback \
	miscfs \
	mtab \
	nginx \
	nrpe \
	procfs \
	rdonlyfs \
	rootfs \
	sysctl \
	sysfs \
	swaps \
	tmpdirs \
	tmpfiles.dev \
	vmware \
	zfs \
	zfs-share \
	zpool
dist_RS_OPTS = \
	$(EXTRA_OPTIONS_INSTANCES)

dist_SYSINIT = \
	$(EXTRA_SYSINIT_SERVICES) \
	devfs \
	dmesg \
	kmod-static-nodes \
	sysfs \
	tmpfiles.dev
dist_SYSBOOT = \
	$(EXTRA_SYSBOOT_SERVICES) \
	checkfs \
	localfs \
	loopback \
	miscfs \
	mtab \
	networkfs \
	procfs \
	rootfs \
	sysctl \
	syslog \
	swaps swapfiles \
	tmpfiles.setup
dist_DEFAULT = \
	$(EXTRA_DEFAULT_SERVICES) \
	getty-tty6 getty-tty5 getty-tty4 getty-tty3 getty-tty2 getty-tty1
dist_SHUTDOWN = \
	$(EXTRA_SHUTDOWN_SERVICES) \
	rdonlyfs

ifneq ($(PREFIX),)
ifneq ($(OS),Linux)
dist_DIRS += $(EXEC_PREFIX)$(SV_SVCDIR)
endif
endif

ifeq ($(RUNIT_INIT_STAGE),yes)
dist_SCRIPTS += runit/1 runit/2 runit/3 runit/ctrlaltdel runit/reboot
dist_DIRS    += $(SYSCONFDIR)/runit
endif
ifeq ($(S6_INIT_STAGE),yes)
dist_SCRIPTS += s6/crash s6/finish s6/init-stage-1
dist_DIRS    += $(SYSCONFDIR)/s6
endif

ifeq ($(SYSVINIT),yes)
dist_SH_SBINS += sv/.lib/bin/initctl
dist_SV_SVCS  += initctl
endif

DISTFILES   = \
	$(dist_SV_OPTS) \
	$(dist_SCRIPTS) $(dist_SV_RUNS:%=%/RUN)
dist_DIRS  += \
	$(SV_LIBDIR)/bin $(SV_LIBDIR)/sbin $(SV_LIBDIR)/sh $(DOCDIR) \
	$(SV_LIBDIR)/cache $(SV_LIBDIR)/opt $(PREFIX)$(SV_SVCDIR) \
	$(SV_SVCDIR).conf.d $(SV_SVCDIR)/.sysinit $(SV_SVCDIR)/.sysboot \
	$(SV_SVCDIR)/.default $(SV_SVCDIR)/.shutdown $(SV_SVCDIR)/.single
DISTDIRS    = $(SBINDIR) $(MANDIR)/man5 $(MANDIR)/man8 $(dist_DIRS)

.PHONY: FORCE all install install-doc install-dist install-all

all: $(SUBDIRS)

FORCE:

$(SUBDIRS): FORCE
	$(MAKE) -C $@

install-all: install install-supervision-initd
install: install-dir install-dist install-sv-svcs
	$(install_DATA)  sv.conf $(DESTDIR)$(SV_SVCDIR).conf
	$(install_SCRIPT) src/rs $(DESTDIR)$(SBINDIR)
	$(install_SCRIPT) src/sv-shutdown $(DESTDIR)$(SBINDIR)
	$(LN_S) -f $(SBINDIR)/rs $(DESTDIR)$(SV_LIBDIR)/sbin/rc
	$(LN_S) -f $(SBINDIR)/rs $(DESTDIR)$(SV_LIBDIR)/sbin/service
	$(LN_S) -f $(SBINDIR)/rs $(DESTDIR)$(SBINDIR)/sv-stage
	$(LN_S) -f $(SBINDIR)/sv-shutdown $(DESTDIR)$(SV_LIBDIR)/sbin/halt
	$(LN_S) -f $(SBINDIR)/sv-shutdown $(DESTDIR)$(SV_LIBDIR)/sbin/poweroff
	$(LN_S) -f $(SBINDIR)/sv-shutdown $(DESTDIR)$(SV_LIBDIR)/sbin/reboot
	$(LN_S) -f $(SBINDIR)/sv-shutdown $(DESTDIR)$(SV_LIBDIR)/sbin/shutdown
	$(install_DATA) -D sv.vim $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	$(install_DATA) $(dist_SH_OPTS:%=sv/.opt/%) $(DESTDIR)$(SV_LIBDIR)/opt
	$(install_SCRIPT) sv/.opt/cmd  $(DESTDIR)$(SV_LIBDIR)/opt
	$(install_SCRIPT) $(dist_SV_BINS) $(DESTDIR)$(SV_LIBDIR)/bin
	$(install_SCRIPT) $(dist_SH_BINS) $(DESTDIR)$(SV_LIBDIR)/sh
	$(install_DATA)   $(dist_SH_LIBS) $(DESTDIR)$(SV_LIBDIR)/sh
	$(install_SCRIPT) $(dist_SH_SBINS) $(DESTDIR)$(SV_LIBDIR)/sbin
	$(install_DATA)   $(dist_RS_OPTS:%=sv.conf.d/%) $(DESTDIR)$(SV_SVCDIR).conf.d
	-$(install_DATA)  $(dist_RS_SVCS:%=sv.conf.d/%) $(DESTDIR)$(SV_SVCDIR).conf.d
	$(install_SCRIPT) $(dist_RS_SVCS:%=sv/%)        $(DESTDIR)$(SV_SVCDIR)
	sed -e 's,\(SV_TYPE.*$$\),\1\nSV_LIBDIR=$(SV_LIBDIR)\nSV_SVCDIR=$(SV_SVCDIR),' \
		-i $(DESTDIR)$(SV_LIBDIR)/opt/cmd
	sed -e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' -e 's|@LIBDIR@|$(LIBDIR)|g' \
		-e 's|@SBINDIR@|$(SBINDIR)|g' -e 's|@RUNDIR@|$(RUNDIR)|g' \
		rs.8 >$(DESTDIR)$(MANDIR)/man8/rs.8
	sed -e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' -e 's|@LIBDIR@|$(LIBDIR)|g' \
		-e 's|@SBINDIR@|$(SBINDIR)|g' -e 's|@RUNDIR@|$(RUNDIR)|g' \
		sv-stage.8 >$(DESTDIR)$(MANDIR)/man8/sv-stage.8
	sed -e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' -e 's|@LIBDIR@|$(LIBDIR)|g' \
		-e 's|@SBINDIR@|$(SBINDIR)|g' -e 's|@RUNDIR@|$(RUNDIR)|g' \
		-e 's|@PREFIX@|$(PREFIX)|g' \
		supervision.5 >$(DESTDIR)$(MANDIR)/man5/supervision.5
	sed -e 's|@_PATH_NOLOGIN@|$(_PATH_NOLOGIN)|g' \
		sv-shutdown.8 >$(DESTDIR)$(MANDIR)/man8/sv-shutdown.8
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(LIBDIR)|g' \
		-e 's|/run/|$(RUNDIR)/|g' \
		-i $(DESTDIR)$(SV_LIBDIR)/sh/runscript-functions \
		   $(DESTDIR)$(SV_LIBDIR)/opt/SVC_OPTIONS
ifdef RUNIT_INIT_STAGE
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(LIBDIR)|g' \
		-e 's|/run/|$(RUNDIR)/|g' \
		-e 's|\(_PATH_WALL=\).*$$|\1$(_PATH_WALL)|g' \
		-i $(DESTDIR)$(SYSCONFDIR)/runit/*
endif
ifdef S6_INIT_STAGE
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(LIBDIR)|g' \
		-e 's|/run/|$(RUNDIR)/|g' \
		-e 's|\(_PATH_WALL=\).*$$|\1$(_PATH_WALL)|g' \
		-i $(DESTDIR)$(SYSCONFDIR)/s6/*
endif
	for svc in $(dist_SVC_INSTANCES); do \
		$(LN_S) -f $${svc#*:} $(DESTDIR)$(SV_SVCDIR)/$${svc%:*}; \
	done
	$(LN_S) -f $(dist_SYSINIT:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR)/.sysinit/
	$(LN_S) -f $(dist_SYSBOOT:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR)/.sysboot/
	$(LN_S) -f $(dist_DEFAULT:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR)/.default/
	$(LN_S) -f $(dist_SHUTDOWN:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR)/.shutdown/
	$(LN_S) -f $(SV_LIBDIR)/opt $(DESTDIR)$(SV_SVCDIR)/.opt
ifneq ($(PREFIX),)
ifneq ($(OS),Linux)
	$(LN_S) -f $(SV_LIBDIR)/opt $(DESTDIR)$(PREFIX)$(SV_SVCDIR)/.opt
endif
endif
ifneq ($(OS),Linux)
	$(LN_S) -f $(SV_SVCDIR)/sulogin $(DESTDIR)$(SV_SVCDIR)/.single
#endif
install-dist: $(DISTFILES)
install-dir :
	$(MKDIR_P) $(DISTDIRS:%=$(DESTDIR)%)
install-doc : install-dir
	$(install_DATA)   $(dist_EXTRA)   $(DESTDIR)$(DOCDIR)
install-sv-svcs: install-dir
	cp -r $(dist_SV_SVCS:%=sv/%) $(DESTDIR)$(SV_SVCDIR)

%/RUN: %
	$(install_SCRIPT) sv/$@ $(DESTDIR)$(SV_SVCDIR)/$@
$(dist_SCRIPTS): FORCE
	$(install_SCRIPT) $@ $(DESTDIR)$(SYSCONFDIR)/$@
$(dist_SV_OPTS): install-sv-svcs
	$(install_DATA)  sv/$@ $(DESTDIR)$(SV_SVCDIR)/$@
install-%-initd:
	$(MKDIR_P) $(DESTDIR)$(RC_CONFDIR)
	$(MKDIR_P) $(DESTDIR)$(RC_INITDIR)
	$(install_SCRIPT) $*.initd $(DESTDIR)$(RC_INITDIR)/$*
	$(install_DATA)   $*.confd $(DESTDIR)$(RC_CONFDIR)/$*

.PHONY: uninstall uninstall-doc uninstall-dist uninstall-all

uninstall-all: uninstall unintsall-supervision-initd
uninstall: uninstall-doc
	rm -f $(DESTDIR)$(SV_SVCDIR).conf
	rm -f $(DESTDIR)$(SBINDIR)/sv-stage $(DESTDIR)$(SBINDIR)/sv-shutdown \
		$(DESTDIR)$(SBINDIR)/rs
ifdef SYSVINIT
	rm -f $(DESTDIR)$(SV_LIBDIR)/sbin/initctl
endif
	rm -f $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	rm -f $(DESTDIR)$(MANDIR)/man5/supervision.5 \
		$(DESTDIR)/$(MANDIR)/man8/sv-stage.8 $(DESTDIR)$(MANDIR)/man8/rs.8 \
		$(DESTDIR)/$(MANDIR)/man8/sv-shutdown.8
	rm -f $(dist_SCRIPTS:%=$(DESTDIR)$(SYSCONFDIR)/%)
	rm -f $(dist_SH_OPTS:%=$(DESTDIR)$(SV_LIBDIR)/opt/%) $(DESTDIR)$(SV_LIBDIR)/opt/cmd
	for svc in $(dist_SVC_INSTANCES); do \
		rm -f $(DESTDIR)$(SV_SVCDIR)/$${svc%:*}; \
	done
	rm -f  $(dist_RS_SVCS:%=$(DESTDIR)$(SV_SVCDIR)/%) \
	       $(dist_RS_SVCS:%=$(DESTDIR)$(SV_SVCDIR).conf.d/%) \
	       $(dist_RS_OPTS:%=$(DESTDIR)$(SV_SVCDIR).conf.d/%)
	rm -fr $(dist_SV_SVCS:%=$(DESTDIR)$(SV_SVCDIR)/%)
	rm -f $(DESTDIR)$(SV_LIBDIR)/bin/* $(DESTDIR)$(SV_LIBDIR)/sbin/* \
		$(DESTDIR)$(SV_LIBDIR)/sh/* $(DESTDIR)$(SV_LIBDIR)/cache/* \
		$(DESTDIR)$(SV_SVCDIR)/getty-tty* $(DESTDIR)$(SV_SVCDIR)/.opt \
		$(DESTDIR)$(SV_SVCDIR)/.s*/*
	-rmdir $(dist_DIRS:%=$(DESTDIR)%)
uninstall-doc:
	rm -f $(dist_EXTRA:%=$(DESTDIR)$(DOCDIR)/%)
uninstall-%-initd:
	rm -f $(DESTDIR)$(RC_CONFDIR)/$*
	rm -f $(DESTDIR)$(RC_INITDIR)/$*
	-rmdir $(DESTDIR)$(RC_CONFDIR)
	-rmdir $(DESTDIR)$(RC_INITDIR)

.PHONY: clean

clean:

