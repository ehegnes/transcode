# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign dist-bzip2

if HAVE_PVM3
PVM3=pvm3
endif

SUBDIRS = . libxio autotools docs contrib libdldarwin libsupport libioaux\
	aclib avilib movlib libac3 libvo libmpeg2\
	libmp3lame libdvenc bbmpeg ffmpeg\
	import export filter src tools $(PVM3)

EXTRA_DIST=transcode.spec libtool


docsdir = $(datadir)/doc/transcode
docs_DATA = ChangeLog
