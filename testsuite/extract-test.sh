#!/bin/bash

for source in $*; do
	dest=$( basename $source )
	awk '/BEGIN_TEST_CODE/ { have_code = 1 } \
	     have_code == 1 && !/_TEST_CODE/ { print $0 } \
	     /END_TEST_CODE/ { have_code = 0 }' < $source > "test-$dest"
done
