# Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

noinst_LIBRARIES = libxio.a

INCLUDES = @LIBXML2_CFLAGS@ -D_REENTRANT

AM_CFLAGS = -DLINUX -Wall -fPIC -DPIC
libxio_a_SOURCES = libxio.c

EXTRA_DIST = static_xio.h xio.h
