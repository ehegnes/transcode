# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = $(BBMPEG_CFLAGS)

bin_PROGRAMS = tcmplex

tcmplex_SOURCES = \
	buffer.c \
	domplex.c \
	inits.c \
	inptstrm.c \
	interact.c \
	multplex.c \
	systems.c \
	tcmplex.c \
	timecode.c

tcmplex_LDADD = \
	$(BBMPEG_LIBS) \
	-lm
