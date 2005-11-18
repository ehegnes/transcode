# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS)

#	-I$(top_srcdir)

noinst_LTLIBRARIES = libtc.la

libtc_la_SOURCES = \
	tc_functions.c \
	tccodecs.c \
	tcmoduleinfo.c \
	optstr.c \
	iodir.c \
	strlcat.c \
	strlcpy.c

EXTRA_DIST = \
	libtc.h \
	tccodecs.h \
	tcmoduleinfo.h \
	iodir.h \
	optstr.h 
