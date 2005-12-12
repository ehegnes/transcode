# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) -I.. -I../src -I../aclib

#	-I$(top_srcdir)

noinst_LTLIBRARIES = libtc.la

libtc_la_SOURCES = \
	tc_functions.c \
	tccodecs.c \
	tcmodule.c \
	tcmoduleinfo.c \
	optstr.c \
	iodir.c \
	strlcat.c \
	strlcpy.c

EXTRA_DIST = \
	libtc.h \
	tccodecs.h \
	tcmodule-core.h \
	tcmodule-data.h \
	tcmodule-info.h \
	tcmodule-plugin.h \
	iodir.h \
	optstr.h 
