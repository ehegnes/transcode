# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

AM_CPPFLAGS = \
	-D_REENTRANT \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	@ACLIB_CFLAGS@

AM_CFLAGS = -fno-omit-frame-pointer

pkgdir = @MOD_PATH@

pkg_LTLIBRARIES = filter_tomsmocomp.la 

filter_tomsmocomp_la_SOURCES = \
	filter_tomsmocomp.c \
	transcoders.c \
	tomsmocompfilter_mmx.c \
	tomsmocompfilter_3dnow.c \
	tomsmocompfilter_sse.c

filter_tomsmocomp_la_LDFLAGS =  -module -avoid-version
filter_tomsmocomp_la_LIBADD = @ACLIB_LIBS@
