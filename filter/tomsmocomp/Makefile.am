# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libtc \
	$(ACLIB_CFLAGS)

AM_CFLAGS = -fno-omit-frame-pointer

pkgdir = $(MOD_PATH)

pkg_LTLIBRARIES = filter_tomsmocomp.la

filter_tomsmocomp_la_SOURCES = \
	filter_tomsmocomp.c \
	tomsmocompfilter_mmx.c \
	tomsmocompfilter_3dnow.c \
	tomsmocompfilter_sse.c

filter_tomsmocomp_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = \
	Readme_TomsMoComp.txt \
	SearchLoop0A.inc \
	SearchLoopBottom.inc \
	SearchLoopEdgeA.inc \
	SearchLoopEdgeA8.inc \
	SearchLoopOddA.inc \
	SearchLoopOddA2.inc \
	SearchLoopOddA6.inc \
	SearchLoopOddAH.inc \
	SearchLoopOddAH2.inc \
	SearchLoopTop.inc \
	SearchLoopVA.inc \
	SearchLoopVAH.inc \
	StrangeBob.inc \
	TomsMoCompAll.inc \
	TomsMoCompAll2.inc \
	WierdBob.inc \
	dscaler_interface.h \
	filter_tomsmocomp.h \
	tomsmocompfilter.h \
	tomsmocompmacros.h
