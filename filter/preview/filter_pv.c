/*
 *  filter_pv.c
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

#define MOD_NAME    "filter_pv.so"
#define MOD_VERSION "v0.1.1 (2002-10-17)"
#define MOD_CAP     "xv only preview plugin"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pv.h"
#include "optstr.h"
#include "font_xpm.h"

static int cache_num=0;
static int cache_ptr=0;
static int cache_enabled=0;

//cache navigation
int cache_long_skip  = 25;
int cache_short_skip =  1;

static char *vid_buf_mem=NULL;
static char **vid_buf=NULL;

static int w, h;

static int cols=20;
static int rows=20;

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "framebuffer.h"

static char buffer[128];
static int size=0;
static int use_secondary_buffer=0;

static int xv_init_ok=0;

static int preview_delay=0;
static int preview_skip=0, preview_skip_num=25;

vob_t *vob=NULL;

/* global variables */

static xv_player_t *xv_player = NULL;


#define ONE_SECOND 1000000 // in units of usec

void inc_preview_delay()
{
  preview_delay+=ONE_SECOND/10;
  if(preview_delay>ONE_SECOND) preview_delay=ONE_SECOND;
}

void dec_preview_delay()
{
  preview_delay-=ONE_SECOND/10;
  if(preview_delay<0) preview_delay=0;
}

void preview_toggle_skip()
{
  preview_skip = (preview_skip>0) ? 0: preview_skip_num; 
}

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

int tc_filter(vframe_list_t *ptr, char *options)
{

  int pre=0, vid=0;

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


  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);
    
    // filter init ok.
    
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    if (options != NULL) {
      
      if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);
      
      optstr_get (options, "cache", "%d", &cache_num);
      
      //adjust for small buffers
      if(cache_num && cache_num<15) {
	cache_num=15;
	cache_long_skip=5;
      }

      optstr_get (options, "skip", "%d", &preview_skip_num);

    }

    if(cache_num<0) printf("[%s] invalid cache number - exit\n", MOD_NAME);
    if(preview_skip_num<0) printf("[%s] invalid number of frames to skip - exit\n", MOD_NAME);

    sprintf(buffer, "%s-%s", PACKAGE, VERSION);
    
    if(xv_player != NULL) return(-1);
    if(!(xv_player = xv_player_new())) return(-1);

    //init filter
    
    w = tc_x_preview;
    h = tc_y_preview;

    size = w*h* 3/2;
    
    if(verbose) printf("[%s] preview window %dx%d\n", MOD_NAME, w, h);
    
    switch(vob->im_v_codec) {
      
    case CODEC_YUV:
      
      if(xv_display_init(xv_player->display, 0, NULL, 
			 w, h, buffer, buffer)<0) return(-1);

      break;
    
    case CODEC_RAW_YUV:
    
      if(xv_display_init(xv_player->display, 0, NULL, 
			  w, h, buffer, buffer)<0) return(-1);
      use_secondary_buffer=1;
      break;
      
    default:
      fprintf(stderr, "[%s] non-YUV codecs not supported for this preview plug-in\n", MOD_NAME);
      return(-1);
    }

    //cache

    if (cache_num) {
      if(preview_cache_init()<0) return(-1);
    }

    xv_init_ok=1;

    return(0);
  }
  
  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

    if(!xv_init_ok) return(0);

    if(size) xv_display_exit(xv_player->display);

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
  
  if(verbose & TC_STATS) printf("[%s] %s/%s %s %s\n", MOD_NAME, vob->mod_path, MOD_NAME, MOD_VERSION, MOD_CAP);
  
  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  pre = (ptr->tag & TC_POST_S_PROCESS)? 1:0;
  vid = (ptr->tag & TC_VIDEO)? 1:0;
  
  if(pre && vid) {
    
    if(preview_skip && (ptr->id % preview_skip_num)) return(0);
    
    if(!xv_player->display->dontdraw) {
      
      //0.6.2 (secondaray buffer for pass-through mode)
      (use_secondary_buffer) ? memcpy(xv_player->display->pixels[0], (char*) ptr->video_buf2, size) : memcpy(xv_player->display->pixels[0], (char*) ptr->video_buf, size); 
      
      //display video frame
      xv_display_show(xv_player->display);
      
      if(cache_enabled) preview_cache_submit(xv_player->display->pixels[0], ptr->id, ptr->attributes);
      
      if(preview_delay) usleep(preview_delay);

    } else {
      
      //check only for X11-events
      xv_display_event(xv_player->display);
    
    }//dontdraw=1?
  
  }//correct slot?
  
  return(0);
}

int preview_cache_init() {
  
  //size must be know!
  
  int n;

  if((vid_buf_mem = (char *) calloc(cache_num, size))==NULL) {
    perror("out of memory");
    return(-1);
  }
  
  if((vid_buf = (char **) calloc(cache_num, sizeof(char *)))==NULL) {
    perror("out of memory");
    return(-1);
  }
  
  for (n=0; n<cache_num; ++n) vid_buf[n] = (char *) (vid_buf_mem + n * size);

  cache_enabled=1;

  return(0);

}

void preview_cache_submit(char *buf, int id, int flag) {    

  char string[255];
  memset (string, 0, 255);
  
  if(!cache_enabled) return;

  cache_ptr = (cache_ptr+1)%cache_num;
  
  memcpy((char*) vid_buf[cache_ptr], buf, size);
  
  (flag & TC_FRAME_IS_KEYFRAME) ? sprintf(string, "%u *", id):sprintf(string, "%u", id);
  
  str2img (vid_buf[cache_ptr], string, w, h, cols, rows, 0, 0, CODEC_YUV);
}

void preview_cache_draw(int next) {

  if(!cache_enabled) return;
  
  cache_ptr+=next;
  
  if(next < 0) cache_ptr+=cache_num;
  
  cache_ptr%=cache_num;

  memcpy(xv_player->display->pixels[0], (char*) vid_buf[cache_ptr], size);

  //display video frame
  xv_display_show(xv_player->display);
  
  return;
  
}

void str2img(char *img, char *c, int width, int height, int char_width, int char_height, int posx, int posy, int codec)
{
    char **cur;
    int posxorig=posx;

    while (*c != '\0' && *c && c) {
	if (*c == '\n') {
	    posy+=char_height;
	    posx=posxorig;
	}
	if (posx+char_width >= width || posy >= height)
	    break;

	cur = char2bmp(*c);
	if (cur) {
	    bmp2img (img, cur, width, height, char_width, char_height, posx, posy, codec);
	    posx+=char_width;
	}

	c++;
    }
}

void bmp2img(char *img, char **c, int width, int height, int char_width, int char_height, int posx, int posy, int codec)
{
    int h, w;

    if (codec == CODEC_YUV) {
	for (h=0; h<char_height; h++) {
	    for (w=0; w<char_width; w++) {
		img[(posy+h)*width+posx+w] = (c[h][w] == '+')?255:img[(posy+h)*width+posx+w];
	    }

	}
    } else {
	for (h=0; h<char_height; h++) {
	    for (w=0; w<char_width; w++) {
		char *col=&img[3*((height-(posy+h))*width+posx+w)];
		*(col-0) = (c[h][w] == '+')?255:*(col-0);
		*(col-1) = (c[h][w] == '+')?255:*(col-1);
		*(col-2) = (c[h][w] == '+')?255:*(col-2);
	    }
	}
    }
    /*
	for (h=char_height-1; h>=0; h--) {
	    for (w=char_width-1; w>=0; w--) {
	    */
}

char **char2bmp(char c) {
    switch (c) {
	case '0': return  &null_xpm[4];
	case '1': return  &one_xpm[4];
	case '2': return  &two_xpm[4];
	case '3': return  &three_xpm[4];
	case '4': return  &four_xpm[4];
	case '5': return  &five_xpm[4];
	case '6': return  &six_xpm[4];
	case '7': return  &seven_xpm[4];
	case '8': return  &eight_xpm[4];
	case '9': return  &nine_xpm[4];
	case '-': return  &minus_xpm[4];
	case ':': return  &colon_xpm[4];
	case ' ': return  &space_xpm[4];
	case '!': return  &exklam_xpm[4];
	case '?': return  &quest_xpm[4];
	case '.': return  &dot_xpm[4];
	case ',': return  &comma_xpm[4];
	case ';': return  &semicomma_xpm[4];
	case 'A': return  &A_xpm[4];
	case 'a': return  &a_xpm[4];
	case 'B': return  &B_xpm[4];
	case 'b': return  &b_xpm[4];
	case 'C': return  &C_xpm[4];
	case 'c': return  &c_xpm[4];
	case 'D': return  &D_xpm[4];
	case 'd': return  &d_xpm[4];
	case 'E': return  &E_xpm[4];
	case 'e': return  &e_xpm[4];
	case 'F': return  &F_xpm[4];
	case 'f': return  &f_xpm[4];
	case 'G': return  &G_xpm[4];
	case 'g': return  &g_xpm[4];
	case 'H': return  &H_xpm[4];
	case 'h': return  &h_xpm[4];
	case 'I': return  &I_xpm[4];
	case 'i': return  &i_xpm[4];
	case 'J': return  &J_xpm[4];
	case 'j': return  &j_xpm[4];
	case 'K': return  &K_xpm[4];
	case 'k': return  &k_xpm[4];
	case 'L': return  &L_xpm[4];
	case 'l': return  &l_xpm[4];
	case 'M': return  &M_xpm[4];
	case 'm': return  &m_xpm[4];
	case 'N': return  &N_xpm[4];
	case 'n': return  &n_xpm[4];
	case 'O': return  &O_xpm[4];
	case 'o': return  &o_xpm[4];
	case 'P': return  &P_xpm[4];
	case 'p': return  &p_xpm[4];
	case 'Q': return  &Q_xpm[4];
	case 'q': return  &q_xpm[4];
	case 'R': return  &R_xpm[4];
	case 'r': return  &r_xpm[4];
	case 'S': return  &S_xpm[4];
	case 's': return  &s_xpm[4];
	case 'T': return  &T_xpm[4];
	case 't': return  &t_xpm[4];
	case 'U': return  &U_xpm[4];
	case 'u': return  &u_xpm[4];
	case 'V': return  &V_xpm[4];
	case 'v': return  &v_xpm[4];
	case 'W': return  &W_xpm[4];
	case 'w': return  &w_xpm[4];
	case 'X': return  &X_xpm[4];
	case 'x': return  &x_xpm[4];
	case 'Y': return  &Y_xpm[4];
	case 'y': return  &y_xpm[4];
	case 'Z': return  &Z_xpm[4];
	case 'z': return  &z_xpm[4];
	case '*': return  &ast_xpm[4];
	default: return NULL;
    }
    return NULL;
}

