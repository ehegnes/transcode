# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-I$(top_srcdir)

noinst_LTLIBRARIES = libtcioaux.la

libtcioaux_la_SOURCES = \
	configs.c \
	configs.h \
	framecode.c \
	framecode.h

EXTRA_DIST = framecode.l
