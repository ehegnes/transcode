# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -I./ -I../src -I$(srcdir)/../src -I../avilib -I$(srcdir)/../avilib @AVIFILE_CFLAGS@ @LAME_CFLAGS@ -I../libac3 -I$(srcdir)/../libac3 @LIBPOSTPROC_CFLAGS@ @MAGICK_CFLAGS@ @FT2_CFLAGS@

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

if HAVE_LIBPOSTPROC
F_PP=filter_pp.la
else
F_PP=
endif

if HAVE_MMX
F_MMX=filter_dilyuvmmx.la 
else
F_MMX=
endif

if HAVE_FREETYPE2
F_TEXT=filter_text.la
endif

if HAVE_MAGICK
F_LOGO=filter_logo.la filter_logoaway.la filter_compare.la
endif

pkg_LTLIBRARIES = 	filter_null.la\
			filter_invert.la\
			filter_nored.la\
			filter_32detect.la\
			filter_32drop.la\
			filter_yuy2toyv12.la\
			filter_skip.la\
			filter_astat.la\
			filter_detectsilence.la\
			filter_lowpass.la\
			filter_aclip.la\
			filter_cut.la\
			filter_cshift.la\
			filter_mask.la\
			filter_dnr.la\
			filter_xsharpen.la\
			filter_smartdeinter.la\
			filter_normalize.la\
			filter_testframe.la\
			filter_slowmo.la\
			filter_fields.la\
			filter_clone.la\
			filter_doublefps.la\
			filter_videocore.la\
			filter_tc_video.la\
			filter_tc_audio.la\
			filter_cpaudio.la\
			filter_hqdn3d.la\
			filter_modfps.la\
			filter_29to23.la\
			filter_smooth.la\
			filter_unsharp.la\
			filter_smartbob.la\
			filter_smartyuv.la\
			filter_msharpen.la\
			$(F_PP) $(F_MMX) $(F_LOGO) $(F_TEXT)

filter_null_la_SOURCES = filter_null.c
filter_null_la_LDFLAGS =  -module -avoid-version 

filter_doublefps_la_SOURCES = filter_doublefps.c
filter_doublefps_la_LDFLAGS =  -module -avoid-version 

filter_clone_la_SOURCES = filter_clone.c
filter_clone_la_LDFLAGS =  -module -avoid-version 

filter_cpaudio_la_SOURCES = filter_cpaudio.c
filter_cpaudio_la_LDFLAGS = -module -avoid-version 

filter_text_la_SOURCES = filter_text.c
filter_text_la_LDFLAGS =  -module -avoid-version 
filter_text_la_LIBADD  = @FT2_LIBS@

filter_smooth_la_SOURCES = filter_smooth.c
filter_smooth_la_LDFLAGS =  -module -avoid-version 

filter_smartyuv_la_SOURCES = filter_smartyuv.c
filter_smartyuv_la_LDFLAGS =  -module -avoid-version 

filter_unsharp_la_SOURCES = filter_unsharp.c
filter_unsharp_la_LDFLAGS =  -module -avoid-version 

filter_hqdn3d_la_SOURCES = filter_hqdn3d.c
filter_hqdn3d_la_LDFLAGS =  -module -avoid-version 

filter_modfps_la_SOURCES = filter_modfps.c
filter_modfps_la_LDFLAGS =  -module -avoid-version 

filter_29to23_la_SOURCES = filter_29to23.c
filter_29to23_la_LDFLAGS =  -module -avoid-version 

filter_tc_video_la_SOURCES = filter_tc_video.c
filter_tc_video_la_LDFLAGS =  -module -avoid-version 

filter_tc_audio_la_SOURCES = filter_tc_audio.c
filter_tc_audio_la_LDFLAGS =  -module -avoid-version 

filter_skip_la_SOURCES = filter_skip.c
filter_skip_la_LDFLAGS =  -module -avoid-version 

filter_cut_la_SOURCES = filter_cut.c
filter_cut_la_LDFLAGS =  -module -avoid-version 

filter_mask_la_SOURCES = filter_mask.c
filter_mask_la_LDFLAGS =  -module -avoid-version 

filter_dnr_la_SOURCES = filter_dnr.c
filter_dnr_la_LDFLAGS =  -module -avoid-version 
filter_dnr_la_LIBADD = -lm

filter_testframe_la_SOURCES = filter_testframe.c
filter_testframe_la_LDFLAGS =  -module -avoid-version 

filter_slowmo_la_SOURCES = filter_slowmo.c
filter_slowmo_la_LDFLAGS =  -module -avoid-version 

filter_astat_la_SOURCES = filter_astat.c
filter_astat_la_LDFLAGS =  -module -avoid-version 

filter_detectsilence_la_SOURCES = filter_detectsilence.c
filter_detectsilence_la_LDFLAGS =  -module -avoid-version 

filter_lowpass_la_SOURCES = filter_lowpass.c
filter_lowpass_la_LDFLAGS =  -module -avoid-version 

filter_aclip_la_SOURCES = filter_aclip.c
filter_aclip_la_LDFLAGS =  -module -avoid-version 

filter_videocore_la_SOURCES = filter_videocore.c
filter_videocore_la_LDFLAGS =  -module -avoid-version 

filter_32detect_la_SOURCES = filter_32detect.c
filter_32detect_la_LDFLAGS =  -module -avoid-version 

filter_32drop_la_SOURCES = filter_32drop.c
filter_32drop_la_LDFLAGS =  -module -avoid-version 

filter_cshift_la_SOURCES = filter_cshift.c
filter_cshift_la_LDFLAGS =  -module -avoid-version 

filter_yuy2toyv12_la_SOURCES = filter_yuy2toyv12.c
filter_yuy2toyv12_la_LDFLAGS =  -module -avoid-version 

filter_smartbob_la_SOURCES = filter_smartbob.c
filter_smartbob_la_LDFLAGS =  -module -avoid-version 
filter_smartbob_la_LIBADD = -L../libvo -lvout ../export/vid_aux.lo

filter_smartdeinter_la_SOURCES = filter_smartdeinter.c
filter_smartdeinter_la_LDFLAGS =  -module -avoid-version 
filter_smartdeinter_la_LIBADD = -L../libvo -lvout ../export/vid_aux.lo

filter_normalize_la_SOURCES = filter_normalize.c
filter_normalize_la_LDFLAGS =  -module -avoid-version 
filter_normalize_la_LIBADD = -lm

filter_compare_la_SOURCES = filter_compare.c
filter_compare_la_LDFLAGS =  -module -avoid-version
filter_compare_la_LIBADD = @MAGICK_LIBS@ -L../libvo -lvout ../export/vid_aux.lo

filter_logo_la_SOURCES = filter_logo.c
filter_logo_la_LDFLAGS =  -module -avoid-version
filter_logo_la_LIBADD = @MAGICK_LIBS@ -L../libvo -lvout ../export/vid_aux.lo

filter_xsharpen_la_SOURCES = filter_xsharpen.c
filter_xsharpen_la_LDFLAGS =  -module -avoid-version 

filter_msharpen_la_SOURCES = filter_msharpen.c
filter_msharpen_la_LDFLAGS = -module -avoid-version
filter_msharpen_la_LIBADD = -L../libvo -lvout ../export/vid_aux.lo

filter_logoaway_la_SOURCES = filter_logoaway.c
filter_logoaway_la_LDFLAGS =  -module -avoid-version 
filter_logoaway_la_LIBADD = @MAGICK_LIBS@

filter_invert_la_SOURCES = filter_invert.c
filter_invert_la_LDFLAGS =  -module -avoid-version 

filter_nored_la_SOURCES = filter_nored.c
filter_nored_la_LDFLAGS =  -module -avoid-version 

filter_pp_la_SOURCES = filter_pp.c
filter_pp_la_LDFLAGS =  -module -avoid-version 
filter_pp_la_LIBADD = @LIBPOSTPROC_LIBS@

filter_dilyuvmmx_la_SOURCES = filter_dilyuvmmx.c deinterlace.c
filter_dilyuvmmx_la_LDFLAGS =  -module -avoid-version 

filter_fields_la_SOURCES = filter_fields.c
filter_fields_la_LDFLAGS =  -module -avoid-version 


EXTRA_DIST =  parse_csv.awk filter_list.awk mmx.h

if HAVE_XAW_SIMPLE
if HAVE_LIBXPM
ADD2=subtitler
endif
endif

if HAVE_X11
SUBDIRS = . divxkey extsub resample test yuvdenoise ivtc preview $(ADD2)
else
SUBDIRS = . divxkey extsub resample test yuvdenoise ivtc
endif

pkg_SCRIPTS = filter_list.awk parse_csv.awk
