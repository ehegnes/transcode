# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = \
	-I$(top_srcdir)/filter \
	-I$(top_srcdir)/src \
	@FFMPEG_LIBS_CFLAGS@


AM_CPPFLAGS = -D_REENTRANT

pkgdir = @MOD_PATH@

pkg_LTLIBRARIES = filter_resample.la

filter_resample_la_SOURCES = filter_resample.c
filter_resample_la_LDFLAGS = -module -avoid-version
filter_resample_la_LIBADD = @FFMPEG_LIBS_LIBS@
