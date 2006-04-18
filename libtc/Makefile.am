# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src

noinst_LTLIBRARIES = libtc.la

if !HAVE_GETOPT_LONG_ONLY
GETOPT_FILES = getopt.c getopt1.c
endif

if HAVE_IBP
XIO_FILES = libxio.c
endif

libtc_la_SOURCES = \
	cfgfile.c \
	framecode.c \
	iodir.c \
	optstr.c \
	ratiocodes.c \
	strlcat.c \
	strlcpy.c \
	tc_functions.c \
	tccodecs.c \
	tcmodule.c \
	tcmoduleinfo.c \
	$(GETOPT_FILES) \
	$(XIO_FILES)

EXTRA_DIST = \
	cfgfile.h \
	framecode.h \
	getopt.h \
	iodir.h \
	libtc.h \
	optstr.h \
	ratiocodes.h \
	static_xio.h \
	tc_lzo.h \
	tccodecs.h \
	tcmodule-core.h \
	tcmodule-data.h \
	tcmodule-info.h \
	tcmodule-plugin.h \
	xio.h
