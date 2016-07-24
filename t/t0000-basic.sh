#!/bin/sh

name="${0##*/}"
source sv/.lib/sh/runscript-functions

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

mountinfo /tmp || error "/tmp is not mounted" && end "$?" /tmp
info "/ mount arguments: $(fstabinfo -a /)"

