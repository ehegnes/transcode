# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -I./ -I../../src -I../ -I../../ -I$(srcdir)/../../src -I$(srcdir)/../.. -I$(srcdir)/.. $(X_CFLAGS) @FT2_CFLAGS@

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

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
color_processor.c \
subtitle_fontname_list.c \
subfont.c
 

filter_subtitler_la_LDFLAGS =  -module -avoid-version 
filter_subtitler_la_LIBADD = $(X_LIBS) $(X_PRE_LIBS) -lXext -lX11 $(X_EXTRA_LIBS) -lXpm -lXt -lXaw -lm @FT2_LIBS@

EXTRA_DIST =  \
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

