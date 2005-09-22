# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/filter \
	-I$(top_srcdir)/src \
	$(ACLIB_CFLAGS) \
	$(LIBPOSTPROC_CFLAGS)

pkgdir = $(MOD_PATH)

pkg_LTLIBRARIES = filter_pp.la

filter_pp_la_SOURCES = filter_pp.c
filter_pp_la_LIBADD = $(LIBPOSTPROC_LIBS)
filter_pp_la_LDFLAGS = -module -avoid-version
