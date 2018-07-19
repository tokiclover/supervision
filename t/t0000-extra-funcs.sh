#!/bin/sh

NULL=/dev/null
SV_LIBDIR=lib
source $SV_LIBDIR/sh/functions
source $SV_LIBDIR/extra-functions

list="a b c d a e r d s b"
echo "raw list: $list"
filtered="$(filter_out $list)"
echo "filtered list: $filtered"
if [ "$list" = "$filtered" ]; then
	error "broken filtering"
fi

push_array test_a 0 1 2 3 4 5
if [ $(array_size test_a) != 6 ]; then
	error "wrong array size"
fi

while :; do
	e=$(pop_array test_a)
	[ -n "$e" ] && echo $e || break
	break
done

push_hash test_h k1 test k2 TEST ky 0
echo "keys: $(keys test_h)"
echo "values: $(values test_h)"
echo "pop_hash: $(pop_hash test_h)"
