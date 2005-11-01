# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/filter \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libtc \
	$(X_CFLAGS) \
	$(FREETYPE2_CFLAGS) \
	$(ICONV_CFLAGS)

pkgdir = $(MOD_PATH)

pkg_LTLIBRARIES = filter_subtitler.la

filter_subtitler_la_SOURCES = \
	filter_subtitler.c \
	load_ppml_file.c \
	load_font.c \
	frame_list.c \
	object_list.c \
	x11.c \
	text_processing.c \
	load_pictures.c \
	parser.c \
	external_programs.c \
	color_processor.c

filter_subtitler_la_LDFLAGS = -module -avoid-version

filter_subtitler_la_LIBADD = \
	$(X_LIBS) \
	$(X_PRE_LIBS) \
	-lXext \
	-lX11 \
	$(X_EXTRA_LIBS) \
	-lXpm \
	-lXaw \
	-lXt \
	-lm \
	$(FREETYPE2_LIBS) \
	$(ICONV_LIBS)

EXTRA_DIST = \
	HOWTO_USE_THIS \
	README.COLOR.PROCESSOR \
	README.PPML \
	CHANGES \
	demo.ppml \
	frame_list.h \
	load_font.h \
	object_list.h \
	rose.ppm \
	subtitler.h \
	subtitler_proto.h \
	sun.ppm \
	LICENSE \
	x11.h
