AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

CFLAGS = -Wall @CFLAGS@ -I../libvo -I$(srcdir)/../libvo

noinst_LIBRARIES = libmpeg2cvs.a

libmpeg2cvs_a_SOURCES = decode.c header.c stats.c slice.c \
		      motion_comp.c motion_comp_mmx.c \
		      idct.c idct_mlib.c idct_mmx.c \
		      idct_altivec.c motion_comp_altivec.c
noinst_HEADERS = vlc.h mpeg2_internal.h mpeg2.h attributes.h 

EXTRA_DIST = COPYING AUTHORS README

