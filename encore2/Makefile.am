# # Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

noinst_HEADERS = vop.h vlc_codes.h TransferIDCT.h timer.h rgb2yuv.h rate_ctl.h quantize.h putvlc.h predictions.h portab.h motion.h max_level.h mad.h halfpel.h enctypes.h encore2.h encoder.h dct.h block.h bitstream.h 
EXTRA_DIST = TransferIDCT.c halfpel.c mad.c Acknowledgement.txt Interface.txt

if HAVE_MMX
MSOURCE = mad_mmx.c halfpel_mmx.c TransferIDCT_mmx.c
MFLAG= -D_MMX_
else
MSOURCE = mad.c halfpel.c TransferIDCT.c
endif

noinst_LIBRARIES = libencore2.a

CFLAGS=-O6 -funroll-loops -ffast-math -DLINUX $(MFLAG) @CFLAGS@

libencore2_a_SOURCES = bitstream.c block.c dct.c \
				encoder.c encore.c \
				motion.c putvlc.c quantize.c \
				rate_ctl.c rgb2yuv.c vop.c \
				predictions.c fast_dct.c timer.c $(MSOURCE)

