# # Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

CFLAGS = -O3 -funroll-loops -ffast-math -DLINUX @CFLAGS@

noinst_LIBRARIES = libac3_tc.a

libac3_tc_a_SOURCES = bitstream.c decode.c imdct.c coeff.c exponent.c stats.c\
	bit_allocate.c  parse.c crc.c debug.c rematrix.c downmix.c dither.c \
	sanity_check.c

noinst_HEADERS = stats.h sanity_check.h rematrix.h parse.h imdct.h exponent.h downmix.h dither.h decode.h debug.h crc.h coeff.h bitstream.h bit_allocate.h ac3_internal.h ac3.h 

EXTRA_DIST = COPYING README


