# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = \
	-I$(top_srcdir)/filter \
	-I$(top_srcdir)/src

AM_CPPFLAGS = -D_REENTRANT

pkgdir = @MOD_PATH@

pkg_LTLIBRARIES = filter_resample.la

filter_resample_la_SOURCES = filter_resample.c resample.c soxfuncs.c
filter_resample_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = \
	resampl.h \
	resample.h \
	st.h
