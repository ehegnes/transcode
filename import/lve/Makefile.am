# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = \
	-I$(top_srcdir)/import \
	-I$(top_srcdir)/src
 
AM_CPPFLAGS = \
	-DMOD_PATH=\"@MOD_PATH@\" \
	-D_REENTRANT

AM_CFLAGS = \
	-funroll-loops \
	-ffast-math

pkgdir = @MOD_PATH@
pkg_LTLIBRARIES = import_lve.la

import_lve_la_SOURCES = import_lve.c
import_lve_la_LIBADD  = \
	@LVE_LIBS@ \
	@FFMPEG_LIB_LIBS@ \
	@MPEG2CVS_LIBS@ \
	@A52_LIBS@ \
	-lstdc++
import_lve_la_LDFLAGS = -no-undefined -module -avoid-version
