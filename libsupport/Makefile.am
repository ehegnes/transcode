## Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES                	=  -I$(top_builddir) -I$(top_srcdir)

noinst_LIBRARIES        	= libsupport.a

libsupport_a_SOURCES		=
EXTRA_libsupport_a_SOURCES	=  getopt.c getopt1.c

libsupport_a_DEPENDENCIES	= @SUPPORTOBJECTS@
libsupport_a_LIBADD		= @SUPPORTOBJECTS@

EXTRA_DIST 			= *.c *.h

