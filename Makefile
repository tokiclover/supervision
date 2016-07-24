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

INSTALL    ?= install
install_SCRIPT = $(INSTALL) -m 755
install_DATA   = $(INSTALL) -m 644
MKDIR_P    ?= mkdir -p

dist_EXTRA  = \
	$(DIST_EXTRA) \
	AUTHORS \
	COPYING \
	README.md \
	TODO \
	BUGS.md \
	ChangeLog
dist_COMMON = \
	sv/.opt/OPTIONS.in \
	sv/.opt/SVC_OPTIONS \
	sv/.opt/SVC_BACKEND
dist_SH_BINS  = \
	sv/.lib/bin/checkpath \
	sv/.lib/bin/fstabinfo \
	sv/.lib/bin/mountinfo \
	sv/.lib/bin/sv-config \
	sv/.lib/bin/sv-shutdown \
	sv/.lib/sh/tmpfiles \
	sv/.lib/sh/runscript \
	sv/.lib/sh/init-stage \
	sv/.lib/sh/cgroup-release-agent \
	sv/.lib/sh/dep
dist_SH_LIBS  = \
	sv/.lib/sh/cgroup-functions \
	sv/.lib/sh/functions \
	sv/.lib/sh/runscript-functions \
	sv/.lib/sh/supervision-functions
dist_SV_RUNS  =
dist_SCRIPTS  = \
	sv/.opt/cmd
dist_SV_SVCS  = \
	$(EXTRA_SUPERVISION_SERVICES) \
	acpid \
	atd \
	cgred \
	cron \
	cupsd \
	cups-browsed \
	dhcp \
	dbus \
	dns \
	getty \
	git-daemon \
	gpm \
	hostapd \
	pcscd \
	rsync-daemon \
	saned \
	snmpd \
	sulogin \
	syslog \
	inetd \
	httpd \
	ntp \
	sshd \
	wpa_supplicant \
	xdm \
	zed
dist_VIRTUALS  = \
	$(EXTRA_VIRTUAL_SERVICES) \
	tmpfiles.setup:tmpfiles.dev \
	swapfiles:swaps \
	networkfs:localfs \
	dev:mdev \
	net:dhcp \
	socklog-inet:syslog socklog-ucspi:syslog socklog-unix:syslog
dist_SV_OPTS  = \
	dns/OPTIONS.dnsmasq \
	dhcp/OPTIONS.dhcpcd dhcp/OPTIONS.dhcpd \
	cron/OPTIONS.cronie cron/OPTIONS.dcron \
	cron/OPTIONS.fcron cron/OPTIONS.vixie-cron \
	httpd/OPTIONS.busybox-httpd httpd/OPTIONS.lighttpd \
	inetd/OPTIONS.ipsvd inetd/OPTIONS.xinetd \
	ntp/OPTIONS.busybox-ntpd ntp/OPTIONS.ntpd \
	syslog/OPTIONS.rsyslog syslog/OPTIONS.socklog syslog/OPTIONS.syslog-ng

dist_RS_SVCS  = \
	$(EXTRA_RUNSCRIPT_SERVICES) \
	checkfs \
	clock \
	console \
	devfs \
	dmesg \
	hostname \
	kmod \
	kmod-static-nodes \
	localfs \
	loopback \
	mdev \
	miscfs \
	mtab \
	procfs \
	rdonlyfs \
	rootfs \
	sysctl \
	sysfs \
	swaps \
	tmpdirs \
	tmpfiles.dev \
	zfs \
	zfs-share \
	zpool
dist_RS_OPTS  = \
	ip6tables \
	dev

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
	$(EXTRA_STAGE_2)
dist_STAGE_3 = \
	$(EXTRA_STAGE_3) \
	rdonlyfs


ifdef RUNIT_INIT_STAGE
dist_COMMON  += runit/reboot
dist_SCRIPTS += runit/1 runit/2 runit/3 runit/ctrlaltdel
dist_DIRS    += $(SYSCONFDIR)/runit
endif
ifdef S6_INIT_STAGE
dist_SCRIPTS += s6/crash s6/finish s6/init-stage-1
dist_DIRS    += $(SYSCONFDIR)/s6
endif

ifdef SYSVINIT
dist_SH_BINS += sv/.lib/bin/initctl
dist_SV_SVCS += initctl
endif

DISTFILES   = $(dist_COMMON) $(dist_EXTRA) \
	$(dist_SV_OPTS) $(dist_SV_SVCS) \
	$(dist_RS_OPTS) $(dist_RS_SVCS) \
	$(dist_SH_BINS) $(dist_SH_LIBS) \
	$(dist_SCRIPTS) $(dist_SV_RUNS:%=%/RUN)
dist_DIRS  += \
	$(SBINDIR) $(LIBDIR)/sv/bin $(LIBDIR)/sv/sh $(DOCDIR) \
	$(SYSCONFDIR)/sv.conf.d $(SYSCONFDIR)/sv/.opt $(SYSCONFDIR)/sv/.stage-0 \
	$(SYSCONFDIR)/sv/.stage-1 $(SYSCONFDIR)/sv/.stage-2 $(SYSCONFDIR)/sv/.stage-3 \
	$(MANDIR)/man5 $(MANDIR)/man8 $(SYSCONFDIR)/sv/.single
DISTDIRS    = $(dist_DIRS)

define svc_cmd =
	for cmd in finish run; do \
		ln -s $(subst log,..,$(2))../.opt/cmd \
		$(DESTDIR)$(SYSCONFDIR)/sv/$(1)/$(2)/$${cmd}; \
	done
endef
define stage_sym =
	for svc in $(2); do \
		ln -fs $(SYSCONFDIR)/sv/$${svc} $(DESTDIR)$(SYSCONFDIR)/sv/.stage-$(1)/$${svc}; \
	done
endef

.PHONY: FORCE all install install-doc install-dist install-all install-svc-dir

all: $(SUBDIRS)

FORCE:

$(SUBDIRS): FORCE
	$(MAKE) -C $@

install-svc-dir:
	$(MKDIR_P) $(dist_SV_SVCS:%=$(DESTDIR)$(SYSCONFDIR)/sv/%)
install-all: install install-supervision-svc
install: install-dir install-dist
	$(install_SCRIPT) sv.conf $(DESTDIR)$(SYSCONFDIR)/sv.conf
	$(install_SCRIPT) src/rs $(DESTDIR)$(SBINDIR)
	$(install_DATA) -D sv.vim $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	sed -e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' -e 's|@LIBDIR@|$(LIBDIR)|g' \
		-e 's|@SBINDIR@|$(SBINDIR)|g' \
		-e 's|@RUNDIR@|$(RUNDIR)|g' \
		supervision.5 >$(DESTDIR)$(MANDIR)/man5/supervision.5
	$(install_DATA) rs.8 $(DESTDIR)$(MANDIR)/man8
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(LIBDIR)|g' \
		-e 's|/run/|$(RUNDIR)/|g' \
		-e 's|/sbin/rs|$(SBINDIR)/rs|g' \
		-i $(DESTDIR)$(LIBDIR)/sv/sh/runscript-functions \
		$(DESTDIR)$(SYSCONFDIR)/sv/.opt/SVC_OPTIONS
	for svc in $(dist_VIRTUALS); do \
		ln -fs $${svc#*:} $(DESTDIR)$(SYSCONFDIR)/sv/$${svc%:*}; \
	done
	for i in 1 2 3 4 5 6; do \
		ln -s getty $(DESTDIR)$(SYSCONFDIR)/sv/getty-tty$${i}; \
	done
	for i in 0 1 2 3; do \
		echo >$(DESTDIR)$(SYSCONFDIR)/sv/.stage-$${i}/.keep_dir-stage-$${i}; \
	done
	for i in 1 2 3 4 5 6; do \
		ln -s ../getty-tty$${i} \
			$(DESTDIR)$(SYSCONFDIR)/sv/.stage-2/getty-tty$${i}; \
	done
	$(call stage_sym,0,$(dist_STAGE_0))
	$(call stage_sym,1,$(dist_STAGE_1))
	$(call stage_sym,2,$(dist_STAGE_2))
	$(call stage_sym,3,$(dist_STAGE_3))
	ln -fs $(LIBDIR)/sv $(DESTDIR)$(SYSCONFDIR)/sv/.lib 
	ln -fs $(SYSCONFDIR)/sv/sulogin $(DESTDIR)$(SYSCONFDIR)/sv/.single
install-dist: $(DISTFILES)
install-dir :
	$(MKDIR_P) $(dist_DIRS:%=$(DESTDIR)%)
install-doc : $(dist_EXTRA)

%/RUN: %
	$(install_SCRIPT) sv/$@ $(DESTDIR)$(SYSCONFDIR)/sv/$@
$(dist_COMMON): FORCE
	$(install_DATA) $@ $(DESTDIR)$(SYSCONFDIR)/$@
$(dist_SCRIPTS): FORCE
	$(install_SCRIPT) $@ $(DESTDIR)$(SYSCONFDIR)/$@
$(dist_EXTRA): FORCE
	$(install_DATA) $@ $(DESTDIR)$(DOCDIR)/$@
$(dist_SH_BINS): FORCE
	$(install_SCRIPT) $@ $(DESTDIR)$(subst sv/.lib,$(LIBDIR)/sv,$@)
$(dist_SH_LIBS): FORCE
	$(install_DATA) $@ $(DESTDIR)$(subst sv/.lib,$(LIBDIR)/sv,$@)
$(dist_SV_SVCS): FORCE install-svc-dir
	if test -d sv/$@/log; then \
		$(MKDIR_P) $(DESTDIR)$(SYSCONFDIR)/sv/$@/log; \
		$(call svc_cmd,$@,log/); \
	fi
	-$(install_DATA)  sv/$@/OPTIONS $(DESTDIR)$(SYSCONFDIR)/sv/$@/OPTIONS
	-$(call svc_cmd,$@)
$(dist_SV_OPTS): $(dist_SV_SVCS)
	$(install_DATA)  sv/$@ $(DESTDIR)$(SYSCONFDIR)/sv/$@
$(dist_RS_SVCS):
	$(install_SCRIPT) sv/$@ $(DESTDIR)$(SYSCONFDIR)/sv/$@
	-$(install_DATA)  sv.conf.d/$@ $(DESTDIR)$(SYSCONFDIR)/sv.conf.d/$@
$(dist_RS_OPTS):
	$(install_DATA)  sv.conf.d/$@ $(DESTDIR)$(SYSCONFDIR)/sv.conf.d/$@
install-%-svc:
	$(MKDIR_P) $(DESTDIR)$(RC_CONFDIR)
	$(MKDIR_P) $(DESTDIR)$(RC_INITDIR)
	$(install_SCRIPT) $*.initd $(DESTDIR)$(RC_INITDIR)/$*
	$(install_DATA)   $*.confd $(DESTDIR)$(RC_CONFDIR)/$*

.PHONY: uninstall uninstall-doc uninstall-dist uninstall-all

uninstall-all: uninstall unintsall-supervision-svc
uninstall: uninstall-doc
	rm -f $(DESTDIR)$(SYSCONFDIR)/sv.conf
	rm -f $(DESTDIR)$(SBINDIR)/rs
ifdef SYSVINIT
	rm -f $(DESTDIR)$(LIBDIR)/sv/bin/initctl
endif
	rm -f $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	rm -f $(DESTDIR)$(MANDIR)/man5/supervision.5* $(DESTDIR)/$(MANDIR)/man8/rs.8*
	rm -f $(dist_COMMON:%=$(DESTDIR)$(SYSCONFDIR)/%)
	rm -f $(dist_SCRIPTS:%=$(DESTDIR)$(SYSCONFDIR)/%)
	for svc in $(dist_VIRTUALS); do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/sv/$${svc%:*}; \
	done
	rm -f  $(dist_RS_SVCS:%=$(DESTDIR)$(SYSCONFDIR)/sv/%) \
	       $(dist_RS_SVCS:%=$(DESTDIR)$(SYSCONFDIR)/sv.conf.d/%) \
	       $(dist_RS_OPTS:%=$(DESTDIR)$(SYSCONFDIR)/sv.conf.d/%)
	rm -fr $(dist_SV_SVCS:%=$(DESTDIR)$(SYSCONFDIR)/sv/%)
	for file in $(subst sv/.lib,$(LIBDIR)/sv,$(dist_SH_BINS)) \
		$(subst sv/.lib,$(LIBDIR)/sv,$(dist_SH_LIBS)); do \
		rm -f $(DESTDIR)$${file}; \
	done
	for dir in lib opt stage-; do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/sv/.$${dir}*; \
	done
	-rmdir $(dist_DIRS:%=$(DESTDIR)%)
uninstall-doc:
	rm -f $(dist_EXTRA:%=$(DESTDIR)$(DOCDIR)
uninstall-%-svc:
	rm -f $(DESTDIR)$(svcconfdir)/$*
	rm -f $(DESTDIR)$(svcinitdir)/$*
	-rmdir $(DESTDIR)$(svcconfdir)
	-rmdir $(DESTDIR)$(svcinitdir)

.PHONY: clean

clean:

