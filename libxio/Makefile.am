# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

noinst_LIBRARIES = libxio.a

INCLUDES = -I./

CFLAGS=-O2 -DLINUX -Wall -fPIC -DPIC $(LIBXML2_CFLAGS) @CFLAGS@ -D_REENTRANT
libxio_a_SOURCES = libxio.c

EXTRA_DIST = xio.h 
