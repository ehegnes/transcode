# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -I./ -I../../src -I../ -I../../ -I$(srcdir)/../../src -I$(srcdir)/../.. @DV_CFLAGS@ -Wall -I$(x_includes) $(GTK_CFLAGS) $(SDL_CFLAGS)

if HAVE_LIBXV
LIBS_XV=-lXv
endif

if HAVE_GTK
LIBS_GTK = $(GTK_LIBS)
endif

if HAVE_SDL
LIBS_SDL = $(SDL_LIBS)
endif

LIBS = $(LIBS_XV) $(LIBS_GTK) $(LIBS_SDL) 

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = filter_preview.la filter_pv.la 

filter_preview_la_SOURCES = filter_preview.c display.c 
filter_preview_la_LDFLAGS = -no-undefined -module -avoid-version 
filter_preview_la_LIBADD = -L$(x_libraries) $(X_LIBS) $(X_PRE_LIBS) -lXext -lX11 $(X_EXTRA_LIBS) @DV_LIBS@ 

filter_pv_la_SOURCES = filter_pv.c pv.c 
filter_pv_la_LDFLAGS = -no-undefined -module -avoid-version 
filter_pv_la_LIBADD = -L$(x_libraries) $(X_LIBS) $(X_PRE_LIBS) -lXext -lX11 $(LIBS_XV)


EXTRA_DIST = *.h 


