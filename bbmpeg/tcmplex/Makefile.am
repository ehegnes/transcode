AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = -I$(top_srcdir)/bbmpeg

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

tcmplex_LDADD = -L$(top_builddir)/bbmpeg -lbbmpeg -lm
