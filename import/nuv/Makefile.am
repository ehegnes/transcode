# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/import \
	-I$(top_srcdir)/src

pkgdir = $(MOD_PATH)
pkg_LTLIBRARIES = import_nuv.la 

import_nuv_la_SOURCES = \
	RTjpegN.c \
	import_nuv.c  \
	minilzo.c  \
	resample.c \
	rtjpeg_aud_plugin.c \
	rtjpeg_vid_plugin.c \
	soxfuncs.c
import_nuv_la_LDFLAGS = -module -avoid-version
import_nuv_la_LIBADD = -lm

EXTRA_DIST =  \
	LICENSE.Nuppelvideo \
	LICENSE.rtjpeg \
	README.rtjpeg \
	RTjpegN.h \
	lzoconf.h \
	minilzo.h \
	nuppelvideo.h \
	resampl.h \
	resample.h \
	rtjpeg_aud_plugin.h \
	rtjpeg_vid_plugin.h \
	st.h
