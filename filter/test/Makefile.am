# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/filter \
	-I$(top_srcdir)/src \
	$(ACLIB_CFLAGS)

pkgdir = $(MOD_PATH)

pkg_LTLIBRARIES = filter_test.la

filter_test_la_SOURCES = filter_test.c
filter_test_la_LDFLAGS = -module -avoid-version
filter_test_la_LIBADD = $(ACLIB_LIBS)
