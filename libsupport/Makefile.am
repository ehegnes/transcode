# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = -I$(top_srcdir)

noinst_LIBRARIES = libsupport.a

libsupport_a_SOURCES =
EXTRA_libsupport_a_SOURCES = \
	getopt.c \
	getopt1.c

libsupport_a_DEPENDENCIES = $(SUPPORT_OBJECTS)
libsupport_a_LIBADD = $(SUPPORT_OBJECTS)

EXTRA_DIST = \
	getopt.c \
	getopt.h \
	getopt1.c

