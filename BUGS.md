udev:
----

udev seems to require a resynchronization before starting an X session;
otherwise, no input would be available! Well, it could happen...

    [ -d ${SV_RUNDIR}/udev ] || dmsetup_opts="${dmsetup_opts} --noudevsync"
	[ -d ${SV_RUNDIR}/udev ] || lvm_opts="--noudevsync"

There is a workaround for this... just wait *udev* to be ready.

**IMPORTANT NOTE:** udev need a system logger to be able to start processing
device events. Otherwise, it will simply refuse to do anything.
Secondly, kernel modules should loaded after udev initialisation; or else, udev
would silently ignore old devices events.
