/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "st.h"
#include <string.h>
#include <ctype.h>
#include <signal.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * util.c.
 * Incorporate Jimen Ching's fixes for real library operation: Aug 3, 1994.
 * Redo all work from scratch, unfortunately.
 * Separate out all common variables used by effects & handlers,
 * and utility routines for other main programs to use.
 */

/* export flags */
/* FIXME: To be moved inside of fileop structure per handler. */
int verbose = 0;	/* be noisy on stderr */

/* FIXME:  These functions are user level concepts.  Move them outside
 * the ST library. 
 */
char *myname = 0;

void
st_report(const char *fmt, ...) 
{
	va_list args;

	//	if (! verbose)
		return;

	fprintf(stderr, "%s: ", myname);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}


void
st_warn(const char *fmt, ...) 
{
	va_list args;

	fprintf(stderr, "%s: ", myname);
	va_start(args, fmt);

	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

void
st_fail(const char *fmt, ...) 
{
	va_list args;

	fprintf(stderr, "%s: ", myname);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(2);
}


/* Warning: no error checking is done with errstr.  Be sure not to
 * go over the array limit ourself!
 */
void
st_fail_errno(ft_t ft, int st_errno, const char *fmt, ...)
{
	va_list args;

	ft->st_errno = st_errno;

	va_start(args, fmt);
	vsprintf(ft->st_errstr, fmt, args);
	va_end(args);
}

int st_is_bigendian(void)
{
    int b;
    char *p;

    b = 1;
    p = (char *) &b;
    if (!*p)
	return 1;
    else
	return 0;
}

int st_is_littleendian(void)
{
    int b;
    char *p;

    b = 1;
    p = (char *) &b;
    if (*p)
	return 1;
    else
	return 0;
}

#if 0  /* unused --AC */
static int strcmpcase(const char *s1, const char *s2)
{
	while(*s1 && *s2 && (tolower(*s1) == tolower(*s2)))
		s1++, s2++;
	return *s1 - *s2;
}
#endif

/*
 * File format routines 
 */

void st_copyformat(ft, ft2)
ft_t ft, ft2;
{
	int noise = 0, i;
	double factor;

	if (ft2->info.rate == 0) {
		ft2->info.rate = ft->info.rate;
		noise = 1;
	}
	if (ft2->info.size == -1) {
		ft2->info.size = ft->info.size;
		noise = 1;
	}
	if (ft2->info.encoding == -1) {
		ft2->info.encoding = ft->info.encoding;
		noise = 1;
	}
	if (ft2->info.channels == -1) {
		ft2->info.channels = ft->info.channels;
		noise = 1;
	}
	if (ft2->comment == NULL) {
		ft2->comment = ft->comment;
		noise = 1;
	}
	/* 
	 * copy loop info, resizing appropriately 
	 * it's in samples, so # channels don't matter
	 */
	factor = (double) ft2->info.rate / (double) ft->info.rate;
	for(i = 0; i < ST_MAX_NLOOPS; i++) {
		ft2->loops[i].start = ft->loops[i].start * factor;
		ft2->loops[i].length = ft->loops[i].length * factor;
		ft2->loops[i].count = ft->loops[i].count;
		ft2->loops[i].type = ft->loops[i].type;
	}
	/* leave SMPTE # alone since it's absolute */
	ft2->instr = ft->instr;
}

void st_cmpformats(ft, ft2)
ft_t ft, ft2;
{
}

/* check that all settings have been given */
void st_checkformat(ft) 
ft_t ft;
{
	if (ft->info.rate == 0)
		st_fail("Sampling rate for %s file was not given\n", ft->filename);
	if ((ft->info.rate < 100) || (ft->info.rate > 999999L))
		st_fail("Sampling rate %lu for %s file is bogus\n", 
			ft->info.rate, ft->filename);
	if (ft->info.size == -1)
		st_fail("Data size was not given for %s file\nUse one of -b/-w/-l/-f/-d/-D", ft->filename);
	if (ft->info.encoding == -1 && ft->info.size != ST_SIZE_FLOAT)
		st_fail("Data encoding was not given for %s file\nUse one of -s/-u/-U/-A", ft->filename);
	/* it's so common, might as well default */
	if (ft->info.channels == -1)
		ft->info.channels = 1;
	/*	st_fail("Number of output channels was not given for %s file",
			ft->filename); */
}

static ft_t ft_queue[2];

static void
sigint(int s)
{
    if (s == SIGINT) {
	if (ft_queue[0])
	    ft_queue[0]->file.eof = 1;
	if (ft_queue[1])
	    ft_queue[1]->file.eof = 1;
    }
}

void
sigintreg(ft)
ft_t ft;
{
    if (ft_queue[0] == 0)
	ft_queue[0] = ft;
    else
	ft_queue[1] = ft;
    signal(SIGINT, sigint);
}
