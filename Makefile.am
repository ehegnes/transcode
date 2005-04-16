# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = foreign

if HAVE_PVM3
PVM3 = pvm3
endif

SUBDIRS = \
	libtc \
	libxio \
	docs \
	contrib \
	libdldarwin \
	libsupport \
	libioaux \
	aclib \
	avilib \
	libac3 \
	libvo \
	import \
	export \
	filter \
	src \
	tools \
	$(PVM3) \
	testsuite

EXTRA_DIST = transcode.spec README-0.6.14

docsdir = $(datadir)/doc/transcode
docs_DATA = ChangeLog
