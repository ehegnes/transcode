AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = -I$(top_srcdir)/libvo

AM_CFLAGS = -fPIC -DPIC

noinst_LIBRARIES = libmpeg2cvs.a

libmpeg2cvs_a_SOURCES = \
	attributes.h \
	cpu_state.c \
	decode.c \
	header.c \
	idct_altivec.c \
	idct.c \
	idct_mlib.c \
	idct_mmx.c \
	motion_comp_altivec.c \
	motion_comp.c \
	motion_comp_mlib.c \
	motion_comp_mmx.c \
	mpeg2.h \
	mpeg2_internal.h \
	slice.c \
	stats.c \
	vlc.h

EXTRA_DIST = COPYING AUTHORS README
