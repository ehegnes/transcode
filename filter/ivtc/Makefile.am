# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/filter \
	-I$(top_srcdir)/src

AM_CPPFLAGS = -D_REENTRANT

pkgdir = $(MOD_PATH)

pkg_LTLIBRARIES = filter_decimate.la filter_ivtc.la

filter_decimate_la_SOURCES = filter_decimate.c
filter_decimate_la_LDFLAGS = -module -avoid-version

filter_ivtc_la_SOURCES = filter_ivtc.c
filter_ivtc_la_LDFLAGS = -module -avoid-version
