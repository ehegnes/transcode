#!/bin/bash

# FIXME: default args,

# get program path, if given
TCMODCHAIN="tcmodchain"
if [ -n "$1" ]; then
	TCMODCHAIN="$1"
fi

if [ ! -x "$TCMODCHAIN" ]; then
	echo "missing tcmodchain program, test aborted" 1>&2
	exit 1
fi

# test helper

# $1, $2 -> modules
# $3, expected result
function check_test() {
	if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ]; then
		echo "bad test parameters (skipped)" 1>&2
		return 1
	fi
	$TCMODCHAIN -C $1 $2 -d 0 # silent operation
	local RET="$?"
	if [ "$RET" == "$3" ]; then
		printf "testing (%16s) with (%16s) | OK\n" $1 $2
	else
		printf "testing (%16s) with (%16s) | >> FAILED << [exp=%i|got=%i]\n" $1 $2 $3 $RET
	fi
	return $RET
}

## `check' (-C) tests first
#
check_test "encode:null" "multiplex:null" 0
check_test "encode:copy" "multiplex:null" 0
check_test "encode:xvid" "multiplex:null" 0
check_test "encode:x264" "multiplex:null" 0
check_test "encode:lame" "multiplex:null" 0
check_test "encode:faac" "multiplex:null" 0
check_test "encode:lzo"  "multiplex:null" 0
#
check_test "encode:null" "multiplex:raw" 0
check_test "encode:copy" "multiplex:raw" 0
check_test "encode:xvid" "multiplex:raw" 0
check_test "encode:x264" "multiplex:raw" 0
check_test "encode:lame" "multiplex:raw" 0
check_test "encode:faac" "multiplex:raw" 0
check_test "encode:lzo"  "multiplex:raw" 0
#
check_test "encode:null" "multiplex:avi" 0
check_test "encode:copy" "multiplex:avi" 0
check_test "encode:xvid" "multiplex:avi" 0
check_test "encode:x264" "multiplex:avi" 0
check_test "encode:lame" "multiplex:avi" 0
check_test "encode:faac" "multiplex:avi" 0
check_test "encode:lzo"  "multiplex:avi" 0
#
# see manpage for return code meaning
check_test "encode:null" "multiplex:y4m" 0
check_test "encode:copy" "multiplex:y4m" 0
check_test "encode:xvid" "multiplex:y4m" 3
check_test "encode:x264" "multiplex:y4m" 3
check_test "encode:lame" "multiplex:y4m" 3
check_test "encode:faac" "multiplex:y4m" 3
check_test "encode:lzo"  "multiplex:y4m" 3
#
