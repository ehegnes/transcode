/*
 *  filter_extsub.c
 *
 *  Copyright (C) Thomas Östreich - January 2002
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

#define MOD_NAME    "filter_extsub.so"
#define MOD_VERSION "0.3.5 (2003-10-15)"
#define MOD_CAP     "DVD subtitle overlay plugin"
#define MOD_AUTHOR  "Thomas Oestreich"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "dl_loader.h"
#include "import/magic.h"

#include "subtitle_buffer.h"
#include "subproc.h"

//provided by transcode
extern void init_aa_table(double aa_weight, double aa_bias);
extern void yuv_antialias(char *image, char *dest, int width, int height, int mode);

#define BUFFER_SIZE SIZE_RGB_FRAME
#define SUBTITLE_BUFFER 100

vob_t *vob=NULL;

static transfer_t import_para;

static pthread_t thread1;

static double f_pts, f_time;

//infos on activated subtitle
static char *sub_frame;
static char *vid_frame;
static char *tmp_frame;

static double sub_pts1=-1.0f, sub_pts2=-1.0f;
static int sub_xpos, sub_ypos;
static int sub_xlen, sub_ylen;
static int sub_id=0;
static int sub_colour[4];
static int sub_alpha[4];

static int codec;
static int vshift=0, tshift=0, post=0;

static unsigned int color1=0, color2=255;


//-------------------------------------------------------------------
//
// retrieve a valid subtitle
//
//-------------------------------------------------------------------

int subtitle_retrieve()
{

  sframe_list_t *sptr = NULL;
  sub_info_t sub;
  int n;

  // get a subtitle from buffer
  pthread_mutex_lock(&sframe_list_lock);
  
  if(sframe_fill_level(TC_BUFFER_EMPTY)) {
      pthread_mutex_unlock(&sframe_list_lock);
      return(-1);
  }
  
  //not beeing empty does not mean ready to go!!!!!

  if(sframe_fill_level(TC_BUFFER_READY)) {
      
      pthread_mutex_unlock(&sframe_list_lock);
      
      if((sptr = sframe_retrieve())==NULL) {
	  //this shouldn´t happen
	  fprintf(stderr, "(%s) internal error (S)\n", __FILE__);
	  return(-1);
      } 
  } else {
      pthread_mutex_unlock(&sframe_list_lock);
      return(-1);
  }
  
  //room for title pixels
  sub.frame = sub_frame;
  
  // conversion
  if(subproc_feedme(sptr->video_buf, sptr->video_size, sptr->id, sptr->pts, &sub)<0) {
    // problems, drop this subtitle
    if(verbose & TC_DEBUG) fprintf(stderr, "(%s) subtitle dropped\n", __FILE__);
    sframe_remove(sptr); 
    pthread_cond_signal(&sframe_list_full_cv); 
    return(-1);
  }
  
  //save data

  sub_id   = sptr->id;
  sub_pts1 = sptr->pts * f_time;
  sub_pts2 = sub_pts1 + sub.time/100.0;

  sub_xpos = sub.x;
  sub_ypos = sub.y;
  sub_xlen = sub.w;
  sub_ylen = sub.h;

  for(n=0; n<4;++n) sub_alpha[n] = sub.alpha[n];

  //release packet buffer
  sframe_remove(sptr);  
  pthread_cond_signal(&sframe_list_full_cv); 

  if(verbose & TC_STATS) 
    printf("[%s] got SUBTITLE %d with pts=%.3f dtime=%.3f\n", MOD_NAME, sub_id, sub_pts1, sub_pts2-sub_pts1); 
  
  return(0);
}

//-------------------------------------------------------------------
//
// assign colors to subtitle colors 0-4
//
//-------------------------------------------------------------------

static int color_set_done=0; 
static int anti_alias_done=0; 
static int skip_anti_alias=0;

static unsigned int ca=2, cb=3; 

static void get_subtitle_colors() {
  
  int y;
  
  for(y=0; y<sub_ylen*sub_xlen; ++y) ++sub_colour[(uint8_t) sub_frame[y]];
  
  if(sub_colour[0] || sub_colour[1] || sub_colour[2] || sub_colour[3]) {
    
    if(sub_colour[1]>sub_colour[2] && sub_colour[1]>sub_colour[3]) {
      ca = 1;
      cb = (sub_colour[2]>sub_colour[3]) ? 2:3;
    }

    if(sub_colour[2]>sub_colour[1] && sub_colour[2]>sub_colour[3]) {
      ca = 2;
      cb = (sub_colour[1]>sub_colour[3]) ? 1:3;
    }

    if(sub_colour[3]>sub_colour[1] && sub_colour[3]>sub_colour[2]) {
      ca = 3;
      cb = (sub_colour[1]>sub_colour[2]) ? 1:2;
    }
  }
  color_set_done=1;
  
  if(verbose & TC_DEBUG) {
    printf("[%s] color dis: 0=%d, 1=%d, 2=%d, 3=%d, ca=%d, cb=%d\n", __FILE__, sub_colour[0], sub_colour[1], sub_colour[2], sub_colour[3], ca, cb);
    printf("[%s] alpha dis: 0=%d, 1=%d, 2=%d, 3=%d, ca=%d, cb=%d\n", __FILE__, sub_alpha[0], sub_alpha[1], sub_alpha[2], sub_alpha[3], ca, cb);
  }
  
}

//-------------------------------------------------------------------
//
// anti-alias subtitle bitmap
//
//-------------------------------------------------------------------

void anti_alias_subtitle(int black) {
  
  int back_col=black;
  int n;

  if(color1<=black) color1=black+1;
  if(color2<=black) color2=black+1;
  
  for(n=0; n<sub_ylen*sub_xlen; ++n) {

    if(sub_frame[n] == ca) {
      sub_frame[n] = color1 & 0xff;
      back_col=black;
      goto next_pix;
    }
    
    if(sub_frame[n] == cb) {
      sub_frame[n] = color2 & 0xff;
      back_col=255;
      goto next_pix;
    }
    
    if(back_col==255) {
      sub_frame[n] = 255 & 0xff;
    } else sub_frame[n]=black;
    
  next_pix:
    continue;
  }
  
  //use transcode's anti-alias routine (full frame mode = 3)
  if(!skip_anti_alias) {
    yuv_antialias(sub_frame, tmp_frame, sub_xlen, sub_ylen, 3);
    tc_memcpy(sub_frame, tmp_frame, sub_xlen * sub_ylen);
  }

  anti_alias_done=1;

}

//-------------------------------------------------------------------
//
// YUV overlay
//
//-------------------------------------------------------------------

static void subtitle_overlay_yuv(char *vid_frame, int w, int h)
{

  int x, y, n, m;

  int eff_sub_ylen, off;

  if(verbose & TC_STATS) 
    printf("SUBTITLE id=%d, x=%d, y=%d, w=%d, h=%d, t=%f\n", sub_id, sub_xpos, sub_ypos, sub_xlen, sub_ylen, sub_pts2-sub_pts1);
  
  if(color_set_done==0) get_subtitle_colors(&ca, &cb);
  
  //check:
  eff_sub_ylen = (sub_ylen+vshift>h) ?  h-vshift:sub_ylen;

  off = (vshift>0) ? vshift:0;

  if(eff_sub_ylen<0 || off>eff_sub_ylen) {
    fprintf(stderr, "[%s] invalid subtitle shift parameter\n", __FILE__); 
    return;
  }

  if(!anti_alias_done) anti_alias_subtitle(16);

  //black the frame
  if(0) {
    for(y=0; y<w*h; ++y) vid_frame[y]=255;
    for(y=0; y<w*h/4; ++y) vid_frame[w*h+y]=80;
    for(y=0; y<w*h/4; ++y) vid_frame[w*h*5/4+y]=80;
  }
  
  n=0;
  
  for(y=0; y<eff_sub_ylen-off; ++y) {
    
    m = sub_xpos + (y+h-eff_sub_ylen)*w+vshift*w;
    
    for(x=0; x<sub_xlen; ++x) {
      
      if(sub_frame[n] != 16) {
	//Y
	vid_frame[m] = sub_frame[n] & 0xff;
	if(0) {
	  //U
	  if(x%2==0 && y%2==0)
	    vid_frame[w*h + x/2+ sub_xpos/2 + (y+h-eff_sub_ylen-vshift)*w/4] = 0x80 & 0xff;
	  //V
	  if(x%2==0 && y%2==0)
	    vid_frame[(w*h*5)/4 + x/2 +  sub_xpos/2 + (y+h-eff_sub_ylen-vshift)*w/4] = 0x80 & 0xff;
	}
      }
      //16=transparent
      ++m;
      ++n;
    } // progress columns
  } //progress rows
}

//-------------------------------------------------------------------
//
// RGB overlay 
//
//-------------------------------------------------------------------

static void subtitle_overlay_rgb(char *vid_frame, int w, int h)
{

  int x, y, n, m;

  int eff_sub_ylen, off;
  
  if(verbose & TC_STATS) 
    printf("SUBTITLE id=%d, x=%d, y=%d, w=%d, h=%d, t=%f\n", sub_id, sub_xpos, sub_ypos, sub_xlen, sub_ylen, sub_pts2-sub_pts1);
  
  if(color_set_done==0) get_subtitle_colors(&ca, &cb);
  
  n=0;

  //check:
  eff_sub_ylen = (sub_ylen+vshift>h) ?  h-vshift:sub_ylen;
  eff_sub_ylen = sub_ylen;

  off = (vshift<0) ? -vshift:0;

  if(eff_sub_ylen<0 || off>eff_sub_ylen) {
    fprintf(stderr, "[%s] invalid subtitle shift parameter\n", __FILE__); 
    return;
  }
  
  if(!anti_alias_done) anti_alias_subtitle(0);
  
  for(y=0; y<eff_sub_ylen-off; ++y) {
    
    m = sub_xpos*3 + (eff_sub_ylen-y+vshift+(off?0:vshift))*w*3;
    
    for(x=0; x<sub_xlen; ++x) {
      
      if(sub_frame[n] !=0) {
	vid_frame[m++]= sub_frame[n] & 0xff;
	vid_frame[m++]= sub_frame[n] & 0xff;
	vid_frame[m++]= sub_frame[n] & 0xff;
      } else {
	m=m+3;
      }
      ++n;
    }
  }
}

//-------------------------------------------------------------------
//
// subtitle overlay wrapper
//
//-------------------------------------------------------------------

void subtitle_overlay(char *vid_frame, int w, int h)
{
  if(codec == CODEC_RGB) subtitle_overlay_rgb(vid_frame, w, h);
  if(codec == CODEC_YUV) subtitle_overlay_yuv(vid_frame, w, h);
}

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static int is_optstr (char *buf) {
    if (strchr(buf, 'h')) return 1;
    if (strchr(buf, '='))
	return 1;
    return 0;
}

int tc_filter(vframe_list_t *ptr, char *options)
{

  int pre=0, vid=0;
  
  int subtitles=0;

  int n=0;

  // API explanation:
  // ================
  //
  // (1) need more infos, than get pointer to transcode global 
  //     information structure vob_t as defined in transcode.h.
  //
  // (2) 'tc_get_vob' and 'verbose' are exported by transcode.
  //
  // (3) filter is called first time with TC_FILTER_INIT flag set.
  //
  // (4) make sure to exit immediately if context (video/audio) or 
  //     placement of call (pre/post) is not compatible with the filters 
  //     intended purpose, since the filter is called 4 times per frame.
  //
  // (5) see framebuffer.h for a complete list of frame_list_t variables.
  //
  // (6) filter is last time with TC_FILTER_CLOSE flag set

  if (ptr->tag & TC_FILTER_GET_CONFIG) {
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYOE", "1");
      optstr_param (options, "track",   "Subtitle track to render",    "%d",    "0", "0", "255");
      optstr_param (options, "vertshift", "offset of subtitle with respect to bottom of frame in rows", "%d",  "0", "0", "height");
      optstr_param (options, "timeshift", "global display start time correction in msec",    "%d",    "0", "0", "-1");
      optstr_param (options, "antialias", "anti-aliasing the rendered text (0=off,1=on)",    "%d",    "1", "0", "1");
      optstr_param (options, "pre",   "Run as a pre filter",    "%d",    "1", "0", "1");
      optstr_param (options, "color1", "Make a subtitle color visible with given intensity", "%d",  "0", "0", "255");
      optstr_param (options, "color2", "Make a subtitle color visible with given intensity", "%d",  "0", "0", "255");
      optstr_param (options, "ca",   "Shuffle the color assignment by choosing another subtitle color",    "%d", "0", "0", "3");
      optstr_param (options, "cb",   "Shuffle the color assignment by choosing another subtitle color",    "%d", "0", "0", "3");

      return 0;
  }

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------
  

  if(ptr->tag & TC_FILTER_INIT) {
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    // filter init ok.
    
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

    //------------------------------------------------------------

    if(options != NULL) {
	if (!is_optstr(options)) {
	    n=sscanf(options,"%d:%d:%d:%d:%d:%d:%d:%d:%d", &vob->s_track, &vshift, &tshift, &skip_anti_alias, &post, &color1, &color2, &ca, &cb);
	} else { // new options
	    //fprintf(stderr, "[%s] NEW options\n", MOD_NAME);
	    optstr_get (options, "track", "%d", &vob->s_track);
	    optstr_get (options, "vertshift", "%d", &vshift);
	    optstr_get (options, "timeshift", "%d", &tshift);
	if (optstr_get (options, "antialias", "%d", &skip_anti_alias)>=0) skip_anti_alias = !skip_anti_alias;
	if (optstr_get (options, "pre", "%d", &post)>=0) post = !post;
	    optstr_get (options, "color1", "%d", &color1);
	    optstr_get (options, "color2", "%d", &color2);
	if (optstr_get (options, "ca", "%d", &ca)>=0) n = 9;
	if (optstr_get (options, "cb", "%d", &cb)>=0) n = 9;
	if (optstr_lookup (options, "help")) return (-1);
	}
    }

    if (vob->im_v_codec == CODEC_YUV)
	vshift = -vshift;
    
    if(n>8) color_set_done=1;
    
    if(verbose) printf("[%s] extracting subtitle 0x%x\n", MOD_NAME, vob->s_track+0x20);

    // start subtitle stream
    import_para.flag=TC_SUBEX;
  
    if(tcv_import(TC_IMPORT_OPEN, &import_para, vob)<0) {
      tc_error("popen subtitle stream");
    }
    
    //------------------------------------------------------------

    subproc_init(NULL, "title", subtitles, (unsigned short)vob->s_track);

    // buffer allocation:

    sframe_alloc(SUBTITLE_BUFFER, import_para.fd);

    // start thread
    if(pthread_create(&thread1, NULL, (void *) subtitle_reader, NULL)!=0)
      tc_error("failed to start subtitle import thread");    

    // misc:

    if (post) {
	f_time = 1./vob->ex_fps;
    } else {
	f_time = 1./vob->fps;
    }

    codec = vob->im_v_codec;
    
    if ((sub_frame = malloc(BUFFER_SIZE))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    } else
      memset(sub_frame, 0, BUFFER_SIZE);  

    if ((vid_frame = malloc(BUFFER_SIZE))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    } else
      memset(vid_frame, 0, BUFFER_SIZE);  

    if ((tmp_frame = malloc(BUFFER_SIZE))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    } else
      memset(tmp_frame, 0, BUFFER_SIZE);  
 
    //for ant-aliasing
    if(!skip_anti_alias) init_aa_table(vob->aa_weight, vob->aa_bias);

    return(0);
  }
  
  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

    void * status;
    
    pthread_cancel(thread1);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
    pthread_cond_signal(&sframe_list_full_cv);
#endif
    pthread_join(thread1, &status);
    
    import_para.flag=TC_SUBEX;

    if(import_para.fd!=NULL) pclose(import_para.fd);
    
    import_para.fd=NULL;

    //FIXME: module already removed by main process

    //    if(tca_import(TC_IMPORT_CLOSE, &import_para, vob)<0) {
    //  tc_error("pclose subtitle stream");
    //}

    if(vid_frame) free(vid_frame);
    if(sub_frame) free(sub_frame);
    
    return(0);
  }
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------

  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  if(verbose & TC_STATS) 
    printf("[%s] %s/%s %s %s\n", MOD_NAME, vob->mod_path, MOD_NAME, MOD_VERSION, MOD_CAP);
  
  if(post) {
    pre = (ptr->tag & TC_POST_S_PROCESS)? 1:0;
  } else {
    pre = (ptr->tag & TC_PRE_S_PROCESS)? 1:0;
  }
  vid = (ptr->tag & TC_VIDEO)? 1:0;

  if(pre==0 || vid==0) return(0);

  //-------------------------------------------------------------------------
  //
  // below is a very fuzzy concept of rendering the subtitle on the movie
  // 
  // (1) check if we have a valid subtitle and render it
  // (2) if (1) fails try to get a new one
  // (3) buffer and display the new one if it's showtime, if not return
  // 
  // repeat steps throughout the movie
  
  // calculate current frame video PTS in [s]
  // adjust for dropped frames so far
  // adjust for user shift (in milliseconds)

  f_pts = f_time*(ptr->id-tc_get_frames_dropped()+vob->psu_offset) + tshift/1000.;

  if(verbose & TC_DEBUG) 
    printf("[%s] frame=%06d pts=%.3f sub1=%.3f sub2=%.3f\n", MOD_NAME, ptr->id, f_pts, sub_pts1, sub_pts2); 
  
  //overlay now?
  if(sub_pts1 <= f_pts && f_pts <= sub_pts2) {
    subtitle_overlay(ptr->video_buf, ptr->v_width, ptr->v_height);
    return(0);
  }
  
  //get a new subtitle, if the last one has expired:

  //reset anti-alias info
  anti_alias_done=0;
  
  if(f_pts > sub_pts2) {
    if(subtitle_retrieve()<0) {
      if(verbose & TC_STATS) 
	printf("[%s] no subtitle available at this time\n", __FILE__); 
      return(0);
    }
  }
  
  //overlay now?
  if(sub_pts1 < f_pts && f_pts < sub_pts2) subtitle_overlay(ptr->video_buf, ptr->v_width, ptr->v_height);
  
  return(0);
}
