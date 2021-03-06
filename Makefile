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
install_EXEC = $(INSTALL) -m 755
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
	ChangeLog-v00 ChangeLog-v0.12 \
	ChangeLog
dist_INIT_SBIN = \
	sv-init
dist_SERVICE_SBIN = \
	sv-run \
	sv-shutdown
dist_SUPERVISION_BIN = \
	supervision \
	svd \
	svp \
	svr
dist_BIN = \
	$(dist_SUPERVISION_BIN)
dist_SBIN = \
	$(dist_INIT_SBIN) \
	$(dist_SERVICE_SBIN)
dist_SV_LIB_SH  = \
	lib/sh/run \
	lib/sh/tmpfiles \
	lib/sh/sv-run.sh \
	lib/sh/sv-init.sh \
	lib/sh/cgroup-release-agent \
	lib/sh/sv-deps.sh
dist_SV_LIB_SBIN = \
	src/sv-config
dist_SV_LIB_NX  = \
	lib/sh/SV-OPTIONS.in \
	lib/sh/SV-CONFIG \
	lib/sh/cgroup-functions \
	lib/sh/functions \
	lib/sh/runscript-functions \
	lib/sh/supervision-functions
dist_SV_LIB_BIN  = \
	src/checkpath \
	src/fstabinfo \
	src/mountinfo \
	src/waitfile
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
	getty.tty1 \
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
	apache2/log \
	cron/log \
	cups-browsed/log \
	cupsd/log \
	dhcp/log \
	dhcpd/log \
	git-daemon/log \
	gpm/log \
	hostapd/log \
	inetd/log \
	libvirtd/log \
	mysql/log \
	nagios/log \
	ntp/log \
	postgresql/log \
	rrdcached/log \
	rsync-daemon/log \
	saned/log \
	snmpd/log \
	snmptrapd/log \
	syslog/log \
	virtlockd/log \
	virtlogd/log \
	wpa_supplicant/log \
	xdm/log \
	zed/log
dist_SV_SVCS_SYM  = \
	$(EXTRA_SERVICE_INSTANCES) \
	getty.tty6:getty.tty1 getty.tty5:getty.tty1 getty.tty4:getty.tty1 \
	getty.tty3:getty.tty1 getty.ttyS0:getty.tty1 getty.ttyS1:getty.tty1 \
	getty.tty2:getty.tty1 \
	tmpfiles.setup:tmpfiles.dev \
	rrdcached.nagios:rrdcached \
	spawn-fcgi.lighttpd:spawn-fcgi.nginx \
	swapfiles:swaps \
	networkfs:localfs \
	wpa_supplicant.wlan0:wpa_supplicant \
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
	SYSBOOT \
	DEFAULT \
	checkfs \
	devfs \
	dmesg \
	hostname \
	kmod-static-nodes \
	libvirt-guests \
	localfs \
	loop-back-device \
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
	urandom \
	vmware \
	zfs \
	zfs-share \
	zpool
dist_RS_OPTS = \
	$(EXTRA_OPTIONS_INSTANCES)
dist_CONFIG_LOCAL = \
	$(EXTRA_CONFIG_LOCAL)
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
	hostname \
	localfs \
	loop-back-device \
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
	local \
	sshd \
	getty.tty6 getty.tty5 getty.tty4 getty.tty3 getty.tty2 getty.tty1
dist_SHUTDOWN = \
	$(EXTRA_SHUTDOWN_SERVICES) \
	rdonlyfs

dist_SV_RUN_SYM = \
	sv-rc \
	sv-rcorder
dist_SV_RUN_LIBEXEC_SYM = rc service
dist_SV_SHUTDOWN_LIBEXEC_SYM = halt poweroff reboot shutdown
dist_SV_LIBEXEC_SYM = \
	envdir \
	envuidgid \
	fghack \
	pgrhack \
	setlock \
	setuidgid \
	softlimit
dist_INIT_MAN = \
	sv-init.8
dist_SERVICE_MAN = \
	supervision.5 \
	sv-rc.8 \
	sv-run.8 \
	sv-rcorder.8 \
	sv-shutdown.8
dist_SUPERVISION_MAN = \
	supervision.1 \
	svd.1 \
	svp.1 \
	svr.1
dist_MAN = \
	$(dist_SUPERVISION_MAN) \
	$(dist_INIT_MAN) \
	$(dist_SERVICE_MAN)

ifeq ($(RUNIT_INIT_STAGE),yes)
dist_RUNIT_INIT_SH += runit/1 runit/2 runit/3 runit/ctrlaltdel runit/reboot
dist_RUNIT_INIT_D += $(DATADIR)/$(PACKAGE)/runit
endif
ifeq ($(S6_INIT_STAGE),yes)
dist_S6_INIT_SH += s6/crash s6/finish s6/init
dist_S6_INIT_D  += $(DATADIR)/$(PACKAGE)/s6
endif

ifeq ($(SYSVINIT),yes)
dist_SV_LIB_SBIN += src/initctl
dist_SV_SVCS  += initctl
endif

dist_INIT = \
	$(dist_INIT_SBIN) \
	$(dist_INIT_MAN)
dist_SERVICE = \
	$(dist_SERVICE_SBIN) \
	$(dist_SERVICE_MAN) \
	$(dist_SV_RUN_SYM) \
	$(dist_SV_RUN_LIBEXEC_SYM) \
	$(dist_SV_SHUTDOWN_LIBEXEC_SYM) \
	$(dist_SV_OPTS) $(dist_SV_SVCS) $(dist_SV_LOGS)
dist_SUPERVISION = \
	$(dist_SUPERVISION_BIN) \
	$(dist_SUPERVISION_MAN)

dist_INITD = sysinit sysboot default shutdown single
dist_DIRS  += \
	$(SV_LIBDIR)/bin $(SV_LIBDIR)/sbin $(SV_LIBDIR)/sh $(DOCDIR) \
	$(SV_LIBDIR)/cache $(SV_SVCDIR) $(SV_SVCDIR).conf.local.d \
	$(SV_SVCDIR).conf.d $(dist_INITD:%=$(SV_SVCDIR).init.d/%) \
	$(DATADIR)/$(PACKAGE)
DISTDIRS    = $(SBINDIR) $(MANDIR)/man1 $(MANDIR)/man5 $(MANDIR)/man8 $(dist_DIRS)

dist_SVC_SED  =
ifneq ($(OS),Linux)
ifneq ($(PREFIX),/usr)
dist_SVC_SED = -e 's|/usr/|$(PREFIX)/|g'
else
dist_SVC_SED =
endif
ifneq ($(PREFIX),/usr/local)
dist_LOCAL_SED = -e 's|/usr/local|$(PREFIX)|g'
else
dist_LOCAL_SED =
endif
endif
ifneq ($(EXEC_PREFIX),)
dist_SVC_SED += 's|/sbin/sv-run|$(SBINDIR)/sv-run|g'
endif

dist_MAN_SED += \
	$(dist_LOCAL_SED) \
	-e 's,@EXEC_PREFIX@,$(EXEC_PREFIX),g' \
	-e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' \
	-e 's|@RUNDIR@|$(RUNDIR)|g' \
	-e 's|@LIBDIR@|$(LIBDIR)|g' \
	-e 's|@SBINDIR@|$(SBINDIR)|g' \
	-e 's|@PREFIX@|$(PREFIX)|g' \
	-e 's|@_PATH_NOLOGIN@|$(_PATH_NOLOGIN)|g'
INIT_SH_SED = -e 's|\(PATH=\).*$$|\1$(_PATH_STDPATH)|g' \
	-e 's|\(_PATH_WALL=\).*$$|\1$(_PATH_WALL)|g' \
	-e 's|/etc|$(SYSCONFDIR)|g' \
	-e 's|/lib|$(LIBDIR)|g' \
	-e 's|/run/|$(RUNDIR)/|g'


.PHONY: FORCE all install install-doc install-dist

all: $(SUBDIRS)

FORCE:

$(SUBDIRS): FORCE
	$(MAKE) -C $@

install: install-supervision install-service install-init
install-init: install-dir $(dist_INIT)
install-supervision: install-dir $(dist_SUPERVISION)
install-service: install-dir $(dist_SERVICE)
ifneq ($(OS),Linux)
	sed -e '/^SVC_OPTS=.*$$/d;s,/sbin/sulogin,/bin/login,g' \
		-i $(DESTDIR}$(SYSCONFIDIR)/sv/sulogin/OPTIONS
endif

	$(install_DATA) $(dist_EXTRA)   $(DESTDIR)$(DOCDIR)
	$(install_DATA) $(dist_CONFIG_LOCAL:%=sv.conf.local.d/%) $(DESTDIR)$(SV_SVCDIR).conf.local.d

ifeq ($(RUNIT_INIT_STAGE),yes)
	$(MKDIR_P) $(DESTDIR)$(dist_RUNIT_INIT_D)
	$(install_EXEC) $(dist_RUNIT_INIT_SH) $(DESTDIR)$(dist_RUNIT_INIT_D)
endif
ifeq ($(S6_INIT_STAGE),yes)
	$(MKDIR_P) $(DESTDIR)$(dist_S6_INIT_D)
	$(install_EXEC) $(dist_S6_INIT_SH) $(DESTDIR)$(dist_S6_INIT_D)
endif

	$(install_DATA)  sv.conf $(DESTDIR)$(SV_SVCDIR).conf
	$(install_DATA) -D sv.vim $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	$(install_EXEC) $(dist_SV_LIB_BIN) $(DESTDIR)$(SV_LIBDIR)/bin
	$(install_EXEC) $(dist_SV_LIB_SH) $(DESTDIR)$(SV_LIBDIR)/sh
	$(install_DATA)   $(dist_SV_LIB_NX) $(DESTDIR)$(SV_LIBDIR)/sh
	$(install_EXEC) $(dist_SV_LIB_SBIN) $(DESTDIR)$(SV_LIBDIR)/sbin
	$(install_DATA)   $(dist_RS_OPTS:%=sv.conf.d/%) $(DESTDIR)$(SV_SVCDIR).conf.d
	-$(install_DATA)  $(dist_RS_SVCS:%=sv.conf.d/%) $(DESTDIR)$(SV_SVCDIR).conf.d
	$(install_EXEC) $(dist_RS_SVCS:%=sv/%)        $(DESTDIR)$(SV_SVCDIR)
	$(install_DATA) -D README.local $(DESTDIR)/usr/local$(SV_SVCDIR).local.d/README
	sed -e 's,\(SV_LIBDIR=\).*$$,\1$(SV_LIBDIR)\nSV_SVCDIR=$(SV_SVCDIR),' \
		-i $(DESTDIR)$(SV_LIBDIR)/sh/run
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(LIBDIR)|g' \
		-e 's|/run/|$(RUNDIR)/|g' \
		-e 's|\(_PATH_STDPATH=\).*$$|\1$(_PATH_STDPATH)|g' \
		-e 's|\(__SV_PREFIX__=\).*$$|\1$(PREFIX)|g' \
		$(dist_LOCAL_SED) \
		-i $(DESTDIR)$(SV_LIBDIR)/sh/runscript-functions \
		   $(DESTDIR)$(SYSCONFDIR)/sv.conf
ifneq ($(dist_SVC_SED),)
	sed $(dist_SVC_SED) \
		-i $(dist_RS_SVCS:%=$(DESTDIR)$(SV_SVCDIR)/%) \
		   $(dist_SV_SVCS:%=$(DESTDIR)$(SV_SVCDIR)/%/OPTIONS*)
endif
	sed $(INIT_SH_SED) -i $(DESTDIR)$(SV_LIBDIR)/sh/sv-init.sh \
		$(dist_INIT_SH:%=$(DESTDIR)$(DATADIR)/$(PACKAGE)/%)
	for svc in $(dist_SV_SVCS_SYM); do \
		echo $(LN_S) "$${svc#*:}" "$(DESTDIR)$(SV_SVCDIR)/$${svc%:*}"; \
		$(LN_S) "$${svc#*:}" "$(DESTDIR)$(SV_SVCDIR)/$${svc%:*}"; \
	done
	$(LN_S) $(dist_SYSINIT:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR).init.d/sysinit/
	$(LN_S) $(dist_SYSBOOT:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR).init.d/sysboot/
	$(LN_S) $(dist_DEFAULT:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR).init.d/default/
	$(LN_S) $(dist_SHUTDOWN:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR).init.d/shutdown/
	$(LN_S) $(dist_SINGLE:%=$(SV_SVCDIR)/%) $(DESTDIR)$(SV_SVCDIR).init.d/single/

install-dir :
	$(MKDIR_P) $(DISTDIRS:%=$(DESTDIR)%)

$(dist_SV_SVCS): install-dir
	$(MKDIR_P) $(DESTDIR)$(SV_SVCDIR)/$@
	$(LN_S) $(SV_LIBDIR)/sh/run $(DESTDIR)$(SV_SVCDIR)/$@/run
	$(LN_S) $(SV_LIBDIR)/sh/run $(DESTDIR)$(SV_SVCDIR)/$@/finish
	$(install_DATA) sv/$@/OPTIONS $(DESTDIR)$(SV_SVCDIR)/$@/

$(dist_SV_LOGS): install-dir
	$(MKDIR_P) $(DESTDIR)$(SV_SVCDIR)/$@
	$(LN_S) $(SV_LIBDIR)/sh/run $(DESTDIR)$(SV_SVCDIR)/$@/run
	$(LN_S) $(SV_LIBDIR)/sh/run $(DESTDIR)$(SV_SVCDIR)/$@/finish

$(dist_SV_OPTS): $(dist_SV_SVCS) $(dist_SV_LOGS)
	$(install_DATA)  sv/$@ $(DESTDIR)$(SV_SVCDIR)/$@

$(dist_SV_RUN_SYM): install-dir
	$(LN_S) $(SBINDIR)/sv-run $(DESTDIR)$(SBINDIR)/$@
$(dist_SV_RUN_LIBEXEC_SYM): install-dir
	$(LN_S) $(SBINDIR)/sv-run $(DESTDIR)$(SV_LIBDIR)/sbin/$@
$(dist_SV_SHUTDOWN_LIBEXEC_SYM): install-dir
	$(LN_S) $(SBINDIR)/sv-shutdown $(DESTDIR)$(SV_LIBDIR)/sbin/$@

$(dist_BIN):
	$(install_EXEC) src/$@ $(DESTDIR)$(BINDIR)
$(dist_SBIN):
	$(install_EXEC) src/$@ $(DESTDIR)$(SBINDIR)
%.8: FORCE install-dir
	sed $(dist_MAN_SED) $($@_SED) src/$@ >$(DESTDIR)$(MANDIR)/man8/$@
%.5: FORCE install-dir
	sed $(dist_MAN_SED) $($@_SED) src/$@ >$(DESTDIR)$(MANDIR)/man5/$@
%.1: FORCE install-dir
	sed $(dist_MAN_SED) $($@_SED) src/$@ >$(DESTDIR)$(MANDIR)/man1/$@

.PHONY: uninstall uninstall-doc uninstall-dist

uninstall: uninstall-doc
	rm -f $(DESTDIR)$(SV_SVCDIR).conf $(DESTDIR)$(VIMDIR)/syntax/sv.vim \
		$(dist_BIN:%=$(DESTDIR)$(BINDIR)/%) \
		$(dist_SBIN:%=$(DESTDIR)$(SBINDIR)/%) \
		$(dist_SV_RUN_SYM:%=$(DESTDIR)$(SBINDIR)/%) \
		$(dist_MAN:%=$(DESTDIR)$(MANDIR)/man*/%)
	rm -f $(dist_CONFIG_LOCAL:%=$(DESTDIR)$(SV_SVCDIR).conf.local.d/%)
	for svc in $(dist_SV_SVCS_SYM); do \
		rm -f $(DESTDIR)$(SV_SVCDIR)/$${svc%:*}; \
	done
	rm -f  $(dist_RS_SVCS:%=$(DESTDIR)$(SV_SVCDIR)/%) \
	       $(dist_RS_SVCS:%=$(DESTDIR)$(SV_SVCDIR).conf.d/%) \
	       $(dist_RS_OPTS:%=$(DESTDIR)$(SV_SVCDIR).conf.d/%)
	rm -fr $(dist_SV_SVCS:%=$(DESTDIR)$(SV_SVCDIR)/%)
	rm -fr $(DESTDIR)$(SV_LIBDIR)/cache/* $(dist_INITD:%=$(DESTDIR)$(SV_SVCDIR).init.d/%)
	rm -f $(dist_SV_LIBEXEC_SYM:%=$(DESTDIR)$(SV_LIBDIR)/bin/%) \
		$(dist_SV_RUN_SYM:%=$(DESTDIR)$(SV_LIBDIR)/sbin/%) \
		$(dist_SV_RUN_LIBEXEC_SYM:%=$(DESTDIR)$(SV_LIBDIR)/sbin/%) \
		$(dist_SV_SHUTDOWN_LIBEXEC_SYM:%=$(DESTDIR)$(SV_LIBDIR)/sbin/%) \
		$(dist_SV_LIB_BIN:src/%=$(DESTDIR)$(SV_LIBDIR)/bin/%) \
		$(dist_SV_LIB_SBIN:src/%=$(DESTDIR)$(SV_LIBDIR)/sbin/%) \
		$(dist_SV_RUN_LIBEXEC_SYM:%=$(DESTDIR)$(SV_LIBDIR)/sbin/%) \
		$(dist_SV_LIB_SH:lib/%=$(DESTDIR)$(SV_LIBDIR)/%) \
		$(dist_SV_LIB_NX:lib/%=$(DESTDIR)$(SV_LIBDIR)/%) \
		$(DESTDIR)$(SV_SVCDIR)/getty.tty*
ifeq ($(RUNIT_INIT_STAGE),yes)
	rm -fr $(DESTDIR)$(dist_RUNIT_INIT_D)
endif
ifeq ($(S6_INIT_STAGE),yes)
	rm -fr $(DESTDIR)$(dist_S6_INIT_D)
endif
	-rmdir $(dist_DIRS:%=$(DESTDIR)%)
uninstall-doc:
	rm -f $(dist_EXTRA:%=$(DESTDIR)$(DOCDIR)/%)

.PHONY: clean

clean:

