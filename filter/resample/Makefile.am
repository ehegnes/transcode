# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/filter \
	-I$(top_srcdir)/src \
	$(FFMPEG_LIBS_CFLAGS)

pkgdir = $(MOD_PATH)

pkg_LTLIBRARIES = filter_resample.la

filter_resample_la_SOURCES = filter_resample.c
filter_resample_la_LDFLAGS = -module -avoid-version
filter_resample_la_LIBADD = $(FFMPEG_LIBS_LIBS)
