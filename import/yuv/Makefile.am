# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/import \
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DMOD_PATH=\"@MOD_PATH@\" \
	-D_REENTRANT

AM_CFLAGS = \
	-funroll-loops \
	-ffast-math

pkgdir = @MOD_PATH@
pkg_LTLIBRARIES = import_yuv.la

import_yuv_la_SOURCES = import_yuv.c
import_yuv_la_LDFLAGS = -no-undefined -module -avoid-version

EXTRA_DIST = Readme
