# # Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -I../../ 

if HAVE_MMX
FF_SRC_MMX = dsputil_mmx.c fdct_mmx.c idct_mmx.c mpegvideo_mmx.c simple_idct_mmx.c motion_est_mmx.c cputest.c
MMX = -DHAVE_MMX
endif


if ARCH_X86
X86=-DARCH_X86
endif

CFLAGS = -O3 -g -Wall -ffast-math $(X86) $(MMX) -DHAVE_AV_CONFIG_H -D_GNU_SOURCE --include=../av_config.h

noinst_LIBRARIES = libavcodec.a

libavcodec_a_SOURCES = ac3enc.c common.c mpegaudio.c utils.c jfdctfst.c dsputil.c mpeg12.c h263.c h263dec.c mpegvideo.c rv10.c mjpeg.c msmpeg4.c motion_est.c $(FF_SRC_MMX) jrevdct.c jfdctint.c fdctdata.c resample.c motion_est.c mpegaudiodec.c pcm.c simple_idct.c ratecontrol.c svq1.c mem.c allcodecs.c  error_resilience.c avcodec.c adpcm.c imgconvert.c imgresample.c eval.c dv.c

noinst_HEADERS = *.h

EXTRA_DIST = mpegvideo_mmx_template.c
