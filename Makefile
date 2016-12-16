-include config.mak
PACKAGE    ?= supervision
VERSION     = $(shell sed -nre '3s/(.*):/\1/p' ChangeLog)

SUBDIRS    := src

PREFIX     ?= /usr/local
SYSCONFDIR ?= /etc
SBINDIR    ?= /sbin
LIBDIR     ?= /lib
RC_CONFDIR ?= $(SYSCONFDIR)/conf.d
RC_INITDIR ?= $(SYSCONFDIR)/init.d
DATADIR    ?= $(PREFIX)/share
DOCDIR     ?= $(DATADIR)/doc/$(PACKAGE)-$(VERSION)
MANDIR     ?= $(DATADIR)/man
VIMDIR     ?= $(DATADIR)/vim/vimfiles
RUNDIR     ?= /var/run
libdir      = $(LIBDIR)/sv
confdir     = $(SYSCONFDIR)/sv

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
	sv/.lib/bin/sv-config \
	src/sv-shutdown
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
	sulogin \
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
	console \
	devfs \
	dmesg \
	hostname \
	kmod \
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

dist_STAGE_0 = \
	$(EXTRA_STAGE_0) \
	devfs \
	dmesg \
	kmod-static-nodes \
	sysfs \
	tmpfiles.dev
dist_STAGE_1 = \
	$(EXTRA_STAGE_1) \
	kmod \
	console \
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
dist_STAGE_2 = \
	$(EXTRA_STAGE_2) \
	getty-tty6 getty-tty5 getty-tty4 getty-tty3 getty-tty2 getty-tty1
dist_STAGE_3 = \
	$(EXTRA_STAGE_3) \
	rdonlyfs


ifdef RUNIT_INIT_STAGE
dist_SCRIPTS += runit/1 runit/2 runit/3 runit/ctrlaltdel runit/reboot
dist_DIRS    += $(SYSCONFDIR)/runit
endif
ifdef S6_INIT_STAGE
dist_SCRIPTS += s6/crash s6/finish s6/init-stage-1
dist_DIRS    += $(SYSCONFDIR)/s6
endif

ifdef SYSVINIT
dist_SH_SBINS += sv/.lib/bin/initctl
dist_SV_SVCS  += initctl
endif

DISTFILES   = \
	$(dist_SV_OPTS) \
	$(dist_SCRIPTS) $(dist_SV_RUNS:%=%/RUN)
dist_DIRS  += \
	$(libdir)/bin $(libdir)/sbin $(libdir)/sh $(DOCDIR) \
	$(libdir)/cache $(libdir)/opt \
	$(confdir).conf.d $(confdir)/.stage-0 $(confdir)/.stage-1 \
	$(confdir)/.stage-2 $(confdir)/.stage-3 $(confdir)/.single
DISTDIRS    = $(SBINDIR) $(MANDIR)/man5 $(MANDIR)/man8 $(dist_DIRS)

.PHONY: FORCE all install install-doc install-dist install-all

all: $(SUBDIRS)

FORCE:

$(SUBDIRS): FORCE
	$(MAKE) -C $@

install-all: install install-supervision-initd
install: install-dir install-dist install-sv-svcs
	$(install_DATA)  sv.conf $(DESTDIR)$(confdir).conf
	$(install_SCRIPT) src/rs $(DESTDIR)$(SBINDIR)
	$(LN_S) -f $(SBINDIR)/rs $(DESTDIR)$(libdir)/sbin/rc
	$(LN_S) -f $(SBINDIR)/rs $(DESTDIR)$(libdir)/sbin/service
	$(LN_S) -f sv-shutdown $(DESTDIR)$(libdir)/sbin/halt
	$(LN_S) -f sv-shutdown $(DESTDIR)$(libdir)/sbin/poweroff
	$(LN_S) -f sv-shutdown $(DESTDIR)$(libdir)/sbin/reboot
	$(LN_S) -f sv-shutdown $(DESTDIR)$(libdir)/sbin/shutdown
	$(install_DATA) sv-shutdown.8 $(DESTDIR)$(MANDIR)/man8
	$(install_DATA) -D sv.vim $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	$(install_DATA) $(dist_SH_OPTS:%=sv/.opt/%) $(DESTDIR)$(libdir)/opt
	$(install_SCRIPT) sv/.opt/cmd  $(DESTDIR)$(libdir)/opt
	$(install_SCRIPT) $(dist_SV_BINS) $(DESTDIR)$(libdir)/bin
	$(install_SCRIPT) $(dist_SH_BINS) $(DESTDIR)$(libdir)/sh
	$(install_DATA)   $(dist_SH_LIBS) $(DESTDIR)$(libdir)/sh
	$(install_SCRIPT) $(dist_SH_SBINS) $(DESTDIR)$(libdir)/sbin
	$(install_DATA)   $(dist_RS_OPTS:%=sv.conf.d/%) $(DESTDIR)$(confdir).conf.d
	-$(install_DATA)  $(dist_RS_SVCS:%=sv.conf.d/%) $(DESTDIR)$(confdir).conf.d
	$(install_SCRIPT) $(dist_RS_SVCS:%=sv/%)        $(DESTDIR)$(confdir)
	sed -e 's,\(RS_TYPE.*$$\),\1\nSV_LIBDIR=$(libdir)\nSV_SVCDIR=$(confdir),' \
		-i $(DESTDIR)$(libdir)/opt/cmd
	sed -e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' -e 's|@LIBDIR@|$(LIBDIR)|g' \
		-e 's|@SBINDIR@|$(SBINDIR)|g' \
		-e 's|@RUNDIR@|$(RUNDIR)|g' \
		rs.8 >$(DESTDIR)$(MANDIR)/man8/rs.8
	sed -e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' -e 's|@LIBDIR@|$(LIBDIR)|g' \
		-e 's|@SBINDIR@|$(SBINDIR)|g' \
		-e 's|@RUNDIR@|$(RUNDIR)|g' \
		supervision.5 >$(DESTDIR)$(MANDIR)/man5/supervision.5
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(LIBDIR)|g' \
		-e 's|/run/|$(RUNDIR)/|g' \
		-e 's|/sbin/rs|$(SBINDIR)/rs|g' \
		-i $(DESTDIR)$(libdir)/sh/runscript-functions \
		$(DESTDIR)$(libdir)/opt/SVC_OPTIONS
ifdef RUNIT_INIT_STAGE
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(LIBDIR)|g' \
		-e 's|/run/|$(RUNDIR)/|g' \
		-i $(DESTDIR)$(SYSCONFDIR)/runit/*
endif
ifdef S6_INIT_STAGE
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(LIBDIR)|g' \
		-e 's|/run/|$(RUNDIR)/|g' \
		-i $(DESTDIR)$(SYSCONFDIR)/s6/*
endif
	for svc in $(dist_SVC_INSTANCES); do \
		$(LN_S) -f $${svc#*:} $(DESTDIR)$(confdir)/$${svc%:*}; \
	done
	$(LN_S) -f $(dist_STAGE_0:%=$(confdir)/%) $(DESTDIR)$(confdir)/.stage-0/
	$(LN_S) -f $(dist_STAGE_1:%=$(confdir)/%) $(DESTDIR)$(confdir)/.stage-1/
	$(LN_S) -f $(dist_STAGE_2:%=$(confdir)/%) $(DESTDIR)$(confdir)/.stage-2/
	$(LN_S) -f $(dist_STAGE_3:%=$(confdir)/%) $(DESTDIR)$(confdir)/.stage-3/
	$(LN_S) -f $(libdir)/opt $(DESTDIR)$(confdir)/.opt
	$(LN_S) -f $(confdir)/sulogin $(DESTDIR)$(confdir)/.single
install-dist: $(DISTFILES)
install-dir :
	$(MKDIR_P) $(DISTDIRS:%=$(DESTDIR)%)
install-doc : install-dir
	$(install_DATA)   $(dist_EXTRA)   $(DESTDIR)$(DOCDIR)
install-sv-svcs: install-dir
	cp -r $(dist_SV_SVCS:%=sv/%) $(DESTDIR)$(confdir)

%/RUN: %
	$(install_SCRIPT) sv/$@ $(DESTDIR)$(confdir)/$@
$(dist_SCRIPTS): FORCE
	$(install_SCRIPT) $@ $(DESTDIR)$(SYSCONFDIR)/$@
$(dist_SV_OPTS): install-sv-svcs
	$(install_DATA)  sv/$@ $(DESTDIR)$(confdir)/$@
install-%-initd:
	$(MKDIR_P) $(DESTDIR)$(RC_CONFDIR)
	$(MKDIR_P) $(DESTDIR)$(RC_INITDIR)
	$(install_SCRIPT) $*.initd $(DESTDIR)$(RC_INITDIR)/$*
	$(install_DATA)   $*.confd $(DESTDIR)$(RC_CONFDIR)/$*

.PHONY: uninstall uninstall-doc uninstall-dist uninstall-all

uninstall-all: uninstall unintsall-supervision-initd
uninstall: uninstall-doc
	rm -f $(DESTDIR)$(confdir).conf
	rm -f $(DESTDIR)$(SBINDIR)/rs
ifdef SYSVINIT
	rm -f $(DESTDIR)$(libdir)/sbin/initctl
endif
	rm -f $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	rm -f $(DESTDIR)$(MANDIR)/man5/supervision.5 \
		$(DESTDIR)/$(MANDIR)/man8/rs.8 \
		$(DESTDIR)/$(MANDIR)/man8/sv-shutdown.8
	rm -f $(dist_SCRIPTS:%=$(DESTDIR)$(SYSCONFDIR)/%)
	rm -f $(dist_SH_OPTS:%=$(DESTDIR)$(libdir)/opt/%) $(DESTDIR)$(libdir)/opt/cmd
	for svc in $(dist_SVC_INSTANCES); do \
		rm -f $(DESTDIR)$(confdir)/$${svc%:*}; \
	done
	rm -f  $(dist_RS_SVCS:%=$(DESTDIR)$(confdir)/%) \
	       $(dist_RS_SVCS:%=$(DESTDIR)$(confdir).conf.d/%) \
	       $(dist_RS_OPTS:%=$(DESTDIR)$(confdir).conf.d/%)
	rm -fr $(dist_SV_SVCS:%=$(DESTDIR)$(confdir)/%)
	rm -f $(DESTDIR)$(libdir)/bin/* $(DESTDIR)$(libdir)/sbin/* \
		$(DESTDIR)$(libdir)/sh/* $(DESTDIR)$(libdir)/cache/* \
		$(DESTDIR)$(confdir)/getty-tty* $(DESTDIR)$(confdir)/.opt \
		$(DESTDIR)$(confdir)/.s*/*
	-rmdir $(dist_DIRS:%=$(DESTDIR)%)
uninstall-doc:
	rm -f $(dist_EXTRA:%=$(DESTDIR)$(DOCDIR)/%)
uninstall-%-initd:
	rm -f $(DESTDIR)$(svcconfdir)/$*
	rm -f $(DESTDIR)$(svcinitdir)/$*
	-rmdir $(DESTDIR)$(svcconfdir)
	-rmdir $(DESTDIR)$(svcinitdir)

.PHONY: clean

clean:

