# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/filter \
	-I$(top_srcdir)/src

if BIG_ENDIAN
BG = -DBIG_ENDIAN
endif

AM_CPPFLAGS = \
	-D_REENTRANT \
	$(BG)

pkgdir = @MOD_PATH@

pkg_LTLIBRARIES = filter_divxkey.la

filter_divxkey_la_SOURCES = filter_divxkey.c bitstream.c
filter_divxkey_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = bitstream.h
