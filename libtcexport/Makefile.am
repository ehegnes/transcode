# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libtc \
	-I$(top_srcdir)/libtcmodule \
	-I$(top_srcdir)/libtcutil

noinst_LTLIBRARIES = libtcexport.la

libtcexport_la_SOURCES = \
	encoder.c \
	export.c \
	multiplexor.c

EXTRA_DIST = \
	encoder.h \
	export.h \
	multiplexor.h

