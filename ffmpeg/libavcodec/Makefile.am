# # Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -I../../ 

if HAVE_MMX
FF_SRC_MMX = i386/fdct_mmx.c i386/cputest.c \
	i386/dsputil_mmx.c i386/mpegvideo_mmx.c \
	i386/idct_mmx.c i386/motion_est_mmx.c \
	i386/simple_idct_mmx.c
MMX = -DHAVE_MMX
endif


if ARCH_X86
X86=-DARCH_X86
endif

if ARCH_PPC
FF_SRC_PPC = ppc/dsputil_ppc.c ppc/mpegvideo_ppc.c
endif

if HAVE_PPC_ALTIVEC
FF_SRC_ALTIVEC = ppc/dsputil_altivec.c ppc/mpegvideo_altivec.c ppc/idct_altivec.c
ALTIVEC=-faltivec
endif

AM_CFLAGS = -O3 -g -Wall -ffast-math $(ALTIVEC) $(X86) $(MMX) -DHAVE_AV_CONFIG_H -D_GNU_SOURCE --include=../av_config.h

noinst_LIBRARIES = libavcodec.a

libavcodec_a_SOURCES = \
 common.c utils.c mem.c allcodecs.c \
      mpegvideo.c h263.c jrevdct.c jfdctfst.c jfdctint.c\
      mpegaudio.c ac3enc.c mjpeg.c resample.c dsputil.c \
      motion_est.c imgconvert.c imgresample.c msmpeg4.c \
      mpeg12.c h263dec.c svq1.c rv10.c mpegaudiodec.c pcm.c simple_idct.c \
      ratecontrol.c adpcm.c eval.c dv.c error_resilience.c \
      wmadec.c fft.c mdct.c mace.c \
      huffyuv.c $(FF_SRC_MMX) $(FF_SRC_PPC) $(FF_SRC_ALTIVEC)

noinst_HEADERS = *.h

SUBDIRS = . i386 ppc
