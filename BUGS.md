udev:
----

udev seems to require a resynchronization before starting an X session;
otherwise, no input would be available! Well, it could happen...

    [ -d ${SV_RUNDIR}/udev ] || dmsetup_opts="${dmsetup_opts} --noudevsync"
	[ -d ${SV_RUNDIR}/udev ] || lvm_opts="--noudevsync"

There is a workaround ofr this... just wait *udev* to be ready.
