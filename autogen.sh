#!/bin/sh

# bail on errors
set -e

# setup the environment.  may need adjusting, see your OS
# manuals for details.
AUTORECONF_ENV="AUTOCONF_VERSION=2.59 AUTOMAKE_VERSION=1.8.3"
CONFIGURE_ENV="CPPFLAGS=-I/usr/local/include LDFLAGS=-L/usr/local/lib"

# clean some junk
rm -rf autotools autom4te.cache config.h.in configure libtool

# generate configure and *.in
env $AUTORECONF_ENV autoreconf -fi

# create Makefiles/see if configure is sane
env $CONFIGURE_ENV ./configure

# create a distribution tarball
make dist-gzip

exit 0
