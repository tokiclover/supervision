#!/bin/sh

sv/sysfs status
sv/sysfs start
sv/atd/run status

printf "bogus usage\n"
rs stage
printf "\n"

ln -fs rs src/sv-stage
src/sv-stage --default status
