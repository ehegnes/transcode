/*
 * framecode.h
 * Copyright (c) 2002 Chris C. Hoover
 * cchoover@charter.net
 */

#ifndef _FRAMECODE_H_
#define _FRAMECODE_H_

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

struct fc_time {
  double        fps; /* frames per second */

  unsigned int  sh; /* start hour */
  unsigned int  sm; /* start minute */
  unsigned int  ss; /* start second */
  unsigned int  sf; /* start frame */
  unsigned int  stf; /* calculated start frame */

  unsigned int  eh; /* end hour */ 
  unsigned int  em; /* end minute */
  unsigned int  es; /* end second */
  unsigned int  ef; /* end frame */
  unsigned int  etf; /* calculated end frame */

  unsigned int  stepf; /* step in frames */
  int vob_offset; /* keep track of vob offset for multiple time ranges */

  struct fc_time * next; /* pointer to next range */
};

int yylex( struct fc_time *fc_list, int *fc_to, int fc_verb );

struct fc_time * new_fc_time( void );
struct fc_time * tail_fc_time( struct fc_time * tlist );
int parse_fc_time_string( char * string, double fps, char *separator, int verbose, struct fc_time **fc_list );
int append_fc_time( struct fc_time * list, struct fc_time * time );
void fc_set_start_time( struct fc_time * time, unsigned int frames );
void fc_time_normalize( struct fc_time * list, int fc_verb );
int fc_frame_in_time( struct fc_time * list, unsigned int frame );
void free_fc_time( struct fc_time * time );

#define rindex(a,b) strrchr(a,b)
#define index(a,b) strchr(a,b)

#endif


