# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/import \
	-I$(top_srcdir)/src \
	$(LIBLVE_CFLAGS)

pkgdir = $(MOD_PATH)
pkg_LTLIBRARIES = import_lve.la

import_lve_la_SOURCES = import_lve.c
import_lve_la_LIBADD  = $(LIBLVE_LIBS)
import_lve_la_LDFLAGS = -no-undefined -module -avoid-version
