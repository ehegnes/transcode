
AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(LIBVO_CFLAGS)

noinst_LTLIBRARIES = libmpeg2cvs.la

libmpeg2cvs_la_SOURCES = \
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
