#!/bin/bash

REF="$( tempfile -p tclst -s .cfg )"
NEW="$( tempfile -p tclst -s .cfg )"
PROG="./test-cfg-filelist"
if [ -n "$1" ]; then
	PROG="$1"
fi


function makelist() {
	echo "[$1]"
	for F in "$1"/*; do
		echo "$F"
	done
}

function testit() {
	makelist "$1"                                     >> "$REF"
	"$PROG" "$REF" "$1" 2>&1 | sed 's/^\[test\]\ //g' >> "$NEW"
	if diff -q "$REF" "$NEW" &> /dev/null ; then
		printf "%-24s OK\n" "($1)"
	else
		printf "%-24s FAILED -> see ($REF|$NEW)\n" "($1)"
		return 1
	fi
	return 0
}

export "TRANSCODE_NO_LOG_COLOR=1"

DIRS="/usr /boot /usr/bin /usr/lib"
I=0
J=0
for DIR in $DIRS; do
	if testit $DIR; then
		let J=$J+1
	else
		break
	fi
	let I=$I+1
done
#rm -f "$REF" "$NEW"

echo "test succesfull/runned = $J/$I"
