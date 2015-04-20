PACKAGE     = supervision
VERSION     = $(shell sed -nre '3s/(.*):/\1/p' ChangeLog)

PREFIX      = /usr/local
SYSCONFDIR  = /etc
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
	sv/.opt/cgroup-functions \
	sv/.opt/functions \
	sv/.opt/runscript-functions \
	sv/.opt/sv-backend \
	sv/.opt/sv.conf \
	sv/.opt/supervision-functions
dist_RUNS   = 
dist_SCRIPTS= \
	sv/.bin/checkpath \
	sv/.bin/rs \
	sv/.bin/supervision-backend \
	sv/.bin/sv-shutdown \
	sv/.bin/mdev-start-dev \
	sv/.opt/cgroup-release-agent \
	sv/.opt/dep \
	sv/.opt/svc \
	sv/.opt/cmd
dist_SERVICES = \
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
	syslog \
	inetd \
	httpd \
	ntp \
	sshd \
	wpa_supplicant \
	udev
dist_SVC_VIRT = \
	net:dhcp \
	socklog-inet:syslog socklog-ucspi:syslog
dist_SVC_OPTS = \
	dns/OPTIONS.dnsmasq \
	dhcp/OPTIONS.dhcpcd dhcp/OPTIONS.dhcpd \
	cron/OPTIONS.cronie cron/OPTIONS.dcron \
	cron/OPTIONS.fcron cron/OPTIONS.vixie-cron \
	httpd/OPTIONS.busybox-httpd httpd/OPTIONS.lighttpd \
	inetd/OPTIONS.ipsvd inetd/OPTIONS.xinetd \
	ntp/OPTIONS.busybox-ntpd ntp/OPTIONS.ntpd \
	syslog/OPTIONS.rsyslog syslog/OPTIONS.socklog syslog/OPTIONS.syslog-ng
dist_RUNSCRIPTS = \
	squashdir \
	zram

ifdef RUNIT
dist_COMMON  += runit/reboot
dist_SCRIPTS += runit/1 runit/2 runit/3 runit/ctrlaltdel
dist_DIRS    += $(SYSCONFDIR)/runit
endif
ifdef S6
dist_SCRIPTS += s6/crash s6/finish s6/init-stage-1
dist_DIRS    += $(SYSCONFDIR)/s6
endif

DISTFILES   = $(dist_COMMON) $(dist_EXTRA) \
	$(dist_SVC_OPTS) \
	$(dist_RUNSCRIPTS) \
	$(dist_SCRIPTS) $(dist_SERVICES) $(dist_RUNS:%=%/RUN)
dist_DIRS  += \
	$(SYSCONFDIR)/sv/.opt $(SYSCONFDIR)/sv/.bin \
	$(SYSCONFDIR)/service $(SYSCONFDIR)/sv \
	$(SYSCONFDIR)/rs.d \
	$(MANDIR)/man1 \
	$(DOCDIR)/$(PACKAGE)-$(VERSION)
DISTDIRS    = $(dist_DIRS)

ifdef STATIC
getty_CMD   = cp -a sv/getty
dist_SVC_VIRT += cron:fcron dns:dnsmasq \
	httpd:busybox-httpd ntp:busybox-ntpd syslog:socklog
else
getty_CMD   = ln -s $(SYSCONFDIR)/sv/getty
endif
getty_NAME  = $(shell which agetty >/dev/null 2>&1 && echo -n agetty || echo -n getty)

define service_DIR =
	$(MKDIR_P) $(DESTDIR)$(SYSCONFDIR)/sv/$(1)
endef
define service_CMD =
	for cmd in finish run; do \
		ln -s $(subst log,..,$(2))../.opt/cmd \
		$(DESTDIR)$(SYSCONFDIR)/sv/$(1)/$(2)/$${cmd}; \
	done
endef

FORCE:

.PHONY: FORCE all install install-doc install-dist install-all

all:

install-all: install install-supervision-svc
install: install-dir install-dist
	$(install_DATA) -D sv.vim $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	sed -e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' supervision.1 \
		>$(DESTDIR)$(MANDIR)/man1/supervision.1
	for i in 1 2 3 4 5 6; do \
		$(getty_CMD) $(DESTDIR)$(SYSCONFDIR)/service/$(getty_NAME)-tty$${i}; \
	done
	for dir in .bin .opt; do \
		ln -f -s $(SYSCONFDIR)/sv/$${dir} $(DESTDIR)$(SYSCONFDIR)/service/$${dir}; \
	done
ifdef STATIC
	for svc in $(dist_SVC_VIRT); do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/sv/$${svc%:*}; \
	done
endif
	for svc in $(dist_SVC_VIRT); do \
		ln -fs $${svc#*:} $(DESTDIR)$(SYSCONFDIR)/sv/$${svc%:*}; \
	done
	for i in 0 1 2 3; do \
		$(MKDIR_P) $(DESTDIR)$(SYSCONFDIR)/rs.d/stage-$${i}; \
		echo >$(DESTDIR)$(SYSCONFDIR)/rs.d/stage-$${i}/.keep_stage-$${i}; \
	done
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
$(dist_SERVICES): FORCE
	if test -d sv/$@/log; then \
		$(call service_DIR,$@/log); \
		$(call service_CMD,$@,log/); \
	else \
		$(call service_DIR,$@); \
	fi
	-$(install_DATA) sv/$@/OPTIONS $(DESTDIR)$(SYSCONFDIR)/sv/$@/OPTIONS
	-$(call service_CMD,$@)
$(dist_SVC_OPTS): $(dist_SERVICES)
ifdef STATIC
	sh -c 'ARGS=$(subst /OPTIONS,,$@); set $${ARGS/./ }; \
	cp -a $(DESTDIR)$(SYSCONFDIR)/sv/$${1} $(DESTDIR)$(SYSCONFDIR)/sv/$${2}; \
	$(install_DATA) sv/$@ $(DESTDIR)$(SYSCONFDIR)/sv/$${2}/OPTIONS'
else
	$(install_DATA) sv/$@ $(DESTDIR)$(SYSCONFDIR)/sv/$@
endif
$(dist_RUNSCRIPTS):
	$(install_SCRIPT) rs.d/$@ $(DESTDIR)$(SYSCONFDIR)/rs.d/$@
	-$(install_DATA)  rs.d/OPTIONS.$@ $(DESTDIR)$(SYSCONFDIR)/rs.d/OPTIONS.$@
install-%-svc:
	$(MKDIR_P) $(DESTDIR)$(RC_CONFDIR)
	$(MKDIR_P) $(DESTDIR)$(RC_INITDIR)
	$(install_SCRIPT) $*.initd $(DESTDIR)$(RC_INITDIR)/$*
	$(install_DATA)   $*.confd $(DESTDIR)$(RC_CONFDIR)/$*

.PHONY: uninstall uninstall-doc uninstall-dist uninstall-all

uninstall-all: uninstall unintsall-supervision-svc
uninstall: uninstall-doc
	rm -f $(DESTDIR)$(VIMDIR)/syntax/sv.vim
	rm -f $(DESTDIR)$(MANDIR)/man1/supervision.1*
	rm -f $(dist_COMMON:%=$(DESTDIR)$(SYSCONFDIR)/%)
	rm -f $(dist_SCRIPTS:%=$(DESTDIR)$(SYSCONFDIR)/%)
ifdef STATIC
	for svc in $(dist_SVC_OPTS); do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/sv/$${svc#*.}; \
	done
endif
	for dir in $(dist_SERVICES); do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/sv/$${dir}; \
	done
	for svc in $(dist_SVC_VIRT); do \
		rm -f $(DESTDIR)$(SYSCONFDIR)/sv/$${svc%:*}; \
	done
	for dir in .bin .opt $(getty_NAME); do \
		rm -fr $(DESTDIR)$(SYSCONFDIR)/service/$${dir}*; \
	done
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

