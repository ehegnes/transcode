# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libtc \
	$(ACLIB_CFLAGS) \
	$(AVILIB_CFLAGS)

pkgdir = $(MOD_PATH)

pkg_LTLIBRARIES = \
	multiplex_raw.la \
	multiplex_avi.la \
	multiplex_null.la

multiplex_avi_la_SOURCES = multiplex_avi.c
multiplex_avi_la_LDFLAGS = -module -avoid-version

multiplex_raw_la_SOURCES = multiplex_raw.c
multiplex_raw_la_LDFLAGS = -module -avoid-version

multiplex_null_la_SOURCES = multiplex_null.c
multiplex_null_la_LDFLAGS = -module -avoid-version

