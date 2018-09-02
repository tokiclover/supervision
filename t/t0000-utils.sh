#!/bin/sh

if [ -n "${ZSH_VERSION}" ]; then
	emulate sh
	NULLCMD=:
	setopt NO_GLOB_SUBST SH_WORD_SPLIT
	disable -r end
fi

name="${0##*/}"
SV_LIBDIR=lib
[ -L src/checkpath ] || ln -s ../src/checkpath $SV_LIBDIR/bin
[ -L src/fstabinfo ] || ln -s ../src/fstabinfo $SV_LIBDIR/bin
[ -L src/mountinfo ] || ln -s ../src/mountinfo $SV_LIBDIR/bin
SV_LIBDIR=$SV_LIBDIR . $SV_LIBDIR/sh/runscript-functions

if yesno Enable; then
	eval_colors 256
	printf "$color_bld${bg_8}$name$color_rst$color_ita${color_fg_18}test$color_rst\n"
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

case "$(uname -s)" in
	[Ll]inux)
echo BOOT_IMAGE=$(get_boot_option BOOT_IMAGE)
	;;
esac
