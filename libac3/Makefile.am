AUTOMAKE_OPTIONS = 1.3 foreign

AM_CFLAGS = -fPIC -DPIC

noinst_LIBRARIES = libac3_tc.a

libac3_tc_a_SOURCES = \
	ac3.h \
	ac3_internal.h \
	bit_allocate.c bit_allocate.h \
	bitstream.c bitstream.h \
	coeff.c coeff.h \
	crc.c crc.h \
	debug.c debug.h \
	decode.c decode.h \
	dither.c dither.h \
	downmix.c downmix.h \
	exponent.c exponent.h \
	imdct.c imdct.h \
	parse.c parse.h \
	rematrix.c rematrix.h \
	sanity_check.c sanity_check.h \
	stats.c stats.h

EXTRA_DIST = COPYING README
