# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -I./ -I../src -I$(srcdir)/../src -I../avilib -I$(srcdir)/../avilib @AVIFILE_CFLAGS@ -I../encore2 -I$(srcdir)/../encore2 @LAME_CFLAGS@ -I../libac3 -I$(srcdir)/../libac3 @LIBPOSTPROC_CFLAGS@ @MAGICK_CFLAGS@

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

if HAVE_LIBPOSTPROC
F_PP=filter_pp.la filter_ppcvs.la
else
F_PP=
endif

if HAVE_MMX
F_MMX=filter_dilyuvmmx.la 
else
F_MMX=
endif

if HAVE_MAGICK
F_LOGO=filter_logo.la
endif

pkg_LTLIBRARIES = 	filter_null.la\
			filter_invert.la\
			filter_32detect.la\
			filter_32drop.la\
			filter_yuy2toyv12.la\
			filter_skip.la\
			filter_astat.la\
			filter_aclip.la\
			filter_cut.la\
			filter_cshift.la\
			filter_mask.la\
			filter_dnr.la\
			filter_xsharpen.la\
			filter_smartdeinter.la\
			filter_normalize.la\
			filter_testframe.la $(F_PP) $(F_MMX) $(F_LOGO)

filter_null_la_SOURCES = filter_null.c
filter_null_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_skip_la_SOURCES = filter_skip.c
filter_skip_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_cut_la_SOURCES = filter_cut.c
filter_cut_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_mask_la_SOURCES = filter_mask.c
filter_mask_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_dnr_la_SOURCES = filter_dnr.c
filter_dnr_la_LDFLAGS = -no-undefined -module -avoid-version 
filter_dnr_la_LIBADD = -lm

filter_testframe_la_SOURCES = filter_testframe.c
filter_testframe_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_astat_la_SOURCES = filter_astat.c
filter_astat_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_aclip_la_SOURCES = filter_aclip.c
filter_aclip_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_32detect_la_SOURCES = filter_32detect.c
filter_32detect_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_32drop_la_SOURCES = filter_32drop.c
filter_32drop_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_cshift_la_SOURCES = filter_cshift.c
filter_cshift_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_yuy2toyv12_la_SOURCES = filter_yuy2toyv12.c
filter_yuy2toyv12_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_smartdeinter_la_SOURCES = filter_smartdeinter.c
filter_smartdeinter_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_normalize_la_SOURCES = filter_normalize.c
filter_normalize_la_LDFLAGS = -no-undefined -module -avoid-version 
filter_normalize_la_LIBADD = -lm

filter_logo_la_SOURCES = filter_logo.c
filter_logo_la_LDFLAGS = -no-undefined -module -avoid-version 
filter_logo_la_LIBADD = @MAGICK_LIBS@ -L../libvo -lvout ../export/vid_aux.lo

filter_xsharpen_la_SOURCES = filter_xsharpen.c
filter_xsharpen_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_invert_la_SOURCES = filter_invert.c
filter_invert_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_pp_la_SOURCES = filter_pp.c
filter_pp_la_LDFLAGS = -no-undefined -module -avoid-version 
filter_pp_la_LIBADD = @LIBPOSTPROC_LIBS@

filter_ppcvs_la_SOURCES = filter_ppcvs.c
filter_ppcvs_la_LDFLAGS = -no-undefined -module -avoid-version 
filter_ppcvs_la_LIBADD = @LIBPOSTPROC_LIBS@

filter_dilyuvmmx_la_SOURCES = filter_dilyuvmmx.c deinterlace.c
filter_dilyuvmmx_la_LDFLAGS = -no-undefined -module -avoid-version 

#filter_dilb_la_SOURCES = filter_dilb.c 
#filter_dilb_la_LDFLAGS = -no-undefined -module -avoid-version 

EXTRA_DIST = *.h 

if HAVE_DV
ADD1=preview 
endif

ADD2=subtitler

if HAVE_X11
SUBDIRS = . divxkey extsub resample test yuvdenoise $(ADD1) $(ADD2)
else
SUBDIRS = . divxkey extsub resample test yuvdenoise
endif
