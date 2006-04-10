# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(LIBXML2_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/aclib

noinst_LTLIBRARIES = libtc.la

if HAVE_IBP
XIO_FILES = libxio.c
endif

libtc_la_SOURCES = \
	tc_functions.c \
	tccodecs.c \
	tcmodule.c \
	tcmoduleinfo.c \
	framecode.c \
	iodir.c \
	optstr.c \
	strlcat.c \
	strlcpy.c \
	$(XIO_FILES)

EXTRA_DIST = \
	libtc.h \
	tccodecs.h \
	tcmodule-core.h \
	tcmodule-data.h \
	tcmodule-info.h \
	tcmodule-plugin.h \
	framecode.h \
	iodir.h \
	optstr.h \
	static_xio.h \
	xio.h
