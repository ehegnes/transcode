# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -I./ -I../ -I$(srcdir)/.. -I../src -I$(srcdir)/../src -I../libxio -I$(srcdir)/../libxio @AVILIB_CFLAGS@ @AVIFILE_CFLAGS@ @LAME_CFLAGS@ @AC3_CFLAGS@ @QT_CFLAGS@ -Wall @DV_CFLAGS@ -I../libvo -I$(srcdir)/../libvo @MJPEG_CFLAGS@ @LIBFAME_CFLAGS@ @MAGICK_CFLAGS@ @LIBJPEG_CFLAGS@ @TCIOAUX_CFLAGS@ @LZO_CFLAGS@ @PVM3_CFLAGS@ -I../pvm3 @FFMPEG_LIBS_CFLAGS@

CFLAGS = -D_REENTRANT -DMOD_PATH=\"@MOD_PATH@\" @CFLAGS@

pkgdir=@MOD_PATH@

if HAVE_ASM_NASM
MODS7 = export_mpeg.la 
endif

if HAVE_AVIFILE
if HAVE_LAME
MODS1 = export_af6.la
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
MODS2 = export_raw.la export_divx4.la export_divx4raw.la export_divx5raw.la export_debugppm.la export_ppm.la export_divx5.la export_ffmpeg.la export_lame.la export_pvn.la
endif

if HAVE_LIBFAME
MOD9 = export_fame.la 
endif

if HAVE_QT
MODS3 = export_mov.la
endif

if HAVE_MJPEG
if HAVE_LAME
MODS8 = export_yuv4mpeg.la export_mpeg2enc.la export_mp2enc.la
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
MODS12=export_jpg.la
endif

if HAVE_LZO
if HAVE_LAME
MODS13 = export_lzo.la
endif
endif

if HAVE_LAME
MODS14 = export_xvid2.la export_xvid3.la export_xvid4.la
endif

if HAVE_PVM3
MODS15 = export_pvm.la
endif

pkg_LTLIBRARIES = export_null.la \
		  export_ac3.la \
		  export_mp2.la \
		  export_wav.la \
		  export_pcm.la \
		  export_mp1e.la \
		  $(MODS1) $(MODS2) $(MODS3) $(MODS4) $(MODS5) $(MODS6) $(MODS7) $(MODS8) $(MOD9) $(MODS10) export_ogg.la \
		  export_toolame.la $(MODS11) $(MODS12) $(MODS13) $(MODS14) $(MODS15)

export_null_la_SOURCES = export_null.c
export_null_la_LDFLAGS =  -module -avoid-version

export_net_la_SOURCES = export_net.c
export_net_la_LDFLAGS =  -module -avoid-version

export_toolame_la_SOURCES = export_toolame.c
export_toolame_la_LDFLAGS =  -module -avoid-version

export_ac3_la_SOURCES = export_ac3.c
export_ac3_la_LDFLAGS =  -module -avoid-version

export_mp2_la_SOURCES = export_mp2.c
export_mp2_la_LDFLAGS =  -module -avoid-version

export_mp1e_la_SOURCES = export_mp1e.c vid_aux.c
export_mp1e_la_LDFLAGS =  -module -avoid-version
export_mp1e_la_LIBADD  = -L../libvo -lvout -lm

export_ogg_la_SOURCES = export_ogg.c
export_ogg_la_LDFLAGS =  -module -avoid-version

export_wav_la_SOURCES = export_wav.c
export_wav_la_LDFLAGS =  -module -avoid-version
export_wav_la_LIBADD = -lm

export_jpg_la_SOURCES = export_jpg.c
export_jpg_la_LDFLAGS =  -module -avoid-version
export_jpg_la_LIBADD = @LIBJPEG_LIBS@ -lm

export_pcm_la_SOURCES = export_pcm.c
export_pcm_la_LDFLAGS =  -module -avoid-version
export_pcm_la_LIBADD = -lm

export_mp2enc_la_SOURCES = export_mp2enc.c
export_mp2enc_la_LDFLAGS =  -module -avoid-version
export_mp2enc_la_LIBADD = -lm

export_fame_la_SOURCES = export_fame.c aud_aux.c
export_fame_la_LDFLAGS =  -module -avoid-version
export_fame_la_LIBADD = @LAME_LIBS@ @LIBFAME_LIBS@ @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -lm

export_lame_la_SOURCES = export_lame.c
export_lame_la_LDFLAGS =  -module -avoid-version
export_lame_la_LIBADD = @LAME_LIBS@ -lm

export_af6_la_SOURCES = export_af6.cpp af6_aux.cpp aud_aux.c
export_af6_la_LDFLAGS =  -module -avoid-version
export_af6_la_LIBADD = @LAME_LIBS@ @AVIFILE_LIBS@ -lstdc++ @AC3_LIBS@ -lm

export_divx4_la_SOURCES = export_divx4.c aud_aux.c divx4_vbr.c
export_divx4_la_LDFLAGS =  -module -avoid-version
export_divx4_la_LIBADD = @LAME_LIBS@ @FFMPEG_LIBS_LIBS@ @AC3_LIBS@ -lm

export_divx5_la_SOURCES = export_divx5.c aud_aux.c divx4_vbr.c
export_divx5_la_LDFLAGS =  -module -avoid-version
export_divx5_la_LIBADD = @LAME_LIBS@ @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -lm

export_divx5raw_la_SOURCES = export_divx5raw.c aud_aux.c divx4_vbr.c
export_divx5raw_la_LDFLAGS =  -module -avoid-version
export_divx5raw_la_LIBADD = @LAME_LIBS@ @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -lm

export_ffmpeg_la_SOURCES = export_ffmpeg.c aud_aux.c ffmpeg_cfg.c vid_aux.c
export_ffmpeg_la_LDFLAGS =  -module -avoid-version
export_ffmpeg_la_LIBADD = @LAME_LIBS@ -L../libvo -lvout @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -lm

export_divx4raw_la_SOURCES = export_divx4raw.c aud_aux.c divx4_vbr.c
export_divx4raw_la_LDFLAGS =  -module -avoid-version
export_divx4raw_la_LIBADD = @LAME_LIBS@ @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -lm

export_xvid2_la_SOURCES = export_xvid2.c aud_aux.c xvid_vbr.c
export_xvid2_la_LDFLAGS =  -module -avoid-version
export_xvid2_la_LIBADD = @LAME_LIBS@ @AC3_LIBS@ -lm

export_xvid3_la_SOURCES = export_xvid3.c aud_aux.c xvid_vbr.c
export_xvid3_la_LDFLAGS =  -module -avoid-version
export_xvid3_la_LIBADD = @LAME_LIBS@ @AC3_LIBS@ -lm

export_xvid4_la_SOURCES = export_xvid4.c aud_aux.c
export_xvid4_la_LDFLAGS =  -module -avoid-version
export_xvid4_la_LIBADD = @LAME_LIBS@ @AC3_LIBS@ -lm

export_raw_la_SOURCES = export_raw.c aud_aux.c
export_raw_la_LDFLAGS =  -module -avoid-version
export_raw_la_LIBADD = @LAME_LIBS@ @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -lm

export_lzo_la_SOURCES = export_lzo.c aud_aux.c
export_lzo_la_LDFLAGS =  -module -avoid-version
export_lzo_la_LIBADD = @LAME_LIBS@ @LZO_LIBS@ @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -lm

export_debugppm_la_SOURCES = export_debugppm.c aud_aux.c
export_debugppm_la_LDFLAGS =  -module -avoid-version
export_debugppm_la_LIBADD = @LAME_LIBS@ @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -L../libvo -lvout -lm

export_ppm_la_SOURCES = export_ppm.c aud_aux.c
export_ppm_la_LDFLAGS =  -module -avoid-version
export_ppm_la_LIBADD = @LAME_LIBS@ @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -L../libvo -lvout -lm

export_mov_la_SOURCES = export_mov.c vid_aux.c
export_mov_la_LDFLAGS =  -module -avoid-version
export_mov_la_LIBADD = @QT_LIBS@ -lm

export_dvraw_la_SOURCES = export_dvraw.c vid_aux.c
export_dvraw_la_LDFLAGS =  -module -avoid-version
export_dvraw_la_LIBADD = -L../libdvenc -ldvenc @DV_LIBS@ -L../libvo -lvout -lm

export_dv_la_SOURCES = export_dv.c aud_aux.c vid_aux.c
export_dv_la_LDFLAGS =  -module -avoid-version 
export_dv_la_LIBADD = @LAME_LIBS@ -L../libdvenc -ldvenc @DV_LIBS@ @AC3_LIBS@  @FFMPEG_LIBS_LIBS@ -L../libvo -lvout -lm 

export_yuv4mpeg_la_SOURCES = export_yuv4mpeg.c aud_aux.c vid_aux.c
export_yuv4mpeg_la_LDFLAGS =  -module -avoid-version
export_yuv4mpeg_la_LIBADD = @LAME_LIBS@ @MJPEG_LIBS@ -L../libvo -lvout @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -lm

export_mpeg_la_SOURCES = export_mpeg.c vid_aux.c
export_mpeg_la_LDFLAGS =  -module -avoid-version
export_mpeg_la_LIBADD = -L../bbmpeg -lbbmpeg -L../libvo -lvout -lm @FFMPEG_LIBS_LIBS@

export_pvm_la_SOURCES = export_pvm.c ../pvm3/pvm_interface.c ../pvm3/pvm_parser.c ../pvm3/external_codec.c ../pvm3/vob_pack_unpack.c
export_pvm_la_LDFLAGS =  -module -avoid-version

export_mjpeg_la_SOURCES = export_mjpeg.c vid_aux.c aud_aux.c
export_mjpeg_la_LDFLAGS =  -module -avoid-version
export_mjpeg_la_LIBADD = @LIBJPEG_LIBS@ @LAME_LIBS@ -L../libvo -lvout @AC3_LIBS@ @FFMPEG_LIBS_LIBS@ -lm

export_mpeg2enc_la_SOURCES = export_mpeg2enc.c vid_aux.c
export_mpeg2enc_la_LDFLAGS =  -module -avoid-version
export_mpeg2enc_la_LIBADD = @MJPEG_LIBS@ -L../libvo -lvout -lm

export_im_la_SOURCES = export_im.c vid_aux.c
export_im_la_LDFLAGS =  -module -avoid-version
export_im_la_LIBADD = @MAGICK_LIBS@ -L../libvo -lvout -lm

export_pvn_la_SOURCES = export_pvn.c aud_aux.c
export_pvn_la_LDFLAGS =  -module -avoid-version
export_pvn_la_LIBADD = @LAME_LIBS@ @AC3_LIBS@ -L../libvo -lvout @FFMPEG_LIBS_LIBS@ -lm

pkg_DATA = export_af6.conf xvid2.cfg xvid3.cfg xvid4.cfg

EXTRA_DIST = \
  af6_aux.h \
  aud_aux.h \
  divx4_encore2.h \
  divx4_encparam.h \
  divx5_encore2.h \
  export_af6.conf \
  export_def.h \
  ffmpeg_cfg.h \
  tc_lzo.h \
  vbr.h \
  vid_aux.h \
  xvid2.h \
  xvid3.h \
  xvid4.h \
  xvid2.cfg \
  xvid3.cfg \
  xvid4.cfg \
  xvid_vbr.h


install-exec-hook:
	@rm -f $(DESTDIR)$(MOD_PATH)/export_xvid.so
	@$(LN_S) export_$(XVID_DEFAULT).so $(DESTDIR)$(MOD_PATH)/export_xvid.so
