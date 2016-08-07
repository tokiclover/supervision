#!/bin/sh

sv/sysfs status
sv/sysfs start
sv/atd/run status

printf "bogus usage\n"
rs stage
printf "\n"

runscript=/lib/sv/sh/runscript
mv ${runscript}{,-} || exit

cat <<EOF >$runscript
#!/bin/sh
echo "\$@"
exit 1
EOF
chmod +x $runscript
printf "starting stage-0\n"
rs -0 stage
printf "\ndefault runlevel\n"
rs default

mv -f ${runscript}{-,} || exit

