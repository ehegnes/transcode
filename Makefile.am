# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = foreign

if HAVE_PVM3
PVM3 = pvm3
endif

SUBDIRS = \
	aclib \
	avilib \
	libac3 \
	libdldarwin \
	libtc \
	libtcaudio \
	libtcvideo \
	encode \
	export \
	filter \
	import \
	multiplex \
	$(PVM3) \
	src \
	testsuite \
	tools \
	contrib \
	docs

EXTRA_DIST = STYLE transcode.spec

docsdir = $(datadir)/doc/transcode
docs_DATA = ChangeLog
