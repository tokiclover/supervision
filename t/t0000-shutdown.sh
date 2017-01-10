#!/bin/sh

[ -x src/sv-shutdown ] || exit
src/sv-shutdown
[ -L src/halt ] || ln -s sv-shutdown src/halt || exit
src/halt -c
[ -L src/reboot ] || ln -s sv-shutdown src/reboot || exit
src/reboot -m 'test this and that' 'to see wht happen'
[ -L src/shutdown ] || ln -s sv-shutdown src/shutdown || exit
src/shutdown -m 'test this and that' 'to see wht happen'
