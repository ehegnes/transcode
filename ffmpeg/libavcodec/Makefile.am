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

if CAN_COMPILE_C_ALTIVEC
FF_SRC_ALTIVEC = ppc/dsputil_altivec.c ppc/mpegvideo_altivec.c ppc/idct_altivec.c \
		 ppc/fft_altivec.c ppc/gmc_altivec.c
ALTIVEC=-maltivec -mabi=altivec
endif

AM_CFLAGS = -O2 -g -Wall -ffast-math $(ALTIVEC) $(X86) $(MMX) -DHAVE_AV_CONFIG_H -D_GNU_SOURCE --include=../av_config.h

noinst_LIBRARIES = libtcavcodec.a

libtcavcodec_a_SOURCES = \
      common.c utils.c mem.c allcodecs.c \
      mpegvideo.c jrevdct.c jfdctfst.c jfdctint.c\
      mpegaudio.c ac3enc.c mjpeg.c resample.c dsputil.c \
      motion_est.c imgconvert.c imgresample.c \
      mpeg12.c mpegaudiodec.c pcm.c simple_idct.c \
      ratecontrol.c adpcm.c eval.c dv.c error_resilience.c \
      fft.c mdct.c mace.c huffyuv.c cyuv.c opts.c raw.c h264.c golomb.c \
      vp3.c asv1.c 4xm.c cabac.c ffv1.c ra144.c ra288.c vcr1.c \
      h263.c msmpeg4.c h263dec.c svq1.c rv10.c wmadec.c indeo3.c \
      cljr.c $(FF_SRC_MMX) $(FF_SRC_PPC) $(FF_SRC_ALTIVEC)

noinst_HEADERS =  \
  ac3.h \
  ac3tab.h \
  avcodec.h \
  bswap.h \
  cabac.h \
  common.h \
  dsputil.h \
  dvdata.h \
  fastmemcpy.h \
  golomb.h \
  h263data.h \
  h264data.h \
  imgconvert_template.h \
  indeo3data.h \
  mpeg12data.h \
  mpeg4data.h \
  mpegaudio.h \
  mpegaudiodectab.h \
  mpegaudiotab.h \
  mpegvideo.h \
  msmpeg4data.h \
  os_support.h \
  ra144.h \
  ra288.h \
  simple_idct.h \
  svq1_cb.h \
  svq1_vlc.h \
  vp3data.h \
  wmadata.h


EXTRA_DIST = motion_est_template.c \
	     wmv2.c \
	     svq3.c \
	     faad.c \
	     mdec.c \
	     amr.c

SUBDIRS = . i386 ppc
