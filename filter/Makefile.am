# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(AVILIB_CFLAGS) \
	$(AVIFILE_CFLAGS) \
	$(LAME_CFLAGS) \
	$(AC3_CFLAGS) \
	$(IMAGEMAGICK_CFLAGS) \
	$(FT2_CFLAGS)

pkgdir = $(MOD_PATH)

if HAVE_MMX
F_MMX = filter_dilyuvmmx.la 
else
F_MMX=
endif

if HAVE_FREETYPE2
F_TEXT = filter_text.la
endif

if HAVE_IMAGEMAGICK
F_LOGO = filter_logo.la filter_logoaway.la filter_compare.la
endif

if CAN_COMPILE_C_ALTIVEC
if SYSTEM_DARWIN
ALTIVEC = -faltivec
else
ALTIVEC = -maltivec -mabi=altivec
endif
endif

if HAVE_GCC2
F_TOMSMOCOMP = 
else
if HAVE_MMX
if HAVE_SSE
if HAVE_3DNOW
F_TOMSMOCOMP = tomsmocomp
endif
endif
endif
endif

AM_CFLAGS = $(ALTIVEC)

pkg_LTLIBRARIES = \
	filter_29to23.la \
	filter_32detect.la \
	filter_32drop.la \
	filter_aclip.la \
	filter_ascii.la \
	filter_astat.la \
	filter_clone.la \
	filter_control.la \
	filter_cpaudio.la \
	filter_cshift.la \
	filter_cut.la \
	filter_denoise3d.la \
	filter_detectclipping.la \
	filter_detectsilence.la \
	filter_dnr.la \
	filter_doublefps.la \
	filter_facemask.la \
	filter_fieldanalysis.la\
	filter_fields.la \
	filter_fps.la \
	filter_hqdn3d.la \
	filter_invert.la \
	filter_levels.la \
	filter_lowpass.la \
	filter_mask.la \
	filter_modfps.la \
	filter_msharpen.la \
	filter_nored.la \
	filter_normalize.la \
	filter_null.la \
	filter_skip.la \
	filter_slowmo.la \
	filter_smartbob.la \
	filter_smartdeinter.la \
	filter_smartyuv.la \
	filter_smooth.la \
	filter_tc_audio.la \
	filter_tc_video.la \
	filter_testframe.la \
	filter_unsharp.la \
	filter_videocore.la \
	filter_whitebalance.la \
	filter_xsharpen.la \
	filter_yuy2toyv12.la \
	$(F_LOGO) \
	$(F_MMX) \
	$(F_TEXT)

filter_29to23_la_SOURCES = filter_29to23.c
filter_29to23_la_LDFLAGS = -module -avoid-version 

filter_32detect_la_SOURCES = filter_32detect.c
filter_32detect_la_LDFLAGS = -module -avoid-version 

filter_32drop_la_SOURCES = filter_32drop.c
filter_32drop_la_LDFLAGS = -module -avoid-version 

filter_aclip_la_SOURCES = filter_aclip.c
filter_aclip_la_LDFLAGS = -module -avoid-version 

filter_ascii_la_SOURCES = filter_ascii.c ../export/vid_aux.c
filter_ascii_la_LDFLAGS = -module -avoid-version 
filter_ascii_la_LIBADD = $(LIBVO_LIBS)

filter_astat_la_SOURCES = filter_astat.c
filter_astat_la_LDFLAGS = -module -avoid-version 

filter_clone_la_SOURCES = filter_clone.c
filter_clone_la_LDFLAGS = -module -avoid-version 

filter_control_la_SOURCES = filter_control.c
filter_control_la_LDFLAGS = -module -avoid-version

filter_cpaudio_la_SOURCES = filter_cpaudio.c
filter_cpaudio_la_LDFLAGS = -module -avoid-version 

filter_cshift_la_SOURCES = filter_cshift.c
filter_cshift_la_LDFLAGS = -module -avoid-version 

filter_cut_la_SOURCES = filter_cut.c
filter_cut_la_LDFLAGS = -module -avoid-version 

filter_denoise3d_la_SOURCES = filter_denoise3d.c
filter_denoise3d_la_LDFLAGS = -module -avoid-version 

filter_detectclipping_la_SOURCES = filter_detectclipping.c
filter_detectclipping_la_LDFLAGS = -module -avoid-version 

filter_detectsilence_la_SOURCES = filter_detectsilence.c
filter_detectsilence_la_LDFLAGS = -module -avoid-version 

filter_dnr_la_SOURCES = filter_dnr.c
filter_dnr_la_LDFLAGS = -module -avoid-version 
filter_dnr_la_LIBADD = -lm

filter_doublefps_la_SOURCES = filter_doublefps.c
filter_doublefps_la_LDFLAGS = -module -avoid-version 

filter_facemask_la_SOURCES = filter_facemask.c ../export/vid_aux.c
filter_facemask_la_LDFLAGS = -module -avoid-version 
filter_facemask_la_LIBADD = $(LIBVO_LIBS)

filter_fieldanalysis_la_SOURCES = filter_fieldanalysis.c
filter_fieldanalysis_la_LDFLAGS = -module -avoid-version 

filter_fields_la_SOURCES = filter_fields.c
filter_fields_la_LDFLAGS = -module -avoid-version 

filter_fps_la_SOURCES = filter_fps.c
filter_fps_la_LDFLAGS = -module -avoid-version

filter_hqdn3d_la_SOURCES = filter_hqdn3d.c
filter_hqdn3d_la_LDFLAGS = -module -avoid-version 

filter_invert_la_SOURCES = filter_invert.c
filter_invert_la_LDFLAGS = -module -avoid-version 

filter_levels_la_SOURCES = filter_levels.c
filter_levels_la_LDFLAGS = -module -avoid-version

filter_lowpass_la_SOURCES = filter_lowpass.c
filter_lowpass_la_LDFLAGS = -module -avoid-version 

filter_mask_la_SOURCES = filter_mask.c
filter_mask_la_LDFLAGS = -module -avoid-version 

filter_modfps_la_SOURCES = filter_modfps.c
filter_modfps_la_LDFLAGS = -module -avoid-version 
filter_modfps_la_LIBADD = -lm

filter_msharpen_la_SOURCES = filter_msharpen.c ../export/vid_aux.c
filter_msharpen_la_LDFLAGS = -module -avoid-version
filter_msharpen_la_LIBADD = $(LIBVO_LIBS)

filter_nored_la_SOURCES = filter_nored.c
filter_nored_la_LDFLAGS = -module -avoid-version 

filter_normalize_la_SOURCES = filter_normalize.c
filter_normalize_la_LDFLAGS = -module -avoid-version 
filter_normalize_la_LIBADD = -lm

filter_null_la_SOURCES = filter_null.c
filter_null_la_LDFLAGS = -module -avoid-version 

filter_skip_la_SOURCES = filter_skip.c
filter_skip_la_LDFLAGS = -module -avoid-version 

filter_slowmo_la_SOURCES = filter_slowmo.c
filter_slowmo_la_LDFLAGS = -module -avoid-version 

filter_smartbob_la_SOURCES = filter_smartbob.c ../export/vid_aux.c
filter_smartbob_la_LDFLAGS = -module -avoid-version 
filter_smartbob_la_LIBADD = $(LIBVO_LIBS)

filter_smartdeinter_la_SOURCES = filter_smartdeinter.c ../export/vid_aux.c
filter_smartdeinter_la_LDFLAGS = -module -avoid-version 
filter_smartdeinter_la_LIBADD = $(LIBVO_LIBS)

filter_smartyuv_la_SOURCES = filter_smartyuv.c
filter_smartyuv_la_LDFLAGS = -module -avoid-version 

filter_smooth_la_SOURCES = filter_smooth.c
filter_smooth_la_LDFLAGS = -module -avoid-version 

filter_tc_audio_la_SOURCES = filter_tc_audio.c
filter_tc_audio_la_LDFLAGS = -module -avoid-version 

filter_tc_video_la_SOURCES = filter_tc_video.c
filter_tc_video_la_LDFLAGS = -module -avoid-version 

filter_testframe_la_SOURCES = filter_testframe.c
filter_testframe_la_LDFLAGS = -module -avoid-version 

filter_unsharp_la_SOURCES = filter_unsharp.c
filter_unsharp_la_LDFLAGS = -module -avoid-version 

filter_videocore_la_SOURCES = filter_videocore.c
filter_videocore_la_LDFLAGS = -module -avoid-version 

filter_whitebalance_la_SOURCES = filter_whitebalance.c ../export/vid_aux.c
filter_whitebalance_la_LDFLAGS = -module -avoid-version
filter_whitebalance_la_LIBADD = $(LIBVO_LIBS) -lm

filter_xsharpen_la_SOURCES = filter_xsharpen.c
filter_xsharpen_la_LDFLAGS = -module -avoid-version

filter_yuy2toyv12_la_SOURCES = filter_yuy2toyv12.c
filter_yuy2toyv12_la_LDFLAGS = -module -avoid-version 


# F_LOGO = filter_logo.la filter_logoaway.la filter_compare.la

filter_logo_la_SOURCES = filter_logo.c ../export/vid_aux.c
filter_logo_la_LDFLAGS = -module -avoid-version
filter_logo_la_LIBADD = $(IMAGEMAGICK_LIBS) $(LIBVO_LIBS)

filter_logoaway_la_SOURCES = filter_logoaway.c
filter_logoaway_la_LDFLAGS = -module -avoid-version 
filter_logoaway_la_LIBADD = $(IMAGEMAGICK_LIBS)

filter_compare_la_SOURCES = filter_compare.c ../export/vid_aux.c
filter_compare_la_LDFLAGS = -module -avoid-version
filter_compare_la_LIBADD = $(IMAGEMAGICK_LIBS) $(LIBVO_LIBS)


# F_MMX = filter_dilyuvmmx.la 

filter_dilyuvmmx_la_SOURCES = filter_dilyuvmmx.c deinterlace.c
filter_dilyuvmmx_la_LDFLAGS = -module -avoid-version 


# F_TEXT = filter_text.la

filter_text_la_SOURCES = filter_text.c
filter_text_la_LDFLAGS = -module -avoid-version 
filter_text_la_LIBADD  = $(FT2_LIBS)


EXTRA_DIST =  \
	parse_csv.awk.in \
	filter_list.awk.in \
	mmx.h

if HAVE_FREETYPE2
if HAVE_XAW_SIMPLE
if HAVE_LIBXPM
if HAVE_LIBICONV
ADD2 = subtitler
endif
endif
endif
endif

if HAVE_X11
SUBDIRS = \
	pp \
	divxkey \
	extsub \
	resample \
	test \
	yuvdenoise \
	ivtc \
	$(F_TOMSMOCOMP) \
	preview \
	$(ADD2)
else
SUBDIRS = \
	pp \
	divxkey \
	extsub \
	resample \
	test \
	yuvdenoise \
	ivtc \
	$(F_TOMSMOCOMP)
endif

pkg_SCRIPTS = \
	filter_list.awk \
	parse_csv.awk
