# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

AM_CFLAGS = -O3 -fomit-frame-pointer
AM_CPPFLAGS = \
	-D_REENTRANT \
	-I$(top_srcdir) \
	-I$(top_srcdir)/filter \
	-I$(top_srcdir)/src \
	@LIBVO_CFLAGS@

pkgdir = @MOD_PATH@

pkg_LTLIBRARIES = filter_pp.la

filter_pp_la_SOURCES = filter_pp.c postprocess.c
filter_pp_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = \
	postprocess.h \
	postprocess_internal.h \
	postprocess_template.c \
	mangle.h
