#!/bin/sh

ln -fs src/sv-run lib/sbin/service
lib/sbin/service sysfs status
sv/sysfs start
sv/atd/run status

printf "bogus usage\n"
src/sv-run stage
printf "\n"

ln -fs sv-run src/sv-rc
src/sv-rc --default status
