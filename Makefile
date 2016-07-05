-include config.mak
PACKAGE    ?= supervision
VERSION     = $(shell sed -nre '3s/(.*):/\1/p' ChangeLog)

SUBDIRS    := src

PREFIX     ?= /usr/local
SYSCONFDIR ?= /etc
SBINDIR    ?= $(EXEC_PREFIX)/sbin
LIBDIR     ?= $(EXEC_PREFIX)/lib
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
	AUTHORS \
	COPYING \
	README.md \
	ChangeLog \
	crypttab
dist_COMMON = \
	sv/.opt/OPTIONS.in \
	sv/.opt/SVC_OPTIONS \
	sv/.opt/SVC_BACKEND \
	sv/.opt/sv.conf
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
	$(EXTRA_SV_SERVICES) \
	acpid \
	atd \
	cgred \
	cron \
	cupsd \
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
	udev
dist_SV_VIRT  = \
	$(EXTRA_SV_VIRTUALS) \
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

dist_RS_SVCS  = $(EXTRA_RS_SERVICES) \
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
	rootfs \
	sysctl \
	sysfs \
	swaps \
	tmpdirs \
	tmpfiles.dev \
	zfs
dist_RS_OPTS  = \
	ip6tables \
	dev
dist_RS_VIRT  = \
	$(EXTRA_RS_VIRTUALS) \
	tmpfiles.setup:tmpfiles.dev \
	swapfiles:swaps \
	networkfs:localfs \
	dev:mdev

dist_STAGE_0 = \
	$(EXTRA_STAGE_0) \
	dev \
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
	swaps swapfiles \
	tmpfiles.setup
dist_STAGE_2 = \
	$(EXTRA_STAGE_2)

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
	$(SYSCONFDIR)/sv/.opt $(EXEC_PREFIX)$(LIBDIR)/sv/bin $(EXEC_PREFIX)$(LIBDIR)/sv/sh \
	$(SYSCONFDIR)/service $(SYSCONFDIR)/sv \
	$(SYSCONFDIR)/rs.d \
	$(MANDIR)/man1 \
	$(EXEC_PREFIX)$(SBINDIR) \
	$(DOCDIR)
DISTDIRS    = $(dist_DIRS)

ifdef STATIC_SERVICE
dist_SV_VIRT += fcron:cron dnsmask:dns \
	busybox-httpd:httpd busybox-ntpd:ntp socklog:syslog
endif

define svc_dir =
	$(MKDIR_P) $(DESTDIR)$(SYSCONFDIR)/sv/$(1)
endef
define svc_cmd =
	for cmd in finish run; do \
		ln -s $(subst log,..,$(2))../.opt/cmd \
		$(DESTDIR)$(SYSCONFDIR)/sv/$(1)/$(2)/$${cmd}; \
	done
endef
define svc_sym =
	for svc in $(2); do \
		ln -fs $${svc#*:} $(DESTDIR)$(SYSCONFDIR)/$(1)/$${svc%:*}; \
	done
endef
define rem_sym =
	for svc in $(2); do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/$(1)/$${svc%:*}; \
	done
endef
define svc_opt  =
	$(install_DATA)  $(1) $(DESTDIR)$(SYSCONFDIR)/$(1)
endef
define rem_svc =
	for svc in $(2); do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/$(1)/$${svc}; \
	done
endef

define stage_sym =
	for svc in $(2); do \
		ln -fs $(SYSCONFDIR)/rs.d/$${svc} $(DESTDIR)$(SYSCONFDIR)/rs.d/stage-$(1)/$${svc}; \
	done
endef

.PHONY: FORCE all install install-doc install-dist install-all

all: $(SUBDIRS)

FORCE:

$(SUBDIRS): FORCE
	$(MAKE) -C $@

install-all: install install-supervision-svc
install: install-dir install-dist
	$(install_SCRIPT) src/rs $(DESTDIR)$(EXEC_PREFIX)$(SBINDIR)
	$(install_DATA) -D sv.vim $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	sed -e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' -e 's|@LIBDIR@|$(EXEC_PREFIX)$(LIBDIR)|g' \
		-e 's|@SBINDIR@|$(EXEC_PREFIX)$(SBINDIR)|g' \
		-e 's|@RUNDIR|$(RUNDIR)|g' \
		supervision.1 >$(DESTDIR)$(MANDIR)/man1/supervision.1
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(EXEC_PREFIX)$(LIBDIR)|g' \
		-e 's|/run/|$(RUNDIR)/|g' \
		 -i $(DESTDIR)$(EXEC_PREFIX)$(LIBDIR)/sv/sh/runscript-functions \
		 $(DESTDIR)$(SYSCONFDIR)/sv/.opt/SVC_OPTIONS
	ln -fns $(EXEC_PREFIX)$(LIBDIR)/sv $(DESTDIR)$(SYSCONFDIR)/sv/.lib
	for dir in .lib .opt; do \
		ln -fns $(SYSCONFDIR)/sv/$${dir} $(DESTDIR)$(SYSCONFDIR)/service/$${dir}; \
	done
ifdef STATIC_SERVICE
	$(call rem_sym,sv,$(dist_SV_VIRT))
	for svc in $(dist_SV_VIRT); do \
		cp -a sv/$${svc#*:} $(DESTDIR)$(SYSCONFDIR)/sv/$${svc%:*}; \
		if test -e sv/$${svc#*:}/OPTIONS.$${svc%:*}; then \
			mv -f $(DESTDIR)$(SYSCONFDIR)/sv/$${svc%:*}/OPTIONS.$${svc%:*} \
				$(DESTDIR)$(SYSCONFDIR)/sv/$${svc%:*}/OPTIONS; \
			rm -f $(DESTDIR)$(SYSCONFDIR)/sv/$${svc%:*}/OPTIONS.*; \
		fi; \
	done
	for i in 1 2 3 4 5 6; do \
		cp -a sv/getty $(DESTDIR)$(SYSCONFDIR)/sv/getty-tty$${i}; \
	done
else
	$(call svc_sym,sv,$(dist_SV_VIRT))
	for i in 1 2 3 4 5 6; do \
		ln -s getty $(DESTDIR)$(SYSCONFDIR)/sv/getty-tty$${i}; \
	done
endif
	$(call svc_sym,rs.d,$(dist_RS_VIRT))
	for i in 1 2 3 4 5 6; do \
		ln -s $(SYSCONFDIR)/sv/getty-tty$${i} \
			$(DESTDIR)$(SYSCONFDIR)/service/getty-tty$${i}; \
	done
	for i in 0 1 2 3; do \
		$(MKDIR_P) $(DESTDIR)$(SYSCONFDIR)/rs.d/stage-$${i}; \
		echo >$(DESTDIR)$(SYSCONFDIR)/rs.d/stage-$${i}/.keep_stage-$${i}; \
	done
	$(call stage_sym,0,$(dist_STAGE_0))
	$(call stage_sym,1,$(dist_STAGE_1))
	$(call stage_sym,2,$(dist_STAGE_2))
	for dir in single boot; do \
		$(MKDIR_P) $(DESTDIR)$(SYSCONFDIR)/service/.$${dir}; \
		echo >$(DESTDIR)$(SYSCONFDIR)/service/.$${dir}/.keep_dir-$${dir}; \
	done
	ln -fs $(SYSCONFDIR)/sv/sulogin $(DESTDIR)$(SYSCONFDIR)/service/.single
	ln -fs $(SYSCONFDIR)/sv/syslog  $(DESTDIR)$(SYSCONFDIR)/service/.boot
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
	$(install_SCRIPT) $@ $(DESTDIR)$(subst sv/.lib,$(EXEC_PREFIX)$(LIBDIR)/sv,$@)
$(dist_SH_LIBS): FORCE
	$(install_DATA) $@ $(DESTDIR)$(subst sv/.lib,$(EXEC_PREFIX)$(LIBDIR)/sv,$@)
$(dist_SV_SVCS): FORCE
	if test -d sv/$@/log; then \
		$(call svc_dir,$@/log); \
		$(call svc_cmd,$@,log/); \
	else \
		$(call svc_dir,$@); \
	fi
	-$(call svc_opt,sv/$@/OPTIONS)
	-$(call svc_cmd,$@)
$(dist_SV_OPTS): $(dist_SV_SVCS)
ifdef STATIC_SERVICE
	sh -c 'ARGS=$(subst /OPTIONS,,$@); set $${ARGS/./ }; \
	cp -a $(DESTDIR)$(SYSCONFDIR)/sv/$${1} $(DESTDIR)$(SYSCONFDIR)/sv/$${2}; \
	$(install_DATA) sv/$@ $(DESTDIR)$(SYSCONFDIR)/sv/$${2}/OPTIONS'
else
	$(call svc_opt,sv/$@)
endif
$(dist_RS_SVCS):
	$(install_SCRIPT) rs.d/$@ $(DESTDIR)$(SYSCONFDIR)/rs.d/$@
	-$(call svc_opt,rs.d/OPTIONS.$@)
$(dist_RS_OPTS):
	-$(call svc_opt,rs.d/OPTIONS.$@)
install-%-svc:
	$(MKDIR_P) $(DESTDIR)$(RC_CONFDIR)
	$(MKDIR_P) $(DESTDIR)$(RC_INITDIR)
	$(install_SCRIPT) $*.initd $(DESTDIR)$(RC_INITDIR)/$*
	$(install_DATA)   $*.confd $(DESTDIR)$(RC_CONFDIR)/$*

.PHONY: uninstall uninstall-doc uninstall-dist uninstall-all

uninstall-all: uninstall unintsall-supervision-svc
uninstall: uninstall-doc
	rm -f $(DESTDIR)$(EXEC_PREFIX)$(SBINDIR)/rs
ifdef SYSVINIT
	rm -f $(DESTDIR)$(EXEC_PREFIX)$(LIBDIR)/sv/bin/initctl
endif
	rm -f $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	rm -f $(DESTDIR)$(MANDIR)/man1/supervision.1*
	rm -f $(dist_COMMON:%=$(DESTDIR)$(SYSCONFDIR)/%)
	rm -f $(dist_SCRIPTS:%=$(DESTDIR)$(SYSCONFDIR)/%)
ifdef STATIC_SERVICE
	for svc in $(dist_SV_OPTS); do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/sv/$${svc#*.}; \
	done
endif
	$(call rem_sym,sv,$(dist_SV_VIRT))
	$(call rem_svc,sv,$(dist_SV_SVCS))
	$(call rem_sym,rs.d,$(dist_RS_VIRT))
	$(call rem_svc,rs.d,$(dist_RS_SVCS))
	$(call rem_svc,rs.d,$(dist_RS_OPTS:%=OPTIONS.%))
	$(call rem_svc,rs.d,$(dist_RS_SVCS:%=OPTIONS.%))
	for file in $(subst sv/.lib,$(EXEC_PREFIX)$(LIBDIR)/sv,$(dist_SH_BINS)) \
		$(subst sv/.lib,$(EXEC_PREFIX)$(LIBDIR)/sv,$(dist_SH_LIBS)); do \
		rm -f $(DESTDIR)$${file}; \
	done
	for dir in .lib .opt getty; do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/service/$${dir}*; \
	done
	rm -fr $(DESTDIR)$(SYSCONFDIR)/sv/.[ol]*
	rm -fr $(DESTDIR)$(SYSCONFDIR)/rs.d/stage-*
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

