# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

AM_CPPFLAGS = \
	-D_REENTRANT \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(AC3_CFLAGS) \
	$(AVILIB_CFLAGS) \
	$(LIBDV_CFLAGS) \
	$(FFMPEG_LIBS_CFLAGS) \
	$(LIBVO_CFLAGS) \
	$(TCIOAUX_CFLAGS) \
	$(XIO_CFLAGS) \
	$(AVIFILE_CFLAGS) \
	$(LAME_CFLAGS) \
	$(LIBQUICKTIME_CFLAGS) \
	$(MJPEGTOOLS_CFLAGS) \
	$(LIBFAME_CFLAGS) \
	$(IMAGEMAGICK_CFLAGS) \
	$(LIBJPEG_CFLAGS) \
	$(LZO_CFLAGS) \
	$(PVM3_CFLAGS)

pkgdir = $(MOD_PATH)

if HAVE_ASM_NASM
MODS_ASM_NASM = export_mpeg.la 
endif

if HAVE_AVIFILE
MODS_AVIFILE = export_af6.la
endif 

if HAVE_IMAGEMAGICK
MODS_IMAGEMAGICK = export_im.la
endif

if HAVE_LIBDV
MODS_LIBDV = export_dvraw.la export_dv.la 
endif

if HAVE_LIBFAME
MODS_LIBFAME = export_fame.la 
endif

if HAVE_LIBJPEG
MODS_LIBJPEG = export_mjpeg.la export_jpg.la
endif

if HAVE_LIBQUICKTIME
MODS_LIBQUICKTIME = export_mov.la
endif

if HAVE_LZO
MODS_LZO = export_lzo.la
endif

if HAVE_MJPEGTOOLS
MODS_MJPEGTOOLS = \
	export_yuv4mpeg.la \
	export_mpeg2enc.la \
	export_mp2enc.la
endif

if NET_STREAM
MODS_NET_STREAM = export_net.la
endif

if HAVE_PVM3
MODS_PVM3 = export_pvm.la
endif

pkg_LTLIBRARIES = \
	export_ac3.la \
	export_debugppm.la \
	export_divx4.la \
	export_divx4raw.la \
	export_divx5.la \
	export_divx5raw.la \
	export_ffmpeg.la \
	export_lame.la \
	export_mp1e.la \
	export_mp2.la \
	export_null.la \
	export_ogg.la \
	export_pcm.la \
	export_ppm.la \
	export_pvn.la \
	export_raw.la \
	export_toolame.la \
	export_wav.la \
	export_xvid2.la \
	export_xvid3.la \
	export_xvid4.la \
	$(MODS_ASM_NASM) \
	$(MODS_AVIFILE) \
	$(MODS_LIBDV) \
	$(MODS_LIBFAME) \
	$(MODS_LIBJPEG) \
	$(MODS_LIBQUICKTIME) \
	$(MODS_MJPEGTOOLS) \
	$(MODS_NET_STREAM) \
	$(MODS_IMAGEMAGICK) \
	$(MODS_LZO) \
	$(MODS_PVM3)

export_ac3_la_SOURCES = export_ac3.c
export_ac3_la_LDFLAGS = -module -avoid-version

export_debugppm_la_SOURCES = export_debugppm.c aud_aux.c
export_debugppm_la_LDFLAGS = -module -avoid-version
export_debugppm_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS) $(LIBVO_LIBS)

export_divx4_la_SOURCES = export_divx4.c aud_aux.c divx4_vbr.c
export_divx4_la_LDFLAGS = -module -avoid-version
export_divx4_la_LIBADD = $(LAME_LIBS) $(FFMPEG_LIBS_LIBS) $(AC3_LIBS)

export_divx4raw_la_SOURCES = export_divx4raw.c aud_aux.c divx4_vbr.c
export_divx4raw_la_LDFLAGS = -module -avoid-version
export_divx4raw_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS)

export_divx5_la_SOURCES = export_divx5.c aud_aux.c divx4_vbr.c
export_divx5_la_LDFLAGS = -module -avoid-version
export_divx5_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS)

export_divx5raw_la_SOURCES = export_divx5raw.c aud_aux.c divx4_vbr.c
export_divx5raw_la_LDFLAGS = -module -avoid-version
export_divx5raw_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS)

export_ffmpeg_la_SOURCES = export_ffmpeg.c aud_aux.c ffmpeg_cfg.c vid_aux.c
export_ffmpeg_la_LDFLAGS = -module -avoid-version
export_ffmpeg_la_LIBADD = $(LAME_LIBS) $(LIBVO_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS)

export_lame_la_SOURCES = export_lame.c
export_lame_la_LDFLAGS = -module -avoid-version

export_mp1e_la_SOURCES = export_mp1e.c vid_aux.c
export_mp1e_la_LDFLAGS = -module -avoid-version
export_mp1e_la_LIBADD  = $(LIBVO_LIBS) -lm

export_mp2_la_SOURCES = export_mp2.c
export_mp2_la_LDFLAGS = -module -avoid-version

export_null_la_SOURCES = export_null.c
export_null_la_LDFLAGS = -module -avoid-version

export_ogg_la_SOURCES = export_ogg.c
export_ogg_la_LDFLAGS = -module -avoid-version

export_pcm_la_SOURCES = export_pcm.c
export_pcm_la_LDFLAGS = -module -avoid-version
export_pcm_la_LIBADD = -lm

export_ppm_la_SOURCES = export_ppm.c aud_aux.c
export_ppm_la_LDFLAGS = -module -avoid-version
export_ppm_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS) $(LIBVO_LIBS)

export_pvn_la_SOURCES = export_pvn.c aud_aux.c
export_pvn_la_LDFLAGS = -module -avoid-version
export_pvn_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(LIBVO_LIBS) $(FFMPEG_LIBS_LIBS)

export_raw_la_SOURCES = export_raw.c aud_aux.c
export_raw_la_LDFLAGS = -module -avoid-version
export_raw_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS)

export_toolame_la_SOURCES = export_toolame.c
export_toolame_la_LDFLAGS = -module -avoid-version

export_wav_la_SOURCES = export_wav.c
export_wav_la_LDFLAGS = -module -avoid-version
export_wav_la_LIBADD = -lm

export_xvid2_la_SOURCES = export_xvid2.c aud_aux.c xvid_vbr.c
export_xvid2_la_LDFLAGS = -module -avoid-version
export_xvid2_la_LIBADD = $(FFMPEG_LIBS_LIBS) $(LAME_LIBS) $(AC3_LIBS)

export_xvid3_la_SOURCES = export_xvid3.c aud_aux.c xvid_vbr.c
export_xvid3_la_LDFLAGS = -module -avoid-version
export_xvid3_la_LIBADD = $(FFMPEG_LIBS_LIBS) $(LAME_LIBS) $(AC3_LIBS)

export_xvid4_la_SOURCES = export_xvid4.c aud_aux.c
export_xvid4_la_LDFLAGS = -module -avoid-version
export_xvid4_la_LIBADD = $(FFMPEG_LIBS_LIBS) $(LAME_LIBS) $(AC3_LIBS)

# MODS_ASM_NASM
export_mpeg_la_SOURCES = export_mpeg.c vid_aux.c
export_mpeg_la_LDFLAGS = -module -avoid-version
export_mpeg_la_LIBADD = $(BBMPEG_LIBS) $(LIBVO_LIBS) $(FFMPEG_LIBS_LIBS)

# MODS_AVIFILE
export_af6_la_SOURCES = export_af6.cpp af6_aux.cpp aud_aux.c
export_af6_la_LDFLAGS = -module -avoid-version
export_af6_la_LIBADD = $(LAME_LIBS) $(AVIFILE_LIBS) -lstdc++ $(AC3_LIBS) -lm

# MODS_IMAGEMAGICK
export_im_la_SOURCES = export_im.c vid_aux.c
export_im_la_LDFLAGS = -module -avoid-version
export_im_la_LIBADD = $(IMAGEMAGICK_LIBS) $(LIBVO_LIBS) -lm

# MODS_LIBDV
export_dv_la_SOURCES = export_dv.c aud_aux.c vid_aux.c
export_dv_la_LDFLAGS = -module -avoid-version 
export_dv_la_LIBADD = $(LAME_LIBS) $(LIBDVENC_LIBS) $(LIBDV_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS) $(LIBVO_LIBS)

export_dvraw_la_SOURCES = export_dvraw.c vid_aux.c
export_dvraw_la_LDFLAGS = -module -avoid-version
export_dvraw_la_LIBADD = $(LIBDVENC_LIBS) $(LIBDV_LIBS) $(LIBVO_LIBS) -lm

# MODS_LIBFAME
export_fame_la_SOURCES = export_fame.c aud_aux.c
export_fame_la_LDFLAGS = -module -avoid-version
export_fame_la_LIBADD = $(LAME_LIBS) $(LIBFAME_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS)

# MODS_LIBJPEG
export_jpg_la_SOURCES = export_jpg.c
export_jpg_la_LDFLAGS = -module -avoid-version
export_jpg_la_LIBADD = $(LIBJPEG_LIBS) -lm

export_mjpeg_la_SOURCES = export_mjpeg.c vid_aux.c aud_aux.c
export_mjpeg_la_LDFLAGS = -module -avoid-version
export_mjpeg_la_LIBADD = $(LIBJPEG_LIBS) $(LAME_LIBS) $(LIBVO_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS)

# MODS_LIBQUICKTIME
export_mov_la_SOURCES = export_mov.c
export_mov_la_LDFLAGS = -module -avoid-version
export_mov_la_LIBADD = $(LIBQUICKTIME_LIBS) -lm

# MODS_LZO
export_lzo_la_SOURCES = export_lzo.c aud_aux.c
export_lzo_la_LDFLAGS = -module -avoid-version
export_lzo_la_LIBADD = $(LAME_LIBS) $(LZO_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS)

# MODS_MJPEGTOOLS
export_mp2enc_la_SOURCES = export_mp2enc.c
export_mp2enc_la_LDFLAGS = -module -avoid-version
export_mp2enc_la_LIBADD = -lm

export_mpeg2enc_la_SOURCES = export_mpeg2enc.c vid_aux.c
export_mpeg2enc_la_LDFLAGS = -module -avoid-version
export_mpeg2enc_la_LIBADD = $(MJPEGTOOLS_LIBS) $(LIBVO_LIBS) -lm

export_yuv4mpeg_la_SOURCES = export_yuv4mpeg.c aud_aux.c vid_aux.c
export_yuv4mpeg_la_LDFLAGS = -module -avoid-version
export_yuv4mpeg_la_LIBADD = $(LAME_LIBS) $(MJPEGTOOLS_LIBS) $(LIBVO_LIBS) $(AC3_LIBS) $(FFMPEG_LIBS_LIBS)

# MODS_NET_STREAM
export_net_la_SOURCES = export_net.c
export_net_la_LDFLAGS = -module -avoid-version

# MODS_PVM3
export_pvm_la_SOURCES = export_pvm.c ../pvm3/pvm_interface.c ../pvm3/pvm_parser.c ../pvm3/external_codec.c ../pvm3/vob_pack_unpack.c
export_pvm_la_LDFLAGS = -module -avoid-version

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
