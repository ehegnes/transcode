/*
 *  callback.c
 *
 *  Copyright (C) Thomas Östreich - October 2002
 *
 *  This file is part of transcode, a linux video stream processing tool
 *      
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#include "transcode.h"
#include "encoder.h"

#include <xio.h>

//-----------------------------------------------------------------
//
// r,R - switch output file
//
//-----------------------------------------------------------------

static int rotate_ctr=0;
static int rotate_flag=0;
static char *base=NULL;
static int pause_flag=0;

void tc_pause_request(void)
{
    pause_flag = !pause_flag;
}

void tc_pause(void)
{
    while (pause_flag) usleep(TC_DELAY_MIN);
}

void tc_outstream_rotate_request(void)
{
  //set flag
  rotate_flag=1;
}

void tc_outstream_rotate(void)
{
  
  char buf[TC_BUF_MAX];
  vob_t *vob=tc_get_vob();

  transfer_t export_para;

  if(!rotate_flag) return;

  //reset flag to avoid re-entry
  rotate_flag=0;

  // do not try to rename /dev/null
  if(strcmp(vob->video_out_file, "/dev/null") == 0) return;

  // get current outfile basename
  base=strdup(vob->video_out_file);

  //check
  if(base==NULL) return;

  // close output
  if(encoder_close(&export_para)<0)
    tc_error("failed to close output");      
  
  // create new filename 
  sprintf(buf, "%s-%03d", base, rotate_ctr++);

  //rename old outputfile
  if(xio_rename(base, buf)<0) tc_error("failed to rename output file\n");

  // reopen output
  if(encoder_open(&export_para, vob)<0) 
    tc_error("failed to open output");      

  fprintf(stderr, "\n(%s) outfile %s saved to %s\n", __FILE__, base, buf);

  free(base);
  
}
