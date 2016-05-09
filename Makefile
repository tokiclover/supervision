PACKAGE     = supervision
VERSION     = $(shell sed -nre '3s/(.*):/\1/p' ChangeLog)

SUBDIRS    := src

PREFIX      = /usr/local
SYSCONFDIR  = /etc
SBINDIR     = /sbin
LIBDIR      = /lib
RC_CONFDIR  = $(SYSCONFDIR)/conf.d
RC_INITDIR  = $(SYSCONFDIR)/init.d
DATADIR     = $(PREFIX)/share
DOCDIR      = $(DATADIR)/doc
MANDIR      = $(DATADIR)/man
VIMDIR      = $(DATADIR)/vim/vimfiles

INSTALL     = install
install_SCRIPT = $(INSTALL) -m 755
install_DATA   = $(INSTALL) -m 644
MKDIR_P     = mkdir -p

dist_EXTRA  = \
	AUTHORS \
	COPYING \
	README.md \
	ChangeLog
dist_COMMON = \
	sv/.opt/OPTIONS.in \
	sv/.opt/SVC_OPTIONS \
	sv/.opt/SVC_BACKEND \
	sv/.opt/sv.conf
dist_SH_BINS  = \
	sv/.lib/bin/checkpath \
	sv/.lib/bin/sp \
	sv/.lib/bin/sv-shutdown \
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
	udev
dist_SV_VIRT  = \
	net:dhcp \
	socklog-inet:syslog socklog-ucspi:syslog
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
	devfs \
	mdev \
	squashdir \
	sysfs \
	tmpdir \
	zram
dist_RS_OPTS  = \
	dev
dist_RS_VIRT  = \
	dev:mdev

ifdef RUNIT
dist_COMMON  += runit/reboot
dist_SCRIPTS += runit/1 runit/2 runit/3 runit/ctrlaltdel
dist_DIRS    += $(SYSCONFDIR)/runit
endif
ifdef S6
dist_SCRIPTS += s6/crash s6/finish s6/init-stage-1
dist_DIRS    += $(SYSCONFDIR)/s6
endif

ifdef SYSVINIT
dist_SV_SVCS += initctl
endif

DISTFILES   = $(dist_COMMON) $(dist_EXTRA) \
	$(dist_SV_OPTS) $(dist_SV_SVCS) \
	$(dist_RS_OPTS) $(dist_RS_SVCS) \
	$(dist_SH_BINS) $(dist_SH_LIBS) \
	$(dist_SCRIPTS) $(dist_SV_RUNS:%=%/RUN)
dist_DIRS  += \
	$(SYSCONFDIR)/sv/.opt $(LIBDIR)/sv/bin $(LIBDIR)/sv/sh \
	$(SYSCONFDIR)/service $(SYSCONFDIR)/sv \
	$(SYSCONFDIR)/rs.d \
	$(MANDIR)/man1 \
	$(SBINDIR) \
	$(DOCDIR)/$(PACKAGE)-$(VERSION)
DISTDIRS    = $(dist_DIRS)

ifdef STATIC
getty_CMD   = cp -a sv/getty
dist_SV_VIRT += cron:fcron dns:dnsmasq \
	httpd:busybox-httpd ntp:busybox-ntpd syslog:socklog
else
getty_CMD   = ln -s $(SYSCONFDIR)/sv/getty
endif
getty_NAME  = $(shell which agetty >/dev/null 2>&1 && echo -n agetty || echo -n getty)

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

.PHONY: FORCE all install install-doc install-dist install-all

all: $(SUBDIRS)

FORCE:

$(SUBDIRS): FORCE
	$(MAKE) -C $@ VERSION=$(VERSION)

install-all: install install-supervision-svc
install: install-dir install-dist
	$(install_SCRIPT) src/rs $(DESTDIR)$(SBINDIR)
ifdef SYSVINIT
	$(install_SCRIPT) src/initctl $(DESTDIR)$(LIBDIR)/sv/bin
endif
	$(install_DATA) -D sv.vim $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	sed -e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' -e 's|@LIBDIR@|$(LIBDIR)|g' \
		-e 's|@SBINDIR@|$(SBINDIR)|g' \
		supervision.1 >$(DESTDIR)$(MANDIR)/man1/supervision.1
	sed -e 's|/etc|$(SYSCONFDIR)|g' -e 's|/lib|$(LIBDIR)|g' \
		   -i $(DESTDIR)$(LIBDIR)/sv/sh/runscript-functions \
		   $(DESTDIR)$(SYSCONFDIR)/sv/.opt/SVC_OPTIONS
	for i in 1 2 3 4 5 6; do \
		$(getty_CMD) $(DESTDIR)$(SYSCONFDIR)/service/$(getty_NAME)-tty$${i}; \
		$(getty_CMD) $(DESTDIR)$(SYSCONFDIR)/sv/$(getty_NAME)-tty$${i}; \
	done
	ln -fns $(LIBDIR)/sv $(DESTDIR)$(SYSCONFDIR)/sv/.lib
	for dir in .lib .opt; do \
		ln -fns $(SYSCONFDIR)/sv/$${dir} $(DESTDIR)$(SYSCONFDIR)/service/$${dir}; \
	done
ifdef STATIC
	$(call rem_sym,sv,$(dist_SV_VIRT))
endif
	$(call svc_sym,sv,$(dist_SV_VIRT))
	$(call svc_sym,rs.d,$(dist_RS_VIRT))
	for i in 0 1 2 3; do \
		$(MKDIR_P) $(DESTDIR)$(SYSCONFDIR)/rs.d/stage-$${i}; \
		echo >$(DESTDIR)$(SYSCONFDIR)/rs.d/stage-$${i}/.keep_stage-$${i}; \
	done
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
	$(install_DATA) $@ $(DESTDIR)$(DOCDIR)/$(PACKAGE)-$(VERSION)/$@
$(dist_SH_BINS): FORCE
	$(install_SCRIPT) $@ $(DESTDIR)$(subst sv/.lib,$(LIBDIR)/sv,$@)
$(dist_SH_LIBS): FORCE
	$(install_DATA) $@ $(DESTDIR)$(subst sv/.lib,$(LIBDIR)/sv,$@)
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
ifdef STATIC
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
	rm -f $(DESTDIR)$(SBINDIR)/rs
ifdef SYSVINIT
	rm -f $(DESTDIR)$(LIBDIR)/sv/bin/initctl
endif
	rm -f $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	rm -f $(DESTDIR)$(MANDIR)/man1/supervision.1*
	rm -f $(dist_COMMON:%=$(DESTDIR)$(SYSCONFDIR)/%)
	rm -f $(dist_SCRIPTS:%=$(DESTDIR)$(SYSCONFDIR)/%)
ifdef STATIC
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
	for file in $(subst sv/.lib,$(LIBDIR)/sv,$(dist_SH_BINS)) \
		$(subst sv/.lib,$(LIBDIR)/sv,$(dist_SH_LIBS)); do \
		rm -f $(DESTDIR)$${file}; \
	done
	for dir in .lib .opt $(getty_NAME); do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/service/$${dir}*; \
	done
	rm -fr $(DESTDIR)$(SYSCONFDIR)/sv/.[ol]*
	rm -fr $(DESTDIR)$(SYSCONFDIR)/rs.d/stage-*
	-rmdir $(dist_DIRS:%=$(DESTDIR)%)
uninstall-doc:
	rm -f $(dist_EXTRA:%=$(DESTDIR)$(DOCDIR)/$(PACKAGE)-$(VERSION)/%)
uninstall-%-svc:
	rm -f $(DESTDIR)$(svcconfdir)/$*
	rm -f $(DESTDIR)$(svcinitdir)/$*
	-rmdir $(DESTDIR)$(svcconfdir)
	-rmdir $(DESTDIR)$(svcinitdir)

.PHONY: clean

clean:

