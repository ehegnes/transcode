/*
 * framecode.l
 * framecode token scanner
 * Copyright (c) 2002 Chris C. Hoover
 * cchoover@charter.net
 */

%option noyywrap

%{

#include "framecode.h"

int fc_to = 0, fc_verb = 1;
struct fc_time * fc_list = NULL;

int
parse_fc_time_string( char * string, double fps )
{
  struct fc_time * time;
  char * hold;

  yy_scan_string( "" );
  hold = strtok( string, "," );

  while( hold != NULL ) {
    yy_scan_string( hold );

    if( ( time = new_fc_time( ) ) == NULL )
      return -1;

    memset (time, 0, sizeof(struct fc_time));

    time->fps = fps;

    if( fc_list == NULL )
      fc_list = time;

    else if( append_fc_time( fc_list, time ) == -1 )
      return -1;

    if( yylex( ) < 0 ) {
      printf( "[%s:%d]syntax error\n", __FILE__, __LINE__ );
      return -1;
    }

    hold = strtok( NULL, "," );
  }

  return 0;
}

void
fc_set_start_time( struct fc_time * time, unsigned int frames )
{
  struct fc_time * next;

  if( time != NULL ) {
    time->sh = 0;
    time->sm = 0;
    time->ss = 0;
    time->sf = frames;
    time->stf = 0;
    next = time->next;
    time->next = NULL;
    fc_time_normalize( time );
    time->next = next;
  }
}

void
fc_time_normalize( struct fc_time * list )
{
  unsigned int th, tm, ts, tf;

  while( list != NULL ) {
    // avoid fpu error
    if( list->fps == 0 )
      list->fps = 1;
    /*
     * start time
     */
    if( list->sf >= list->fps ) {
      list->ss += list->sf / list->fps;
      list->sf -= ( list->fps * ( list->sf / list->fps ) );
    }

    if( list->ss >= 60  ) {
      list->sm += list->ss / 60;
      list->ss -= ( 60  * ( list->ss / 60  ) );
    }

    if( list->sm >= 60  ) {
      list->sh += list->sm / 60;
      list->sm -= ( 60  * ( list->sm / 60  ) );
    }

    list->stf = list->sh * 3600 * list->fps + 
                list->sm *   60 * list->fps + 
		            list->ss * list->fps        + 
			          list->sf;
    /*
     * end time
     */
    if( list->ef >= list->fps ) {
      list->es += list->ef / list->fps;
      list->ef -= ( list->fps * ( list->ef / list->fps ) );
    }

    if( list->es >= 60  ) {
      list->em += list->es / 60;
      list->es -= ( 60  * ( list->es / 60  ) );
    }

    if( list->em >= 60  ) {
      list->eh += list->em / 60;
      list->em -= ( 60  * ( list->em / 60  ) );
    }

    list->etf = list->eh * 3600 * list->fps + 
                list->em *   60 * list->fps + 
		            list->es * list->fps        + 
			          list->ef;
    /*
     * swap values if required
     */
    if( list->stf > list->etf ) {
      th = list->sh;
      tm = list->sm;
      ts = list->ss;
      tf = list->sf;
      list->sh = list->eh;
      list->sm = list->em;
      list->ss = list->es;
      list->sf = list->ef;
      list->eh = th;
      list->em = tm;
      list->es = ts;
      list->ef = tf;
      tf = list->stf;
      list->stf = list->etf;
      list->etf = tf;

      if( fc_verb )
        printf( "reverse\n" );
    }

    list = list->next;
  }
}

struct fc_time *
new_fc_time( )
{
  struct fc_time * time = NULL;

  if( ( time = (struct fc_time *)malloc( sizeof( struct fc_time ) ) ) == NULL ) {
    printf( "[%s:%d] malloc failure\n", __FILE__, __LINE__ );
    return NULL;
  }

  return time;
}

void
free_fc_time( struct fc_time * time )
{
  struct fc_time * temp;

  while( time != NULL ) {
    temp = time->next;
    free( time );
    time = temp;
  }
}

struct fc_time *
tail_fc_time( struct fc_time * tlist )
{
  struct fc_time * temp = tlist;

  while( temp != NULL && temp->next != NULL )
    temp = temp->next;

  return temp;
}

int
append_fc_time( struct fc_time * list, struct fc_time * time )
{
  struct fc_time * temp;
  int result = -1;

  if( list != NULL && ( temp = tail_fc_time( list ) ) != NULL ) {
    temp->next = time;
    result = 0;
  }

  return result;
}

%}

DIGIT                             [0-9]
FRM_BLOCK                         \.?{DIGIT}*
HMS_BLOCK                         :?{DIGIT}*
TO                                -
OOPS                              .
SKIP                              [ \t\n]+
FRAMES                            {FRM_BLOCK}
SECONDS_AND_FRAMES                {HMS_BLOCK}{FRAMES}
MINUTES_SECONDS_AND_FRAMES        {HMS_BLOCK}{SECONDS_AND_FRAMES}
HOURS_MINUTES_SECONDS_AND_FRAMES  {DIGIT}*{MINUTES_SECONDS_AND_FRAMES}

%%

{SKIP}   /* eat up whitespace */

{FRAMES} {

  if( *yytext == '.' )
    yytext++;

  if( fc_verb )
    printf( "%d frames ", atoi( yytext ) );

  if( fc_to )
    tail_fc_time( fc_list )->ef = (unsigned)atoi( yytext );

  else
    tail_fc_time( fc_list )->sf = (unsigned)atoi( yytext );
}

{SECONDS_AND_FRAMES} {

  if( *yytext == ':' )
    yytext++;

  if( index( yytext, '.' ) == NULL ) {
    if( fc_verb )
      printf( "%d seconds ", atoi( yytext ) );

    if( fc_to )
 	    tail_fc_time( fc_list )->es = (unsigned)atoi( yytext );

    else
 	    tail_fc_time( fc_list )->ss = (unsigned)atoi( yytext );
  }

  else {
    if( fc_verb )
      printf( "%d seconds and %d frames ", atoi( yytext ), atoi( index( yytext, '.' ) + 1 ) );

    if( fc_to ) {
 	    tail_fc_time( fc_list )->es = (unsigned)atoi( yytext );
 	    tail_fc_time( fc_list )->ef = (unsigned)atoi( index( yytext, '.' ) + 1 );
    }
    else {
 	    tail_fc_time( fc_list )->ss = (unsigned)atoi( yytext );
 	    tail_fc_time( fc_list )->sf = (unsigned)atoi( index( yytext, '.' ) + 1 );
    }
  }
}

{MINUTES_SECONDS_AND_FRAMES} {

  if( *yytext == ':' )
    yytext++;

  if( index( yytext, '.' ) == NULL ) {
    if( fc_verb )
      printf( "%d minutes and %d seconds ", atoi( yytext ), atoi( index( yytext, ':' ) + 1 ) );

    if( fc_to ) {
 	    tail_fc_time( fc_list )->em = (unsigned)atoi( yytext );
 	    tail_fc_time( fc_list )->es = (unsigned)atoi( index( yytext, ':' ) + 1 );
    }

    else {
 	    tail_fc_time( fc_list )->sm = (unsigned)atoi( yytext );
 	    tail_fc_time( fc_list )->ss = (unsigned)atoi( index( yytext, ':' ) + 1 );
    }
  }

  else {
    if( fc_verb )
      printf( "%d minutes, %d seconds and %d frames ", atoi( yytext ), atoi( index( yytext, ':' ) + 1 ), atoi( index( yytext, '.' ) + 1 ) );

    if( fc_to ) {
 	    tail_fc_time( fc_list )->em = (unsigned)atoi( yytext );
 	    tail_fc_time( fc_list )->es = (unsigned)atoi( index( yytext, ':' ) + 1 );
      tail_fc_time( fc_list )->ef = (unsigned)atoi( index( yytext, '.' ) + 1 );
    }
    else {
 	    tail_fc_time( fc_list )->sm = (unsigned)atoi( yytext );
 	    tail_fc_time( fc_list )->ss = (unsigned)atoi( index( yytext, ':' ) + 1 );
      tail_fc_time( fc_list )->sf = (unsigned)atoi( index( yytext, '.' ) + 1 );
    }
  }
}

{HOURS_MINUTES_SECONDS_AND_FRAMES}  {

  if( index( yytext, '.' ) == NULL ) {
    if( fc_verb )
      printf( "%d hours, %d minutes and %d seconds ", atoi( yytext ), atoi( index( yytext, ':' ) + 1 ), atoi( rindex( yytext, ':' ) + 1 ) );

    if( fc_to ) {
 	    tail_fc_time( fc_list )->eh = (unsigned)atoi( yytext );
 	    tail_fc_time( fc_list )->em = (unsigned)atoi( index( yytext, ':' ) + 1 );
      tail_fc_time( fc_list )->es = (unsigned)atoi( rindex( yytext, ':' ) + 1 );
    }
    else {
 	    tail_fc_time( fc_list )->sh = (unsigned)atoi( yytext );
 	    tail_fc_time( fc_list )->sm = (unsigned)atoi( index( yytext, ':' ) + 1 );
      tail_fc_time( fc_list )->ss = (unsigned)atoi( rindex( yytext, ':' ) + 1 );
    }
  }

  else {
    if( fc_verb )
      printf( "%d hours, %d minutes, %d seconds and %d frames ", atoi( yytext ), atoi( index( yytext, ':' ) + 1 ), atoi( rindex( yytext, ':' ) + 1 ), atoi( index( yytext, '.' ) + 1 ) );

    if( fc_to ) {
 	    tail_fc_time( fc_list )->eh = (unsigned)atoi( yytext );
 	    tail_fc_time( fc_list )->em = (unsigned)atoi( index( yytext, ':' ) + 1 );
      tail_fc_time( fc_list )->es = (unsigned)atoi( rindex( yytext, ':' ) + 1 );
      tail_fc_time( fc_list )->ef = (unsigned)atoi( index( yytext, '.' ) + 1 );
    }
    else {
 	    tail_fc_time( fc_list )->sh = (unsigned)atoi( yytext );
 	    tail_fc_time( fc_list )->sm = (unsigned)atoi( index( yytext, ':' ) + 1 );
      tail_fc_time( fc_list )->ss = (unsigned)atoi( rindex( yytext, ':' ) + 1 );
      tail_fc_time( fc_list )->sf = (unsigned)atoi( index( yytext, '.' ) + 1 );
    }
  }
}

{TO} {

  if( fc_to == 1 ) {
    if( fc_verb )
      printf( "\n[%s:%d] too to\n", __FILE__, __LINE__ );
    return -1;
  }

  fc_to = 1;

  if( fc_verb )
    printf( "to " ); 
}

{OOPS} {

  if( fc_verb )
    printf( "\n[%s:%d] Unrecognized character: %s\n", __FILE__, __LINE__, yytext );

  return -1;
}

<<EOF>>  {

  fc_time_normalize( tail_fc_time( fc_list ) );
  fc_to = 0;

  if( fc_verb )
    printf( "\n" );

  yyterminate();
}

%%

