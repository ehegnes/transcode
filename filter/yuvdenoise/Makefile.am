# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall -I./ -I../.. -I../../src/ -I$(srcdir)/../.. -I$(srcdir)/../../src -I../../libvo
AM_CFLAGS = -funroll-all-loops -finline-functions -ffast-math -frerun-cse-after-loop -frerun-loop-opt -fexpensive-optimizations -fmove-all-movables

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = \
         filter_yuvdenoise.la \
	 filter_yuvmedian.la

filter_yuvdenoise_la_SOURCES = filter_yuvdenoise.c deinterlace.c denoise.c motion.c 
filter_yuvdenoise_la_LDFLAGS =  -module -avoid-version -L../../libvo -lvout

filter_yuvmedian_la_SOURCES = filter_yuvmedian.c 
filter_yuvmedian_la_LDFLAGS =  -module -avoid-version

EXTRA_DIST = deinterlace.h denoise.h global.h mjpeg_types.h motion.h


