# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

AM_CPPFLAGS = \
	-D_REENTRANT \
	$(LIBXML2_CFLAGS)

noinst_LTLIBRARIES = libxio.la

libxio_la_SOURCES = libxio.c

EXTRA_DIST = \
	static_xio.h \
	xio.h
