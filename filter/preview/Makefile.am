# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/filter \
	-I$(top_srcdir)/src \
	$(DLDARWININCL)

AM_CPPFLAGS = \
	-D_REENTRANT \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	$(LIBDV_CFLAGS) \
	$(X_CFLAGS) \
	$(GTK_CFLAGS) \
	$(SDL_CFLAGS)

if HAVE_LIBXV
LIBS_XV = -lXv
endif

if HAVE_GTK
LIBS_GTK = $(GTK_LIBS)
endif

if HAVE_SDL
LIBS_SDL = $(SDL_LIBS)
endif

LIBS = \
	$(LIBS_XV) \
	$(LIBS_GTK) \
	$(LIBS_SDL)

pkgdir = $(MOD_PATH)

if HAVE_LIBDV
MOD1 = filter_preview.la
endif

pkg_LTLIBRARIES = $(MOD1) filter_pv.la

filter_preview_la_SOURCES = filter_preview.c display.c
filter_preview_la_LDFLAGS = -module -avoid-version
filter_preview_la_LIBADD = \
	$(X_LIBS) \
	$(X_PRE_LIBS) \
	-lXext \
	-lX11 \
	$(X_EXTRA_LIBS) \
	$(LIBDV_LIBS)

filter_pv_la_SOURCES = filter_pv.c pv.c
filter_pv_la_LDFLAGS = -module -avoid-version
filter_pv_la_LIBADD = \
	$(X_LIBS) \
	$(X_PRE_LIBS) \
	-lXext \
	-lX11 \
	$(LIBS_XV) \
	$(DLDARWINLIB)

EXTRA_DIST = \
	display.h \
	dv_types.h \
	filter_preview.h \
	font_xpm.h \
	pv.h
