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

if HAVE_AVIFILE
MOD = import_af6.la
else
MOD =
endif

pkgdir = @MOD_PATH@
pkg_LTLIBRARIES = $(MOD)

import_af6_la_SOURCES = import_af6.c
import_af6_la_LDFLAGS = -no-undefined -module -avoid-version
