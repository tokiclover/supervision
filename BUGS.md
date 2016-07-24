udev:
----

udev seems to require a resynchronization before starting an X session;
so, `rs udevadm zap; rs udevadm start` is necessary to safely start X;
otherwise, no input would be available!
