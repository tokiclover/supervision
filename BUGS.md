udev:
----

udev seems to require a resynchronization before starting an X session;
so, `rs udevadm zap; rs udevadm start` is necessary to safely start X;
otherwise, no input would be available! Well, it could happen...
So, udevadm should be started as late as possible to populate `/dev` (which
should be the opposite!) udev is bogus here. Even tried to add conditionaly
`--noudevsync` to {device-mapper,lvm} without success:

    [ -d ${SV_RUNDIR}/udev ] || dmsetup_opts="${dmsetup_opts} --noudevsync"
	[ -d ${SV_RUNDIR}/udev ] || lvm_opts="--noudevsync"
