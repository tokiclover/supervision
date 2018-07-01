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
dist_SH_BINS  = \
	lib/sh/cmd \
	lib/sh/tmpfiles \
	lib/sh/runscript \
	lib/sh/init-stage \
	lib/sh/cgroup-release-agent \
	lib/sh/depgen
dist_SV_SBINS = \
	lib/bin/sv-config
dist_SH_LIBS  = \
	lib/sh/SV_OPTIONS.in \
	lib/sh/SV_BACKEND \
	lib/sh/cgroup-functions \
	lib/sh/functions \
	lib/sh/runscript-functions \
	lib/sh/supervision-functions
dist_SV_BINS  = \
	src/checkpath \
	src/fstabinfo \
	src/mountinfo \
	src/waitfile
dist_SCRIPTS  = $(dist_INIT_STAGE)
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
dist_SV_LOGS = \
	$(EXTRA_SVLOG_SERVICES) \
	apache2 \
	cron \
	cups-browsed \
	cupsd \
	dhcp \
	dhcpd \
	git-daemon \
	gpm \
	hostapd \
	inetd \
	libvirtd \
	mysql \
	nagios \
	ntp \
	postgresql \
	rrdcached.nagios \
	rrdcached \
	rsync-daemon \
	saned \
	snmpd \
	snmptrapd \
	socklog-inet \
	socklog-ucspi \
	socklog-unix \
	syslog \
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

dist_SINGLE = \
	$(EXTRA_SINGLE_SERVICES)
dist_SYSINIT = \
	$(EXTRA_SYSINIT_SERVICES) \
	devfs \
	dmesg \
	kmod-static-nodes \
	sysfs
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
	sshd \
	getty-tty6 getty-tty5 getty-tty4 getty-tty3 getty-tty2 getty-tty1
dist_SHUTDOWN = \
	$(EXTRA_SHUTDOWN_SERVICES) \
	rdonlyfs

dist_RS_SYMLINKS = rc service
dist_SHUTDOWN_SYMLINKS = halt poweroff reboot shutdown
dist_BINS_SYMLINKS = envdir envuidgid fghack pgrhack setlock setuidgid softlimit

ifneq ($(PREFIX),)
ifneq ($(OS),Linux)
dist_DIRS += $(PREFIX)$(SV_SVCDIR)
endif
endif

ifeq ($(RUNIT_INIT_STAGE),yes)
dist_INIT_STAGE += runit/1 runit/2 runit/3 runit/ctrlaltdel runit/reboot
dist_DIRS    += $(SYSCONFDIR)/runit
endif
ifeq ($(S6_INIT_STAGE),yes)
dist_INIT_STAGE += s6/crash s6/finish s6/init
dist_DIRS    += $(SYSCONFDIR)/s6
endif

ifeq ($(SYSVINIT),yes)
dist_SV_SBINS += src/initctl
dist_SV_SVCS  += initctl
endif

DISTFILES   = \
	$(dist_INIT_STAGE) $(dist_SV_OPTS)
dist_DIRS  += \
	$(SV_LIBDIR)/bin $(SV_LIBDIR)/sbin $(SV_LIBDIR)/sh $(DOCDIR) \
	$(SV_LIBDIR)/cache $(PREFIX)$(SV_SVCDIR) \
	$(SV_SVCDIR).conf.d $(SV_SVCDIR)/.sysinit $(SV_SVCDIR)/.sysboot \
	$(SV_SVCDIR)/.default $(SV_SVCDIR)/.shutdown $(SV_SVCDIR)/.single
DISTDIRS    = $(SBINDIR) $(MANDIR)/man5 $(MANDIR)/man8 $(dist_DIRS)

dist_SVC_SED  =
ifneq ($(OS),Linux)
dist_SVC_SED += -e 's|/usr/|$(PREFIX)/|g'
endif
ifneq ($(EXEC_PREFIX),)
dist_SVC_SED += 's|/sbin/rs|$(SBINDIR)/rs|g'
endif

dist_MAN_SED += \
	-e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' \
	-e 's|@RUNDIR@|$(RUNDIR)|g' \
	-e 's|@LIBDIR@|$(LIBDIR)|g' \
	-e 's|@SBINDIR@|$(SBINDIR)|g' \
	-e 's|@PREFIX@|$(PREFIX)|g' \
	-e 's|@_PATH_NOLOGIN@|$(_PATH_NOLOGIN)|g'
INIT_STAGE_SED = -e 's|\(PATH=\).*$$|\1$(_PATH_STDPATH)|g' \
	-e 's|\(_PATH_WALL=\).*$$|\1$(_PATH_WALL)|g' \
	-e 's|/etc|$(SYSCONFDIR)|g' \
	-e 's|/lib|$(LIBDIR)|g' \
	-e 's|/run/|$(RUNDIR)/|g'


.PHONY: FORCE all install install-doc install-dist

all: $(SUBDIRS)

FORCE:

$(SUBDIRS): FORCE
	$(MAKE) -C $@

install: install-dir install-dist install-sv-svcs
	$(install_DATA)  sv.conf $(DESTDIR)$(SV_SVCDIR).conf
	$(install_SCRIPT) src/rs $(DESTDIR)$(SBINDIR)
	$(install_SCRIPT) src/sv-shutdown $(DESTDIR)$(SBINDIR)
	$(LN_S) -f $(SBINDIR)/rs $(DESTDIR)$(SBINDIR)/sv-stage
	for s in $(dist_RS_SYMLINKS); do \
		$(LN_S) -f $(SBINDIR)/rs $(DESTDIR)$(SV_LIBDIR)/sbin/$${s}; \
	done
	for s in $(dist_SHUTDOWN_SYMLINKS); do \
		$(LN_S) -f $(SBINDIR)/sv-shutdown $(DESTDIR)$(SV_LIBDIR)/sbin/$${s}; \
	done
	$(install_DATA) -D sv.vim $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	$(install_SCRIPT) $(dist_SV_BINS) $(DESTDIR)$(SV_LIBDIR)/bin
	$(install_SCRIPT) $(dist_SH_BINS) $(DESTDIR)$(SV_LIBDIR)/sh
	$(install_DATA)   $(dist_SH_LIBS) $(DESTDIR)$(SV_LIBDIR)/sh
	$(install_SCRIPT) $(dist_SV_SBINS) $(DESTDIR)$(SV_LIBDIR)/sbin
	$(install_DATA)   $(dist_RS_OPTS:%=sv.conf.d/%) $(DESTDIR)$(SV_SVCDIR).conf.d
	-$(install_DATA)  $(dist_RS_SVCS:%=sv.conf.d/%) $(DESTDIR)$(SV_SVCDIR).conf.d
	$(install_SCRIPT) $(dist_RS_SVCS:%=sv/%)        $(DESTDIR)$(SV_SVCDIR)
	sed -e 's,\(SV_LIBDIR=\).*$$,\1$(SV_LIBDIR)\nSV_SVCDIR=$(SV_SVCDIR),' \
		-i $(DESTDIR)$(SV_LIBDIR)/sh/cmd
	sed $(dist_MAN_SED) rs.8 >$(DESTDIR)$(MANDIR)/man8/rs.8
	sed $(dist_MAN_SED) sv-stage.8 >$(DESTDIR)$(MANDIR)/man8/sv-stage.8
	sed $(dist_MAN_SED) supervision.5 >$(DESTDIR)$(MANDIR)/man5/supervision.5
	sed $(dist_MAN_SED) sv-shutdown.8 >$(DESTDIR)$(MANDIR)/man8/sv-shutdown.8
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(LIBDIR)|g' \
		-e 's|/run/|$(RUNDIR)/|g' \
		-e 's|\(_PATH_STDPATH=\).*$$|\1$(_PATH_STDPATH)|g' \
		-e 's|\(__SV_PREFIX__=\).*$$|\1$(PREFIX)|g' \
		-i $(DESTDIR)$(SV_LIBDIR)/sh/runscript-functions \
		   $(DESTDIR)$(SYSCONFDIR)/sv.conf
ifneq ($(dist_SVC_SED),)
	sed $(dist_SVC_SED) \
		-i $(dist_RS_SVCS:%=$(DESTDIR)$(SV_SVCDIR)/%) \
		   $(dist_SV_SVCS:%=$(DESTDIR)$(SV_SVCDIR)/%/OPTIONS*)
endif
	sed $(INIT_STAGE_SED) -i $(DESTDIR)$(SV_LIBDIR)/sh/init-stage \
		$(dist_INIT_STAGE:%=$(DESTDIR)$(SYSCONFDIR)/%)
	for svc in $(dist_SVC_INSTANCES); do \
		$(LN_S) -f "$${svc#*:}" $(DESTDIR)$(SV_SVCDIR)/$${svc%:*}; \
	done
	$(LN_S) -f $(dist_SYSINIT:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR)/.sysinit/
	$(LN_S) -f $(dist_SYSBOOT:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR)/.sysboot/
	$(LN_S) -f $(dist_DEFAULT:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR)/.default/
	$(LN_S) -f $(dist_SHUTDOWN:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR)/.shutdown/
	$(LN_S) -f $(dist_SINGLE:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR)/.single/
install-dist: install-dir $(DISTFILES)
	$(install_DATA)   $(dist_EXTRA)   $(DESTDIR)$(DOCDIR)
install-dir :
	$(MKDIR_P) $(DISTDIRS:%=$(DESTDIR)%)
install-sv-svcs: $(dist_SV_SVCS) $(dist_SV_LOGS)
$(dist_SV_SVCS): install-dir
	$(MKDIR_P) $(DESTDIR)$(SV_SVCDIR)/$@
	$(LN_S) $(SV_LIBDIR)/sh/cmd $(DESTDIR)$(SV_SVCDIR)/$@/run
	$(LN_S) $(SV_LIBDIR)/sh/cmd $(DESTDIR)$(SV_SVCDIR)/$@/finish
	$(install_DATA) sv/$@/OPTIONS $(DESTDIR)$(SV_SVCDIR)/$@/
$(dist_SV_LOGS): install-dir
	$(MKDIR_P) $(DESTDIR)$(SV_SVCDIR)/$@/log
	$(LN_S) $(SV_LIBDIR)/sh/cmd $(DESTDIR)$(SV_SVCDIR)/$@/log/run
	$(LN_S) $(SV_LIBDIR)/sh/cmd $(DESTDIR)$(SV_SVCDIR)/$@/log/finish

$(dist_SCRIPTS): FORCE
	$(install_SCRIPT) $@ $(DESTDIR)$(SYSCONFDIR)/$@
$(dist_SV_OPTS): $(dist_SV_SVCS) $(dist_SV_LOGS)
	$(install_DATA)  sv/$@ $(DESTDIR)$(SV_SVCDIR)/$@

.PHONY: uninstall uninstall-doc uninstall-dist

uninstall: uninstall-doc
	rm -f $(DESTDIR)$(SV_SVCDIR).conf $(DESTDIR)$(VIMDIR)/syntax/sv.vim \
		$(DESTDIR)$(SBINDIR)/sv-stage $(DESTDIR)$(SBINDIR)/sv-shutdown \
		$(DESTDIR)$(SBINDIR)/rs \
		$(DESTDIR)$(MANDIR)/man5/supervision.5 \
		$(DESTDIR)$(MANDIR)/man8/sv-stage.8 $(DESTDIR)$(MANDIR)/man8/rs.8 \
		$(DESTDIR)$(MANDIR)/man8/sv-shutdown.8
	rm -f $(dist_SCRIPTS:%=$(DESTDIR)$(SYSCONFDIR)/%)
	for svc in $(dist_SVC_INSTANCES); do \
		rm -f $(DESTDIR)$(SV_SVCDIR)/$${svc%:*}; \
	done
	rm -f  $(dist_RS_SVCS:%=$(DESTDIR)$(SV_SVCDIR)/%) \
	       $(dist_RS_SVCS:%=$(DESTDIR)$(SV_SVCDIR).conf.d/%) \
	       $(dist_RS_OPTS:%=$(DESTDIR)$(SV_SVCDIR).conf.d/%)
	rm -fr $(dist_SV_SVCS:%=$(DESTDIR)$(SV_SVCDIR)/%) \
		$(DESTDIR)$(SV_LIBDIR)/cache
	rm -f $(dist_BINS_SYMLINKS:%=$(DESTDIR)$(SV_LIBDIR)/bin/%) \
		$(dist_RS_SYMLINKS:%=$(DESTDIR)$(SV_LIBDIR)/sbin/%) \
		$(dist_SHUTDOWN_SYMLINKS:%=$(DESTDIR)$(SV_LIBDIR)/sbin/%) \
		$(dist_SV_BINS:%=$(DESTDIR)$(SV_LIBDIR)/bin/%) \
		$(dist_SV_SBINS:%=$(DESTDIR)$(SV_LIBDIR)/sbin/%) \
		$(dist_RS_SYMLINKS:%=$(DESTDIR)$(SV_LIBDIR)/sbin/%) \
		$(dist_SH_BINS:%=$(DESTDIR)$(SV_LIBDIR)/sh/%) \
		$(dist_SH_LIBS:%=$(DESTDIR)$(SV_LIBDIR)/sh/%) \
		$(DESTDIR)$(SV_SVCDIR)/getty-tty* \
		$(DESTDIR)$(SV_SVCDIR)/.[ds]*/*
	-rmdir $(dist_DIRS:%=$(DESTDIR)%)
uninstall-doc:
	rm -f $(dist_EXTRA:%=$(DESTDIR)$(DOCDIR)/%)

.PHONY: clean

clean:

