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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "../libdldarwin/dlfcn.h"
# endif
#endif


#define MOD_NAME    "filter_pv.so"
#define MOD_VERSION "v0.2.2 (2003-02-27)"
#define MOD_CAP     "xv only preview plugin"
#define MOD_AUTHOR  "Thomas Oestreich, Tilmann Bitterberg"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pv.h"
#include "optstr.h"
#include "font_xpm.h"
#include "filter.h"
#include "video_trans.h"

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
static char *undo_buffer = NULL;
static char *run_buffer[2] = {NULL, NULL};
static char *process_buffer[3] = {NULL, NULL, NULL};

static int process_ctr_cur=0;

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

char * preview_alloc_align_buffer(size_t size)
{
#ifdef HAVE_GETPAGESIZE
   long buffer_align=getpagesize();
#else
   long buffer_align=0;
#endif
   char *buf = malloc(size + buffer_align);

   long adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((long) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   return (buf + adjust);
}
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

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VYO", "1");
      optstr_param (options, "cache", "Number of raw frames to cache for seeking",  "%d", "15", "15", "255");
      optstr_param (options, "skip", "display only every Nth frame",  "%d", "0", "0", "255");
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
    
    if (options != NULL) {
      
      if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);
      
      optstr_get (options, "cache", "%d", &cache_num);
      
      //adjust for small buffers
      if(cache_num && cache_num<15) {
	cache_num=15;
	cache_long_skip=5;
      }

      optstr_get (options, "skip", "%d", &preview_skip_num);
      if (optstr_lookup(options, "help")) return -1;

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
      
    case CODEC_YUV422:
      
      if(xv_display_init(xv_player->display, 0, NULL, 
			 w, h, buffer, buffer, 1)<0) return(-1);
			 size = w*h*2;

      break;
    
    case CODEC_YUV:
      
      if(xv_display_init(xv_player->display, 0, NULL, 
			 w, h, buffer, buffer, 0)<0) return(-1);

      break;
    
    case CODEC_RAW_YUV:
    
      if(xv_display_init(xv_player->display, 0, NULL, 
			  w, h, buffer, buffer, 0)<0) return(-1);
      use_secondary_buffer=1;
      break;
      
    default:
      fprintf(stderr, "[%s] non-YUV codecs not supported for this preview plug-in\n", MOD_NAME);
      return(-1);
    }

    //cache

    if (cache_num) {
      if(preview_cache_init()<0) return(-1);

      if ((undo_buffer = preview_alloc_align_buffer(SIZE_RGB_FRAME)) == NULL)
	  return (-1);
      if ((run_buffer[0] = preview_alloc_align_buffer (SIZE_RGB_FRAME)) == NULL)
	  return (-1);
      if ((run_buffer[1] = preview_alloc_align_buffer (SIZE_RGB_FRAME)) == NULL)
	  return (-1);
      if ((process_buffer[0] = preview_alloc_align_buffer (SIZE_RGB_FRAME)) == NULL)
	  return (-1);
      if ((process_buffer[1] = preview_alloc_align_buffer (SIZE_RGB_FRAME)) == NULL)
	  return (-1);
      if ((process_buffer[2] = preview_alloc_align_buffer (SIZE_RGB_FRAME)) == NULL)
	  return (-1);
      
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

  //we do nothing if not properly initialized
  if(!xv_init_ok) return(0);

  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  pre = (ptr->tag & TC_POST_S_PROCESS)? 1:0;
  vid = (ptr->tag & TC_VIDEO)? 1:0;
  
  if( (ptr->tag & TC_PRE_PROCESS) && vid && cache_enabled) {
      process_ctr_cur = (process_ctr_cur+1)%3;
      memcpy (process_buffer[process_ctr_cur], ptr->video_buf, ptr->video_size);
      return 0;
  }
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

int preview_filter_buffer(int frames_needed)
{
    int current,i;

    static int this_filter = -1;
    static vframe_list_t *ptr = NULL; 
    vob_t *vob = tc_get_vob();

    if (ptr == NULL) 
	ptr = malloc (sizeof (vframe_list_t));
    memset (ptr, 0, sizeof (vframe_list_t));

    if (!cache_enabled) return 0;

    if (this_filter == -1);
	this_filter = plugin_find_id("pv");

    for (current = frames_needed, i = 1; current > 0; current--, i++){

#undef NO_PROCESS
#ifdef NO_PROCESS
	memcpy (run_buffer[0], (char *)vid_buf[cache_ptr-(current-1)], size);
	memcpy (run_buffer[1], (char *)vid_buf[cache_ptr-(current-1)], size);
#else
	memcpy (run_buffer[0], process_buffer[(process_ctr_cur+1)%3], SIZE_RGB_FRAME);
	memcpy (run_buffer[1], process_buffer[(process_ctr_cur+1)%3], SIZE_RGB_FRAME);
#endif

	if (i == 1) {
	    memcpy (undo_buffer, (char *)vid_buf[cache_ptr], size);
	}

	ptr->bufid = 1;
	ptr->next = ptr;

	ptr->filter_id = 0;
	ptr->v_codec = CODEC_YUV;
	ptr->id  = i; // frame
	ptr->internal_video_buf_0 = run_buffer[0];
	ptr->internal_video_buf_1 = run_buffer[1];

	// RGB
	ptr->video_buf_RGB[0]=ptr->internal_video_buf_0;
	ptr->video_buf_RGB[1]=ptr->internal_video_buf_1;

	//YUV
	ptr->video_buf_Y[0] = ptr->internal_video_buf_0;
	ptr->video_buf_Y[1] = ptr->internal_video_buf_1;

	ptr->video_buf_U[0] = ptr->video_buf_Y[0]
	    + TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT;
	ptr->video_buf_U[1] = ptr->video_buf_Y[1]
	    + TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT;

	ptr->video_buf_V[0] = ptr->video_buf_U[0]
	    + (TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT)/4;
	ptr->video_buf_V[1] = ptr->video_buf_U[1]
	    + (TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT)/4;

	//default pointer
	ptr->video_buf  = ptr->internal_video_buf_0;
	ptr->video_buf2 = ptr->internal_video_buf_1;
	ptr->free = 1;

#ifdef NO_PROCESS
	ptr->video_size = size;
	ptr->v_width = w;
	ptr->v_height = h;
#else
	ptr->v_width = vob->im_v_width;
	ptr->v_height = vob->im_v_height;
	ptr->video_size = vob->im_v_width*vob->im_v_height*3/2;
#endif

	// we disable this filter (filter_pv), because it does not make sense
	// to be run in the preview loop
	plugin_disable_id (this_filter);

	// PRE
	ptr->tag= TC_VIDEO | TC_PRE_S_PROCESS  | TC_PRE_M_PROCESS;
	process_vid_plugins (ptr);

	// CORE
	process_vid_frame(vob, ptr);

	// POST
	ptr->tag= TC_VIDEO | TC_POST_S_PROCESS | TC_POST_M_PROCESS;
	process_vid_plugins (ptr);

	plugin_enable_id (this_filter);
	
	memcpy (vid_buf[cache_ptr-current+1], ptr->video_buf, size);
	preview_cache_draw(0);

	memcpy ((char *)vid_buf[cache_ptr], undo_buffer, size);
    }
    return 0;
}
#if 0
void preview_filter(void)
{
    FILE *f;
    FILE *g;
    char tmpfile[] = "/tmp/filter-select";
    char infile[] = "/tmp/filter-in";
    char buf[1024];
    char filter_name[255];
    char *config, *c;
    int filter_handle, this_filter=-1, disable = 0;
    int frames_needed = 1;
    int current=0;
    int i;
    
    if (!cache_enabled) return;

    // build commandline
    snprintf (buf, 1024, 
	   "xterm -title \"Transcode Filter select\" -e %s/filter_list.awk %s %s &&  cat %s && rm -f %s",
	   vob->mod_path, vob->mod_path, tmpfile, tmpfile, tmpfile);
    if ((f = popen (buf, "r")) == NULL) {
	perror ("popen filter select");
	return;
    }
    // recycle
    memset (buf, 0, 1024);

    // block until data is available
    // filter Name
    fgets(buf, 1024, f);
    // delete newline
    buf[strlen(buf)-1] = '\0';
    strcpy (filter_name, buf);

    // (c)onfig or (d)isable
    memset (buf, 0, 1024);
    fgets(buf, 1024, f);
    buf[strlen(buf)-1] = '\0';
    if ( strcmp (buf, "(d)") == 0) { disable = 1; }

    pclose (f);

    if (disable) {
	filter_handle = plugin_find_id (filter_name);
	if (filter_handle == -1) {
	    // not loaded
	    return; 
	} else {
	    plugin_disable_id(filter_handle);
	    goto redisplay_frame;
	}
    }
    filter_handle = plugin_get_handle (filter_name);

    this_filter  = plugin_find_id ("pv");
    fprintf (stderr, "[%s] this_filter (%d)\n", MOD_NAME, this_filter);
    
    // we now have a valid ID
    if ( (config = filter_single_readconf(filter_handle)) == NULL) {
	fprintf(stderr, "[%s] Filter \"%s\" can not be configured.\n", MOD_NAME, filter_name);
    }

    if ((g = fopen(tmpfile, "w")) != NULL) {
	fputs (config, g);
	fclose (g);
    } else {
	fprintf(stderr, "[%s] unable to write to %s.\n", MOD_NAME, tmpfile);
	return;
    }
    
    if ((c = strchr (config, '\n'))) {
	*c = '\0';
	optstr_frames_needed (config, &frames_needed);
    } else 
	frames_needed = 1;
    
    printf ("XXX optstr_frames_needed:(%d)\n", frames_needed);


    free (config);

    // recycle
    memset (buf, 0, 1024);

    snprintf (buf, 1024, 
	  "xterm -title \"Transcode parameters\" -e %s/parse_csv.awk %s %s %s && cat %s && rm -f %s %s",
	  vob->mod_path, tmpfile, filter_name, infile, infile, tmpfile, infile);

    if ((f = popen (buf, "r")) == NULL) {
	perror ("popen filter param");
	return;
    }

    // recycle
    memset (buf, 0, 1024);

    // block until data is available
    fgets(buf, 1024, f);
    pclose (f);


    buf[strlen(buf)-1] = '\0';

    //fprintf(stderr, "XX buf (%s)", buf);
    // XXX
    if (buf && *buf)
	filter_single_configure_handle (filter_handle, strchr (buf, '='));

redisplay_frame: 
    // logoaway pos=210x136:size=257x175:mode=2
    for (current = frames_needed, i = 1; current > 0; current--, i++){

	vframe_list_t ptr;

	if (!disable)
	    memcpy (undo_buffer, (char *)vid_buf[cache_ptr], size);

	memcpy (run_buffer[0], (char *)vid_buf[cache_ptr-(current-1)], size);
	memcpy (run_buffer[1], (char *)vid_buf[cache_ptr-(current-1)], size);

	ptr.bufid = 1;
	ptr.next = &ptr;
	ptr.tag= TC_VIDEO | TC_POST_M_PROCESS | TC_PRE_M_PROCESS |
	    TC_POST_S_PROCESS | TC_PRE_S_PROCESS;

	ptr.filter_id = 0;
	ptr.v_codec = CODEC_YUV;
	ptr.id  = i; // frame
	ptr.internal_video_buf_0 = run_buffer[0];
	ptr.internal_video_buf_1 = run_buffer[1];

	// RGB
	ptr.video_buf_RGB[0]=ptr.internal_video_buf_0;
	ptr.video_buf_RGB[1]=ptr.internal_video_buf_1;

	//YUV
	ptr.video_buf_Y[0] = ptr.internal_video_buf_0;
	ptr.video_buf_Y[1] = ptr.internal_video_buf_1;

	ptr.video_buf_U[0] = ptr.video_buf_Y[0]
	    + TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT;
	ptr.video_buf_U[1] = ptr.video_buf_Y[1]
	    + TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT;

	ptr.video_buf_V[0] = ptr.video_buf_U[0]
	    + (TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT)/4;
	ptr.video_buf_V[1] = ptr.video_buf_U[1]
	    + (TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT)/4;

	//default pointer
	ptr.video_buf  = ptr.internal_video_buf_0;
	ptr.video_buf2 = ptr.internal_video_buf_1;
	ptr.free = 1;

	ptr.video_size = size;
	ptr.v_width = w;
	ptr.v_height = h;


	// we disable this filter (filter_pv), because it does not make sense
	// to be run in the preview loop
	plugin_disable_id (this_filter);
	process_vid_plugins (&ptr);
	plugin_enable_id (this_filter);
	
	memcpy (vid_buf[cache_ptr-current+1], ptr.video_buf, size);
	preview_cache_draw(0);
    }
    return;

}
#endif

void preview_cache_undo(void) 
{
    if (!cache_enabled) return;

    memcpy((char *)vid_buf[cache_ptr], undo_buffer, size);
    preview_cache_draw(0);
}
void preview_cache_draw(int next) {

  if(!cache_enabled) return;
  
  cache_ptr+=next;
  
  if(next < 0) cache_ptr+=cache_num;
  while (cache_ptr<0)
      cache_ptr+=cache_num;
  
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
		img[(posy+h)*width+posx+w] = (c[h][w] == '+')?230:img[(posy+h)*width+posx+w];
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

int preview_grab_jpeg(void) 
{
#if defined(__FreeBSD__) || defined (__APPLE__)
    const
#endif  
    char *error;
    char *prefix = "preview_grab-";
    static vob_t *mvob;
    static void *jpeg_vhandle = NULL;
    static int (*JPEG_export)(int opt, void *para1, void *para2);
    static int counter = 0;

    char module[TC_BUF_MAX];
    transfer_t export_para;
    int ret = 0;

    if(!cache_enabled) return 1;
    
    if (jpeg_vhandle == NULL) { 
	sprintf(module, "%s/export_%s.so", MOD_PATH, "jpg");
	jpeg_vhandle = dlopen(module, RTLD_GLOBAL| RTLD_LAZY);
	if (!jpeg_vhandle) {
	    tc_warn("%s", dlerror());
	    tc_warn("(%s) loading \"%s\" failed", __FILE__, module);
	    return(1);
	}
	JPEG_export = dlsym(jpeg_vhandle, "tc_export");   
	if ((error = dlerror()) != NULL)  {
	    tc_warn("%s", error);
	    return(1);
	}
	export_para.flag = TC_DEBUG;
	ret = JPEG_export(TC_EXPORT_NAME, &export_para, NULL);

	mvob = malloc(sizeof(vob_t));
	memcpy (mvob, vob, sizeof(vob_t));
	mvob->video_out_file = prefix;

	export_para.flag = TC_VIDEO;
	if((ret=JPEG_export(TC_EXPORT_INIT, &export_para, mvob))==TC_EXPORT_ERROR) {
	    tc_warn("(%s) video jpg export module error: init failed", __FILE__);
	    return(1);
	}

	export_para.flag = TC_VIDEO;	
	if((ret=JPEG_export(TC_EXPORT_OPEN, &export_para, mvob))==TC_EXPORT_ERROR) {
	    tc_warn("(%s) video export module error: open failed", __FILE__);
	    return(1);
	}
    }

    // encode and export video frame
    export_para.buffer = (char *)vid_buf[cache_ptr];
    export_para.size   = size;
    export_para.attributes = TC_FRAME_IS_KEYFRAME;
    export_para.flag   = TC_VIDEO;

    if(JPEG_export(TC_EXPORT_ENCODE, &export_para, mvob)<0) {
	tc_warn("(%s) error encoding jpg frame", __FILE__);
	return 1;
    }
    printf("[%s] Saved JPEG to %s%06d.jpg\n", "filter_pv", prefix, counter++);


    return 0;
}

/* vim:sw=4
 */
