/*
 *  import_mpeg2.c
 *
 *  Copyright (C) Thomas �streich - June 2001
 *
 *  This file is part of transcode, a video stream  processing tool
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

#define MOD_NAME    "import_mpeg2.so"
#define MOD_VERSION "v0.4.0 (2003-10-02)"
#define MOD_CODEC   "(video) MPEG2"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_VID;

#define MOD_PRE mpeg2
#include "import_def.h"


extern int errno;
char import_cmd_buf[TC_BUF_MAX];

typedef struct tbuf_t {
	int off;
	int len;
	char *d;
} tbuf_t;

// m2v passthru
static int can_read = 1;
static tbuf_t tbuf;
static int m2v_passthru=0;
static FILE *f; // video fd


/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
  char requant_buf[256];
  long sret;

  if(param->flag != TC_VIDEO) return(TC_IMPORT_ERROR);
  
  if(vob->ts_pid1==0) { // no transport stream
    
    switch(vob->im_v_codec) {
      
    case CODEC_RGB:

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                      "tcextract -x mpeg2 -i \"%s\" -d %d |"
                      " tcdecode -x mpeg2 -d %d",
                      vob->video_in_file, vob->verbose, vob->verbose);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	return(TC_IMPORT_ERROR);

      break;
      
    case CODEC_YUV:

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                      "tcextract -x mpeg2 -i \"%s\" -d %d |"
                      " tcdecode -x mpeg2 -d %d -y yv12",
                      vob->video_in_file, vob->verbose, vob->verbose);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	return(TC_IMPORT_ERROR);

      break;

    case CODEC_RAW:
    case CODEC_RAW_YUV:
	
	memset(requant_buf, 0, sizeof (requant_buf)); 
	if (vob->m2v_requant > M2V_REQUANT_FACTOR) {
	  snprintf (requant_buf, 256, " | tcrequant -d %d -f %f ",
                    vob->verbose, vob->m2v_requant);
	}
	m2v_passthru=1;

        sret = snprintf(import_cmd_buf, TC_BUF_MAX, 
		        "tcextract -x mpeg2 -i \"%s\" -d %d%s", 
		        vob->video_in_file, vob->verbose, requant_buf);
        if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	  return(TC_IMPORT_ERROR);

	break;
    }
  
  } else {
    
    switch(vob->im_v_codec) {
      
    case CODEC_RGB:

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                      "tccat -i \"%s\" -d %d -n 0x%x |"
                      " tcextract -x mpeg2 -t m2v -d %d |"
                      " tcdecode -x mpeg2 -d %d",
                      vob->video_in_file, vob->verbose, vob->ts_pid1,
                      vob->verbose, vob->verbose);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	return(TC_IMPORT_ERROR);

      break;
      
    case CODEC_YUV:

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                      "tccat -i \"%s\" -d %d -n 0x%x |"
                      " tcextract -x mpeg2 -t m2v -d %d |"
                      " tcdecode -x mpeg2 -d %d -y yv12",
                      vob->video_in_file, vob->verbose,vob->ts_pid1,
                      vob->verbose, vob->verbose);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	return(TC_IMPORT_ERROR);

      break;
    }
  }
   
  // print out
  if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
  
  param->fd = NULL;
  
  // popen
  if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
    perror("popen RGB stream");
    return(TC_IMPORT_ERROR);
  }
     
  // we handle the read;
  if (m2v_passthru) {
    f = param->fd;
    param->fd = NULL;

    tbuf.d = malloc (SIZE_RGB_FRAME);
    tbuf.len = SIZE_RGB_FRAME;
    tbuf.off = 0;

    if ((tbuf.len = fread(tbuf.d, 1, tbuf.len, f)) < 0)
      return(TC_IMPORT_ERROR);

    // find a sync word
    while (tbuf.off+4<tbuf.len) {
      if (tbuf.d[tbuf.off+0]==0x0 && tbuf.d[tbuf.off+1]==0x0 && 
	  tbuf.d[tbuf.off+2]==0x1 && 
	  (unsigned char)tbuf.d[tbuf.off+3]==0xb3) break;
      else tbuf.off++;
    }
    if (tbuf.off+4>=tbuf.len)  {
      fprintf (stderr, "Internal Error. No sync word\n");
      return (TC_IMPORT_ERROR);
    }

  }

  return(TC_IMPORT_OK);
}

/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode{

  if(param->flag == TC_VIDEO && m2v_passthru) {

    // ---------------------------------------------------
    // This code splits the MPEG2 elementary stream
    // into packets. It sets the type of the packet
    // as an frame attribute.
    // I frames (== Key frames) are not only I frames,
    // they also carry the sequence headers in the packet.
    // ---------------------------------------------------

    int ID, start_seq, start_pic, pic_type;

    ID = tbuf.d[tbuf.off+3]&0xff;

    switch (ID) {
      case 0xb3: // sequence
	start_seq = tbuf.off;

	// look for pic header
	while (tbuf.off+6<tbuf.len) {

	  if (tbuf.d[tbuf.off+0]==0x0 && tbuf.d[tbuf.off+1]==0x0 && 
	      tbuf.d[tbuf.off+2]==0x1 && tbuf.d[tbuf.off+3]==0x0 && 
	      ((tbuf.d[tbuf.off+5]>>3)&0x7)>1 && 
	      ((tbuf.d[tbuf.off+5]>>3)&0x7)<4) {
	    if (verbose & TC_DEBUG)
              printf("Completed a sequence + I frame from %d -> %d\n", 
		      start_seq, tbuf.off);

	    param->attributes |= (TC_FRAME_IS_KEYFRAME | TC_FRAME_IS_I_FRAME);
	    param->size = tbuf.off-start_seq;

	    // spit frame out
	    tc_memcpy(param->buffer, tbuf.d+start_seq, param->size);
	    memmove(tbuf.d, tbuf.d+param->size, tbuf.len-param->size);
	    tbuf.off = 0;
	    tbuf.len -= param->size;

	    if (verbose & TC_DEBUG)
              printf("%02x %02x %02x %02x\n", tbuf.d[0]&0xff, tbuf.d[1]&0xff,
                                              tbuf.d[2]&0xff, tbuf.d[3]&0xff);
	    return TC_IMPORT_OK;
	  }
	  else tbuf.off++;
	}

	// not enough data.
	if (tbuf.off+6 >= tbuf.len) {

	  if (verbose & TC_DEBUG) printf("Fetching in Sequence\n");
	  memmove (tbuf.d, tbuf.d+start_seq, tbuf.len - start_seq);
	  tbuf.len -= start_seq;
	  tbuf.off = 0;

	  if (can_read>0) {
	    can_read = fread (tbuf.d+tbuf.len, SIZE_RGB_FRAME-tbuf.len, 1, f);
	    tbuf.len += (SIZE_RGB_FRAME-tbuf.len);
	  } else {
	    printf("No 1 Read %d\n", can_read);
	    /* XXX: Flush buffers */
	    return TC_IMPORT_ERROR;
	  }
	}
	break;

      case 0x00: // pic header

	start_pic = tbuf.off;
	pic_type = (tbuf.d[start_pic+5] >> 3) & 0x7;
	tbuf.off++;

	while (tbuf.off+6<tbuf.len) {
	  if (tbuf.d[tbuf.off+0]==0x0 && tbuf.d[tbuf.off+1]==0x0 && 
	      tbuf.d[tbuf.off+2]==0x1 && 
	      (unsigned char)tbuf.d[tbuf.off+3]==0xb3) {
	    if (verbose & TC_DEBUG)
               printf("found a last P or B frame %d -> %d\n", 
                       start_pic, tbuf.off);

	    param->size = tbuf.off - start_pic;
	    if (pic_type == 2) param->attributes |= TC_FRAME_IS_P_FRAME;
	    if (pic_type == 3) param->attributes |= TC_FRAME_IS_B_FRAME;

	    tc_memcpy(param->buffer, tbuf.d+start_pic, param->size);
	    memmove(tbuf.d, tbuf.d+param->size, tbuf.len-param->size);
	    tbuf.off = 0;
	    tbuf.len -= param->size;

	    return TC_IMPORT_OK;

	  } else if // P or B frame
	    (tbuf.d[tbuf.off+0]==0x0 && tbuf.d[tbuf.off+1]==0x0 && 
	     tbuf.d[tbuf.off+2]==0x1 && tbuf.d[tbuf.off+3]==0x0 && 
	     ((tbuf.d[tbuf.off+5]>>3)&0x7)>1 && 
	     ((tbuf.d[tbuf.off+5]>>3)&0x7)<4) {
	      if (verbose & TC_DEBUG)
                printf("found a P or B frame from %d -> %d\n", 
		        start_pic, tbuf.off);

	      param->size = tbuf.off - start_pic;
	      if (pic_type == 2) param->attributes |= TC_FRAME_IS_P_FRAME;
	      if (pic_type == 3) param->attributes |= TC_FRAME_IS_B_FRAME;

	      tc_memcpy(param->buffer, tbuf.d+start_pic, param->size);
	      memmove(tbuf.d, tbuf.d+param->size, tbuf.len-param->size);
	      tbuf.off = 0;
	      tbuf.len -= param->size;

	      return TC_IMPORT_OK;

	    } else tbuf.off++;

	  // not enough data.
	  if (tbuf.off+6 >= tbuf.len) {

	    memmove (tbuf.d, tbuf.d+start_pic, tbuf.len - start_pic);
	    tbuf.len -= start_pic;
	    tbuf.off = 0;

	    if (can_read>0) {
	      can_read = fread (tbuf.d+tbuf.len, SIZE_RGB_FRAME-tbuf.len, 1, f);
	      tbuf.len += (SIZE_RGB_FRAME-tbuf.len);
	    } else {
	      printf("No 1 Read %d\n", can_read);
	      /* XXX: Flush buffers */
	      return TC_IMPORT_ERROR;
	    }
	  }
	}
	break;
      default:
	// should not get here
	printf("Default case\n");
	tbuf.off++;
	break;
    }


  }
  return(TC_IMPORT_OK);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  

    if(param->fd != NULL) pclose(param->fd);
    if(f != NULL) pclose(f);
    param->fd = f = NULL;

    return(TC_IMPORT_OK);
}
