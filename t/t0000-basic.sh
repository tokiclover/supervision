#!/bin/sh

name="${0##*/}"
SV_LIBDIR=sv/.lib
[ -L sv/.lib/bin/checkpath ] || ln -s ../../../src/checkpath sv/.lib/bin
[ -L sv/.lib/bin/fstabinfo ] || ln -s ../../../src/fstabinfo sv/.lib/bin
[ -L sv/.lib/bin/mountinfo ] || ln -s ../../../src/mountinfo sv/.lib/bin
SV_LIBDIR=$SV_LIBDIR source sv/.lib/sh/runscript-functions

if yesno Enable; then
	eval_colors 256
	echo -e "$COLOR_BLD${BG_8}$name$COLOR_RST$COLOR_ITA${FG_18}TEST$COLOR_RST"
fi

begin "making /tmp/$name.XXXXXX"
testdir="$(checkpath -d -m0700 -p /tmp $name.XXXXXXX)"
end "$?"
checkpath -d -m0770 -g cdrom $testdir &&
ls -d -l $testdir &&
rmdir $testdir

device_info -a rtc && info "rtc device found" || warn "no rtc device found"

mountinfo -P '^/(tmp|run)$' -T '^(tmpfs|cgroup|sysfs)$'
mountinfo /tmp || error "/tmp is not mounted" && end "$?" /tmp
info "/ mount arguments: $(fstabinfo -a /)"

echo BOOT_IMAGE=$(get_boot_option BOOT_IMAGE)
