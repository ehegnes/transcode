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
  double        fps;

  unsigned int  sh;
  unsigned int  sm;
  unsigned int  ss;
  unsigned int  sf;
  unsigned int  stf;

  unsigned int  eh;
  unsigned int  em;
  unsigned int  es;
  unsigned int  ef;
  unsigned int  etf;

  struct fc_time * next;
};

int yylex( void );

struct fc_time * new_fc_time( void );
struct fc_time * tail_fc_time( struct fc_time * tlist );
int parse_fc_time_string( char * string, double fps );
int append_fc_time( struct fc_time * list, struct fc_time * time );
void fc_set_start_time( struct fc_time * time, unsigned int frames );
void fc_time_normalize( struct fc_time * list );
void free_fc_time( struct fc_time * time );

#endif


