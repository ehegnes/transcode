# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libtc \
	-I$(top_srcdir)/import \
	$(AC3_CFLAGS) \
	$(AVILIB_CFLAGS) \
	$(WAVLIB_CFLAGS) \
	$(ACLIB_CFLAGS) \
	$(LIBDV_CFLAGS) \
	$(LIBAVCODEC_CFLAGS) \
	$(TCIOAUX_CFLAGS) \
	$(XIO_CFLAGS) \
	$(LAME_CFLAGS) \
	$(LIBQUICKTIME_CFLAGS) \
	$(MJPEGTOOLS_CFLAGS) \
	$(IMAGEMAGICK_CFLAGS) \
	$(LIBJPEG_CFLAGS) \
	$(LZO_CFLAGS) \
	$(PVM3_CFLAGS)

pkgdir = $(MOD_PATH)

if HAVE_IMAGEMAGICK
MODS_IMAGEMAGICK = export_im.la
endif

if HAVE_LIBDV
MODS_LIBDV = export_dvraw.la export_dv.la
endif

if HAVE_LIBJPEG
MODS_LIBJPEG = export_jpg.la
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

if HAVE_PVM3
MODS_PVM3 = export_pvm.la
endif

pkg_LTLIBRARIES = \
	export_ac3.la \
	export_divx5.la \
	$(MODS_LIBDV) \
	export_ffmpeg.la \
	$(MODS_IMAGEMAGICK) \
	$(MODS_LIBJPEG) \
	export_lame.la \
	$(MODS_LZO) \
	$(MODS_LIBQUICKTIME) \
	export_mp2.la \
	export_null.la \
	export_ogg.la \
	export_ppm.la \
	$(MODS_PVM3) \
	export_raw.la \
	export_toolame.la \
	export_wav.la \
	export_xvid4.la \
	$(MODS_MJPEGTOOLS)

export_ac3_la_SOURCES = export_ac3.c
export_ac3_la_LDFLAGS = -module -avoid-version

export_divx5_la_SOURCES = export_divx5.c aud_aux.c divx4_vbr.c
export_divx5_la_LDFLAGS = -module -avoid-version
export_divx5_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(LIBAVCODEC_LIBS)

export_ffmpeg_la_SOURCES = export_ffmpeg.c aud_aux.c ffmpeg_cfg.c
export_ffmpeg_la_LDFLAGS = -module -avoid-version
export_ffmpeg_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(LIBAVCODEC_LIBS)

export_lame_la_SOURCES = export_lame.c
export_lame_la_LDFLAGS = -module -avoid-version

export_mp2_la_SOURCES = export_mp2.c
export_mp2_la_LDFLAGS = -module -avoid-version

export_null_la_SOURCES = export_null.c
export_null_la_LDFLAGS = -module -avoid-version

export_ogg_la_SOURCES = export_ogg.c
export_ogg_la_LDFLAGS = -module -avoid-version

export_ppm_la_SOURCES = export_ppm.c aud_aux.c
export_ppm_la_LDFLAGS = -module -avoid-version
export_ppm_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(LIBAVCODEC_LIBS)

export_raw_la_SOURCES = export_raw.c aud_aux.c
export_raw_la_LDFLAGS = -module -avoid-version
export_raw_la_LIBADD = $(LAME_LIBS) $(AC3_LIBS) $(LIBAVCODEC_LIBS)

export_toolame_la_SOURCES = export_toolame.c
export_toolame_la_LDFLAGS = -module -avoid-version

export_wav_la_SOURCES = export_wav.c
export_wav_la_LDFLAGS = -module -avoid-version
export_wav_la_LIBADD = -lm

export_xvid4_la_SOURCES = export_xvid4.c aud_aux.c
export_xvid4_la_LDFLAGS = -module -avoid-version
export_xvid4_la_LIBADD = $(LIBAVCODEC_LIBS) $(LAME_LIBS) $(AC3_LIBS)

# MODS_IMAGEMAGICK
export_im_la_SOURCES = export_im.c
export_im_la_LDFLAGS = -module -avoid-version
export_im_la_LIBADD = $(IMAGEMAGICK_LIBS) -lm

# MODS_LIBDV
export_dv_la_SOURCES = export_dv.c aud_aux.c
export_dv_la_LDFLAGS = -module -avoid-version
export_dv_la_LIBADD = $(LAME_LIBS) $(LIBDV_LIBS) $(AC3_LIBS) $(LIBAVCODEC_LIBS)

export_dvraw_la_SOURCES = export_dvraw.c
export_dvraw_la_LDFLAGS = -module -avoid-version
export_dvraw_la_LIBADD = $(LIBDV_LIBS) -lm

# MODS_LIBJPEG
export_jpg_la_SOURCES = export_jpg.c
export_jpg_la_LDFLAGS = -module -avoid-version
export_jpg_la_LIBADD = $(LIBJPEG_LIBS) -lm

# MODS_LIBQUICKTIME
export_mov_la_SOURCES = export_mov.c
export_mov_la_LDFLAGS = -module -avoid-version
export_mov_la_LIBADD = $(LIBQUICKTIME_LIBS) -lm

# MODS_LZO
export_lzo_la_SOURCES = export_lzo.c aud_aux.c
export_lzo_la_LDFLAGS = -module -avoid-version
export_lzo_la_LIBADD = $(LAME_LIBS) $(LZO_LIBS) $(AC3_LIBS) $(LIBAVCODEC_LIBS)

# MODS_MJPEGTOOLS
export_mp2enc_la_SOURCES = export_mp2enc.c
export_mp2enc_la_LDFLAGS = -module -avoid-version
export_mp2enc_la_LIBADD = -lm

export_mpeg2enc_la_SOURCES = export_mpeg2enc.c
export_mpeg2enc_la_LDFLAGS = -module -avoid-version
export_mpeg2enc_la_LIBADD = $(MJPEGTOOLS_LIBS) -lm

export_yuv4mpeg_la_SOURCES = export_yuv4mpeg.c aud_aux.c
export_yuv4mpeg_la_LDFLAGS = -module -avoid-version
export_yuv4mpeg_la_LIBADD = $(LAME_LIBS) $(MJPEGTOOLS_LIBS) $(AC3_LIBS) $(LIBAVCODEC_LIBS)

# MODS_PVM3
export_pvm_la_SOURCES = export_pvm.c ../pvm3/pvm_interface.c ../pvm3/pvm_parser.c ../pvm3/external_codec.c ../pvm3/vob_pack_unpack.c
export_pvm_la_LDFLAGS = -module -avoid-version

pkg_DATA = xvid4.cfg

EXTRA_DIST = \
	aud_aux.h \
	divx5_encore2.h \
	export_def.h \
	ffmpeg_cfg.h \
	tc_lzo.h \
	vbr.h \
	xvid4.h \
	xvid4.cfg

install-exec-hook:
	@mkdir -p $(DESTDIR)$(MOD_PATH)
	@rm -f $(DESTDIR)$(MOD_PATH)/export_xvid.so
	@$(LN_S) export_xvid4.so $(DESTDIR)$(MOD_PATH)/export_xvid.so
