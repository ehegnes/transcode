# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

AC3_LIB=-L../libac3 -lac3_tc
AC3_INC=-I../libac3 -I$(srcdir)/../libac3

INCLUDES = -I./ -I../ -I$(srcdir)/.. -I../src -I$(srcdir)/../src -I../avilib -I$(srcdir)/../avilib @AVIFILE_CFLAGS@ -I../encore2 -I$(srcdir)/../encore2  @LAME_CFLAGS@ $(AC3_INC) @QT_CFLAGS@ -Wall @DV_CFLAGS@ -I../libvo -I$(srcdir)/../libvo @MJPEG_CFLAGS@ @LIBFAME_CFLAGS@ @MAGICK_CFLAGS@ @LIBJPEG_CFLAGS@ -I../libioaux -I$(srcdir)/../libioaux

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

#if HAVE_ASM_NASM
if TRANSCODE_0_6
MODS7 = export_mpeg.la 
endif
#endif

if HAVE_AVIFILE
if HAVE_LAME

if AVIFILE_0_6
MODS1 = export_af6.la
else
MODS1 = export_divx.la export_iv50.la 
endif
endif
endif 

if HAVE_DV
MODS5 = export_dvraw.la

if HAVE_LAME
MODS6 = export_dv.la
else
MODS6 = 
endif

else
MODS5 = 
endif

if HAVE_LAME
MODS2 = export_opendivx.la export_raw.la export_divx4.la export_divx4raw.la export_ppm.la export_xvid.la export_xvidraw.la export_xvidcvs.la export_divx5.la export_ffmpeg4.la 
endif

if HAVE_LIBFAME
MOD9 = export_fame.la 
endif

if HAVE_QT
MODS3 = export_mov.la
endif

if HAVE_MJPEG
if HAVE_LAME
MODS8 = export_yuv4mpeg.la export_mpeg2enc.la export_mp2enc.la export_ffmpeg.la
endif
endif

if NET_STREAM
if TRANSCODE_0_6
MODS4 = export_net.la
endif
endif

if HAVE_MAGICK
MODS10=export_im.la
endif

if HAVE_LIBJPEG
if HAVE_LAME
MODS11=export_mjpeg.la
endif
endif

pkg_LTLIBRARIES = export_null.la export_wav.la export_pcm.la $(MODS1) $(MODS2) $(MODS3) $(MODS4) $(MODS5) $(MODS6) $(MODS7) $(MODS8) $(MOD9) $(MODS10) export_ogg.la export_toolame.la $(MODS11)

export_null_la_SOURCES = export_null.c
export_null_la_LDFLAGS = -no-undefined -module -avoid-version 

export_net_la_SOURCES = export_net.c
export_net_la_LDFLAGS = -no-undefined -module -avoid-version 

export_toolame_la_SOURCES = export_toolame.c
export_toolame_la_LDFLAGS = -no-undefined -module -avoid-version 

export_ogg_la_SOURCES = export_ogg.c
export_ogg_la_LDFLAGS = -no-undefined -module -avoid-version 

export_wav_la_SOURCES = export_wav.c
export_wav_la_LDFLAGS = -no-undefined -module -avoid-version 
export_wav_la_LIBADD = -L../avilib -lavi -lm 

export_pcm_la_SOURCES = export_pcm.c
export_pcm_la_LDFLAGS = -no-undefined -module -avoid-version 
export_pcm_la_LIBADD = -L../avilib -lavi -lm 

export_mp2enc_la_SOURCES = export_mp2enc.c
export_mp2enc_la_LDFLAGS = -no-undefined -module -avoid-version 

export_divx_la_SOURCES = export_divx.cpp aud_aux.c
export_divx_la_LDFLAGS = -no-undefined -module -avoid-version 
export_divx_la_LIBADD = @LAME_LIBS@ @AVIFILE_LIBS@ -lstdc++ $(AC3_LIB) -L../avilib -lavi -lm 

export_iv50_la_SOURCES = export_iv50.cpp aud_aux.c
export_iv50_la_LDFLAGS = -no-undefined -module -avoid-version 
export_iv50_la_LIBADD =  @LAME_LIBS@ @AVIFILE_LIBS@ -lstdc++ $(AC3_LIB) -L../avilib -lavi -lm 

export_fame_la_SOURCES = export_fame.c aud_aux.c
export_fame_la_LDFLAGS = -no-undefined -module -avoid-version 
export_fame_la_LIBADD = @LAME_LIBS@ @LIBFAME_LIBS@ -L../avilib $(AC3_LIB) -lavi -lm

export_af6_la_SOURCES = export_af6.cpp af6_aux.cpp aud_aux.c
export_af6_la_LDFLAGS = -no-undefined -module -avoid-version 
export_af6_la_LIBADD = @LAME_LIBS@ @AVIFILE_LIBS@ -lstdc++ $(AC3_LIB) -L../avilib -lavi -lm 

export_opendivx_la_SOURCES = export_opendivx.c aud_aux.c
export_opendivx_la_LDFLAGS = -no-undefined -module -avoid-version 
export_opendivx_la_LIBADD = @LAME_LIBS@ -L../encore2 -lencore2 -L../avilib $(AC3_LIB) -lavi -lm

export_divx4_la_SOURCES = export_divx4.c aud_aux.c divx4_vbr.c
export_divx4_la_LDFLAGS = -no-undefined -module -avoid-version 
export_divx4_la_LIBADD = @LAME_LIBS@ -L../avilib $(AC3_LIB) -lavi -lm

export_divx5_la_SOURCES = export_divx5.c aud_aux.c divx4_vbr.c
export_divx5_la_LDFLAGS = -no-undefined -module -avoid-version 
export_divx5_la_LIBADD = @LAME_LIBS@ -L../avilib $(AC3_LIB) -lavi -lm

export_ffmpeg4_la_SOURCES = export_ffmpeg4.c aud_aux.c divx4_vbr.c
export_ffmpeg4_la_LDFLAGS = -no-undefined -module -avoid-version 
export_ffmpeg4_la_LIBADD = @LAME_LIBS@ -L../ffmpeg/libavcodec/ -lavcodec -L../avilib $(AC3_LIB) -lavi -lm

export_divx4raw_la_SOURCES = export_divx4raw.c aud_aux.c divx4_vbr.c
export_divx4raw_la_LDFLAGS = -no-undefined -module -avoid-version 
export_divx4raw_la_LIBADD =  @LAME_LIBS@ -L../avilib $(AC3_LIB) -lavi -lm

export_xvid_la_SOURCES = export_xvid.c aud_aux.c xvid_vbr.c
export_xvid_la_LDFLAGS = -no-undefined -module -avoid-version 
export_xvid_la_LIBADD = @LAME_LIBS@ -L../avilib -L../libioaux -lioaux $(AC3_LIB) -lavi -lm

export_xvidraw_la_SOURCES = export_xvidraw.c aud_aux.c xvid_vbr.c
export_xvidraw_la_LDFLAGS = -no-undefined -module -avoid-version 
export_xvidraw_la_LIBADD = @LAME_LIBS@ -L../avilib -L../libioaux -lioaux $(AC3_LIB) -lavi -lm

export_xvidcvs_la_SOURCES = export_xvidcvs.c aud_aux.c xvid_vbr.c
export_xvidcvs_la_LDFLAGS = -no-undefined -module -avoid-version 
export_xvidcvs_la_LIBADD = @LAME_LIBS@ -L../avilib -L../libioaux -lioaux $(AC3_LIB) -lavi -lm 

export_raw_la_SOURCES = export_raw.c aud_aux.c
export_raw_la_LDFLAGS = -no-undefined -module -avoid-version 
export_raw_la_LIBADD = @LAME_LIBS@ -L../avilib -lavi $(AC3_LIB) -lm

export_ppm_la_SOURCES = export_ppm.c aud_aux.c
export_ppm_la_LDFLAGS = -no-undefined -module -avoid-version 
export_ppm_la_LIBADD = @LAME_LIBS@ -L../avilib -lavi $(AC3_LIB) -L../libvo -lvout -lm

export_mov_la_SOURCES = export_mov.c 
export_mov_la_LDFLAGS = -no-undefined -module -avoid-version 
export_mov_la_LIBADD = @QT_LIBS@ -lm

export_dvraw_la_SOURCES = export_dvraw.c 
export_dvraw_la_LDFLAGS = -no-undefined -module -avoid-version 
export_dvraw_la_LIBADD = -L../libdvenc -ldvenc @DV_LIBS@ -lm

export_dv_la_SOURCES = export_dv.c aud_aux.c
export_dv_la_LDFLAGS = -no-undefined -module -avoid-version 
export_dv_la_LIBADD = @LAME_LIBS@ -L../avilib -lavi -L../libdvenc -ldvenc @DV_LIBS@ $(AC3_LIB) -lm

export_yuv4mpeg_la_SOURCES = export_yuv4mpeg.c aud_aux.c vid_aux.c
export_yuv4mpeg_la_LDFLAGS = -no-undefined -module -avoid-version 
export_yuv4mpeg_la_LIBADD = @LAME_LIBS@ @MJPEG_LIBS@ -L../libvo -lvout $(AC3_LIB) -L../avilib -lavi -lm

export_mpeg_la_SOURCES = export_mpeg.c vid_aux.c
export_mpeg_la_LDFLAGS = -no-undefined -module -avoid-version 
export_mpeg_la_LIBADD = -L../bbmpeg -lbbmpeg -L../ffmpeg/libavcodec/ -lavcodec -L../libvo -lvout -lm

export_ffmpeg_la_SOURCES = export_ffmpeg.c vid_aux.c aud_aux.c 
export_ffmpeg_la_LDFLAGS = -no-undefined -module -avoid-version 
export_ffmpeg_la_LIBADD = @LAME_LIBS@ -L../ffmpeg/libavcodec/ -L../libvo -lvout $(AC3_LIB) -lavcodec -L../avilib -lavi -lm

export_mjpeg_la_SOURCES = export_mjpeg.c vid_aux.c aud_aux.c 
export_mjpeg_la_LDFLAGS = -no-undefined -module -avoid-version 
export_mjpeg_la_LIBADD = @LIBJPEG_LIBS@ @LAME_LIBS@ -L../libvo -lvout $(AC3_LIB) -L../avilib -lavi -lm

export_mpeg2enc_la_SOURCES = export_mpeg2enc.c vid_aux.c
export_mpeg2enc_la_LDFLAGS = -no-undefined -module -avoid-version 
export_mpeg2enc_la_LIBADD = @MJPEG_LIBS@ -L../libvo -lvout -lm

export_im_la_SOURCES = export_im.c vid_aux.c
export_im_la_LDFLAGS = -no-undefined -module -avoid-version 
export_im_la_LIBADD = @MAGICK_LIBS@ -L../libvo -lvout -lm 

pkg_DATA = export_af6.conf xvid.cfg

EXTRA_DIST = *.h *.conf *.cfg



