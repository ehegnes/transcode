# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS)

#	-I$(top_srcdir)

noinst_LTLIBRARIES = libtc.la

libtc_la_SOURCES = \
	tc_functions.c \
	strlcat.c \
	strlcpy.c

EXTRA_DIST = \
	libtc.h \
	tc_lzo.h \
	tc_func_excl.h
