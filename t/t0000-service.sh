#!/bin/sh

ln -fs src/rs lib/sbin/service
lib/sbin/service sysfs status
sv/sysfs start
sv/atd/run status

printf "bogus usage\n"
src/rs stage
printf "\n"

ln -fs rs src/sv-stage
src/sv-stage --default status
