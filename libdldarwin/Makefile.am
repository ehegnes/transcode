## Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES                	=  -I$(top_builddir) -I$(top_srcdir)

noinst_LIBRARIES        	= libdldarwin.a

libdldarwin_a_SOURCES		=
EXTRA_libdldarwin_a_SOURCES	=  dlfcn_simple.c

libdldarwin_a_DEPENDENCIES	= @DLDARWINOBJECTS@
libdldarwin_a_LIBADD		= @DLDARWINOBJECTS@

EXTRA_DIST 			= *.c *.h

