/*
 *  filter_text
 *
 *  Copyright (C) Tilmann Bitterberg - April 2003
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

#define MOD_NAME    "filter_text.so"
#define MOD_VERSION "v0.1.4 (2004-02-14)"
#define MOD_CAP     "write text in the image"
#define MOD_AUTHOR  "Tilmann Bitterberg"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <unistd.h>
#include <inttypes.h>

// FreeType specific includes
#include <ft2build.h>
#ifdef FT_FREETYPE_H
#include FT_FREETYPE_H


#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"
#include "video_trans.h"

// basic parameter

enum POS { NONE, TOP_LEFT, TOP_RIGHT, BOT_LEFT, BOT_RIGHT, CENTER, BOT_CENTER };

#define MAX_OPACITY 100

static unsigned char yuv255to224[] = {
 16,  17,  18,  19,  20,  20,  21,  22,  23,  24,  25,  26,  27,  27,  28, 
 29,  30,  31,  32,  33,  34,  34,  35,  36,  37,  38,  39,  40,  41,  41, 
 42,  43,  44,  45,  46,  47,  48,  49,  49,  50,  51,  52,  53,  54,  55, 
 56,  56,  57,  58,  59,  60,  61,  62,  63,  63,  64,  65,  66,  67,  68, 
 69,  70,  70,  71,  72,  73,  74,  75,  76,  77,  77,  78,  79,  80,  81, 
 82,  83,  84,  85,  85,  86,  87,  88,  89,  90,  91,  92,  92,  93,  94, 
 95,  96,  97,  98,  99,  99, 100, 101, 102, 103, 104, 105, 106, 106, 107, 
108, 109, 110, 111, 112, 113, 114, 114, 115, 116, 117, 118, 119, 120, 121, 
121, 122, 123, 124, 125, 126, 127, 128, 128, 129, 130, 131, 132, 133, 134, 
135, 135, 136, 137, 138, 139, 140, 141, 142, 142, 143, 144, 145, 146, 147, 
148, 149, 150, 150, 151, 152, 153, 154, 155, 156, 157, 157, 158, 159, 160, 
161, 162, 163, 164, 164, 165, 166, 167, 168, 169, 170, 171, 171, 172, 173, 
174, 175, 176, 177, 178, 179, 179, 180, 181, 182, 183, 184, 185, 186, 186, 
187, 188, 189, 190, 191, 192, 193, 193, 194, 195, 196, 197, 198, 199, 200, 
200, 201, 202, 203, 204, 205, 206, 207, 207, 208, 209, 210, 211, 212, 213, 
214, 215, 215, 216, 217, 218, 219, 220, 221, 222, 222, 223, 224, 225, 226, 
227, 228, 229, 229, 230, 231, 232, 233, 234, 235, 236, 236, 237, 238, 239, 240
};

typedef struct MyFilterData {
    /* public */
	unsigned int start;  /* start frame */
	unsigned int end;    /* end frame */
	unsigned int step;   /* every Nth frame */
	unsigned int dpi;    /* dots per inch resolution */
	unsigned int points; /* pointsize */
	char *font;          /* full path to font to use */
	int posx;            /* X offset in video */
	int posy;            /* Y offset in video */
	enum POS pos;        /* predifined position */
	char *string;        /* text to display */
	int fade;            /* fade in/out (speed) */
	int transparent;     /* do not draw a black bounding box */
	int antialias;       /* do sub frame anti-aliasing (not done) */
	int R, G, B;         /* color to apply in RGB */
	int Y, U, V;         /* color to apply in YUV */
	int flip;

    /* private */
	int opaque;          /* Opaqueness of the text */
	int boolstep;
	int top_space;
	int do_time;
	int start_fade_out;
	int boundX, boundY;
	int fade_in, fade_out;


	FT_Library  library;
	FT_Face     face;
	FT_GlyphSlot  slot;

} MyFilterData;

static MyFilterData *mfd = NULL;

static void help_optstr(void) 
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf ("    This filter renders text into the video stream\n");
   printf ("* Options\n");
   printf ("         'range' apply filter to [start-end]/step frames [0-oo/1]\n");
   printf ("           'dpi' dots-per-inch resolution [96]\n");
   printf ("        'points' point size of font in 1/64 [25]\n");
   printf ("          'font' full path to font file [/usr/X11R6/.../arial.ttf]\n");
   printf ("        'string' text to print [date]\n");
   printf ("          'fade' Fade in and/or fade out [0=off, 1=slow, 10=fast]\n");
   printf (" 'notransparent' disable transparency\n");
   printf ("           'pos' Position (0-width x 0-height) [0x0]\n");
   printf ("        'posdef' Position (0=None 1=TopL 2=TopR 3=BotL 4=BotR 5=Cent 6=BotCent) [0]\n");
}

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;

  static int width=0, height=0;
  static int size, codec=0;
  int w, h, i;
  int error;
  time_t mytime = time(NULL);
  static char *buf = NULL;
  char *p, *q;
  char *default_font = "/usr/X11R6/lib/X11/fonts/TrueType/arial.ttf";
  extern int flip; // transcode.c
  
  if (ptr->tag & TC_AUDIO)
      return 0;

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char b[128];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYO", "1");

      snprintf(b, 128, "%u-%u/%d", mfd->start, mfd->end, mfd->step);
      optstr_param (options, "range", "apply filter to [start-end]/step frames", 
	      "%u-%u/%d", b, "0", "oo", "0", "oo", "1", "oo");

      optstr_param (options, "string", "text to display (no ':') [defaults to `date`]", 
	      "%s", mfd->string);
      
      optstr_param (options, "font", "full path to font file [defaults to arial.ttf]", 
	      "%s", mfd->font);

      snprintf(b, 128, "%d", mfd->points);
      optstr_param (options, "points", "size of font (in points)", 
	      "%d", b, "1", "100");

      snprintf(b, 128, "%d", mfd->dpi);
      optstr_param (options, "dpi", "resolution of font (in dpi)", 
	      "%d", b, "72", "300");

      snprintf(b, 128, "%d", mfd->fade);
      optstr_param (options, "fade", "fade in/out (0=off, 1=slow, 10=fast)", 
	      "%d", b, "0", "10");

      snprintf(b, 128, "%d", mfd->antialias);
      optstr_param (options, "antialias", "Anti-Alias text (0=off 1=on)", 
	      "%d", b, "0", "10");

      optstr_param (options, "pos", "Position (0-width x 0-height)",  
	      "%dx%d", "0x0", "0", "width", "0", "height");

      optstr_param (options, "posdef", "Position (0=None 1=TopL 2=TopR 3=BotL 4=BotR 5=Cent 6=BotCent)",  "%d", "0", "0", "5");

      optstr_param (options, "notransparent", "disable transparency (enables block box)", "", "0");

      return 0;
  }
  
  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);

    if((mfd = (MyFilterData *)malloc (sizeof(MyFilterData))) == NULL) return (-1);

    // just to be safe
    memset (mfd, 0, sizeof(MyFilterData));

    mfd->start=0;
    mfd->end=(unsigned int)-1;
    mfd->step=1;

    mfd->points=25;
    mfd->dpi = 96;
    mfd->font = strdup(default_font);
    mfd->string = NULL;

    mfd->fade = 0;
    mfd->posx=0;
    mfd->posy=0;
    mfd->pos=NONE;
    mfd->transparent=1;
    mfd->antialias=1;

    mfd->do_time=1;
    mfd->opaque=MAX_OPACITY;
    mfd->fade_in = 0;
    mfd->fade_out = 0;
    mfd->start_fade_out=0;
    mfd->top_space = 0;
    mfd->boundX=0;
    mfd->boundY=0;
    mfd->flip = flip;

    //if the user wants flipping, do it here in this filter
    if (mfd->flip) flip=TC_FALSE;

    mfd->R = mfd->B = mfd->G = 0xff; // white
    mfd->Y = 240; mfd->U = mfd->V = 128;


    // do `date` as default
    mytime = time(NULL);
    mfd->string = ctime(&mytime);
    mfd->string[strlen(mfd->string)-1] = '\0';

    if (options != NULL) {
	char font[PATH_MAX];
	char string[PATH_MAX];

	memset (font, 0, PATH_MAX);
	memset (string, 0, PATH_MAX);
    
	if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

	optstr_get (options, "range",  "%u-%u/%d", &mfd->start, &mfd->end, &mfd->step);
	optstr_get (options, "dpi",    "%u",       &mfd->dpi);
	optstr_get (options, "points", "%u",       &mfd->points);
	optstr_get (options, "font",   "%[^:]",    &font);
	optstr_get (options, "posdef", "%d",       &mfd->pos);
	optstr_get (options, "pos",    "%dx%d",    &mfd->posx,  &mfd->posy);
	optstr_get (options, "string", "%[^:]",    &string);
	optstr_get (options, "fade",   "%d",       &mfd->fade);
	optstr_get (options, "antialias",   "%d",       &mfd->antialias);
	optstr_get (options, "color",   "%2x%2x%2x",  &mfd->R, &mfd->G, &mfd->B);
        mfd->Y =  (0.257 * mfd->R) + (0.504 * mfd->G) + (0.098 * mfd->B) + 16;
        mfd->U =  (0.439 * mfd->R) - (0.368 * mfd->G) - (0.071 * mfd->B) + 128;
        mfd->V = -(0.148 * mfd->R) - (0.291 * mfd->G) + (0.439 * mfd->B) + 128;

	if (optstr_lookup (options, "notransparent") )
	    mfd->transparent = !mfd->transparent;

	if (font && strlen(font)>0) {
	    free (mfd->font);
	    mfd->font=strdup(font);
	}

	if (string && strlen(string)>0) {
	    mfd->string=strdup(string);
	    mfd->do_time=0;
	}
	   
    }


    if (verbose) {
	printf (" Text Settings:\n");
	printf ("            string = \"%s\"\n", mfd->string);
	printf ("             range = %u-%u\n", mfd->start, mfd->end);
	printf ("              step = %u\n", mfd->step);
	printf ("               dpi = %u\n", mfd->dpi);
	printf ("            points = %u\n", mfd->points);
	printf ("              font = %s\n", mfd->font);
	printf ("            posdef = %d\n", mfd->pos);
	printf ("               pos = %dx%d\n", mfd->posx, mfd->posy);
	printf ("       color (RGB) = %x %x %x\n", mfd->R, mfd->G, mfd->B);
	printf ("       color (YUV) = %x %x %x\n", mfd->Y, mfd->U, mfd->V);
    }

    if (options)
	if (optstr_lookup (options, "help")) {
	    help_optstr();
	}

    if (mfd->start % mfd->step == 0) mfd->boolstep = 0;
    else mfd->boolstep = 1;

    width  = vob->ex_v_width;
    height = vob->ex_v_height;
    codec  = vob->im_v_codec;

    if (codec == CODEC_RGB)
      size = width*3;
    else 
      size = width*3/2;

    if((buf = (char *)malloc (height*size)) == NULL) return (-1);

    if (codec == CODEC_RGB)
	memset (buf, 0, height*size);
    else {
	memset (buf, 16, height*width);
	memset (buf+height*width, 128, height*width/2);
    }

    // init lib
    error = FT_Init_FreeType (&mfd->library);
    if (error) { fprintf(stderr, "[%s] ERROR: init lib!\n", MOD_NAME); return -1;}

    error = FT_New_Face (mfd->library, mfd->font, 0, &mfd->face);
    if (error == FT_Err_Unknown_File_Format) {
	fprintf(stderr, "[%s] ERROR: Unsupported font format\n", MOD_NAME);
	return -1;
    } else if (error) {
	fprintf(stderr, "[%s] ERROR: Cannot handle file\n", MOD_NAME);
	return -1;
    }
    
    error = FT_Set_Char_Size(
              mfd->face,      /* handle to face object           */
              0,              /* char_width in 1/64th of points  */
              mfd->points*64, /* char_height in 1/64th of points */
              mfd->dpi,       /* horizontal device resolution    */
              mfd->dpi );     /* vertical device resolution      */

    if (error) { fprintf(stderr, "[%s] ERROR: Cannot set char size\n", MOD_NAME); return -1; }
    
    // guess where the the groundline is
    // find the bounding box
    for (i=0; i<strlen(mfd->string); i++) {

	error = FT_Load_Char( mfd->face, mfd->string[i], FT_LOAD_RENDER);
	mfd->slot = mfd->face->glyph;

	if (mfd->top_space < mfd->slot->bitmap_top)
	    mfd->top_space = mfd->slot->bitmap_top;

	// if you think about it, its somehow correct ;)
	/*
	if (mfd->boundY < 2*mfd->slot->bitmap.rows - mfd->slot->bitmap_top)
	    mfd->boundY = 2*mfd->slot->bitmap.rows - mfd->slot->bitmap_top;
	    */
	if (mfd->boundY < 2*(mfd->slot->bitmap.rows) - mfd->slot->bitmap_top)
	    mfd->boundY = 2*(mfd->slot->bitmap.rows) - mfd->slot->bitmap_top;
	
	/*
	printf ("`%c\': rows(%2d) width(%2d) pitch(%2d) left(%2d) top(%2d) "
		"METRIC: width(%2d) height(%2d) bearX(%2d) bearY(%2d)\n",
		mfd->string[i], mfd->slot->bitmap.rows, mfd->slot->bitmap.width, 
		mfd->slot->bitmap.pitch, mfd->slot->bitmap_left, mfd->slot->bitmap_top,
		mfd->slot->metrics.width>>6, mfd->slot->metrics.height>>6,
		mfd->slot->metrics.horiBearingX>>6, mfd->slot->metrics.horiBearingY>>6);
		*/

	mfd->boundX += mfd->slot->advance.x >> 6;
    }

    switch (mfd->pos) {
	case NONE: /* 0 */
	    break;
	case TOP_LEFT:
	    mfd->posx = 0;
	    mfd->posy = 0;
	    break;
	case TOP_RIGHT:
	    mfd->posx = width  - mfd->boundX;
	    mfd->posy = 0;
	    break;
	case BOT_LEFT:
	    mfd->posx = 0;
	    mfd->posy = height - mfd->boundY;
	    break;
	case BOT_RIGHT:
	    mfd->posx = width  - mfd->boundX;
	    mfd->posy = height - mfd->boundY;
	    break;
	case CENTER:
	    mfd->posx = (width - mfd->boundX)/2;
	    mfd->posy = (height- mfd->boundY)/2;
	    /* align to not cause color disruption */
	    if (mfd->posx&1) mfd->posx++;
	    if (mfd->posy&1) mfd->posy++;
	    break;
	case BOT_CENTER:
	    mfd->posx = (width - mfd->boundX)/2;
	    mfd->posy = height - mfd->boundY;
	    if (mfd->posx&1) mfd->posx++;
	    break;
    }

    
    if ( mfd->posy < 0 || mfd->posx < 0 || 
	    mfd->posx+mfd->boundX > width ||
	    mfd->posy+mfd->boundY > height) {
	fprintf(stderr, "[%s] ERROR invalid position\n", MOD_NAME);
	return (-1);
    }


    //render into temp buffer
    if (codec == CODEC_YUV) {

	p = buf+mfd->posy*width+mfd->posx;

	for (i=0; i<strlen(mfd->string); i++) {

	    error = FT_Load_Char( mfd->face, mfd->string[i], FT_LOAD_RENDER);
	    mfd->slot = mfd->face->glyph;

	    if (verbose > 1) {
		// see http://www.freetype.org/freetype2/docs/tutorial/metrics.png
		/*
		printf ("`%c\': rows(%2d) width(%2d) pitch(%2d) left(%2d) top(%2d) "
			"METRIC: width(%2d) height(%2d) bearX(%2d) bearY(%2d)\n",
			mfd->string[i], mfd->slot->bitmap.rows, mfd->slot->bitmap.width, 
			mfd->slot->bitmap.pitch, mfd->slot->bitmap_left, mfd->slot->bitmap_top,
			mfd->slot->metrics.width>>6, mfd->slot->metrics.height>>6,
			mfd->slot->metrics.horiBearingX>>6, mfd->slot->metrics.horiBearingY>>6);
			*/
	    }

	    for (h=0; h<mfd->slot->bitmap.rows; h++) {
		for (w=0; w<mfd->slot->bitmap.width; w++)  {
		    unsigned char c = mfd->slot->bitmap.buffer[h*mfd->slot->bitmap.width+w] &0xff;
		    c = yuv255to224[c];
		    // make it transparent
		    if (mfd->transparent && c==16) continue;

		    p[width*(h+mfd->top_space - mfd->slot->bitmap_top) + 
			w+mfd->slot->bitmap_left] = c&0xff;

		    }
		}
	    p+=((mfd->slot->advance.x >> 6) - (mfd->slot->advance.y >> 6)*width);
	} 

    } else if (codec == CODEC_RGB) {

	p = buf + 3*(height-mfd->posy)*width + 3*mfd->posx;

	for (i=0; i<strlen(mfd->string); i++) {

	    // render the char
	    error = FT_Load_Char( mfd->face, mfd->string[i], FT_LOAD_RENDER);
	    mfd->slot = mfd->face->glyph; // shortcut

	    for (h=0; h<mfd->slot->bitmap.rows; h++) {
		for (w=0; w<mfd->slot->bitmap.width; w++)  {
		    unsigned char c = mfd->slot->bitmap.buffer[h*mfd->slot->bitmap.width+w];
		    c = c>254?254:c;
		    c = c<16?16:c;
		    // make it transparent
		    if (mfd->transparent && c==16) continue;

		    p[3*(width*(-(h+mfd->top_space - mfd->slot->bitmap_top)) + 
			    w+mfd->slot->bitmap_left)-2] = c&0xff;
		    p[3*(width*(-(h+mfd->top_space - mfd->slot->bitmap_top)) + 
			    w+mfd->slot->bitmap_left)-1] = c&0xff;
		    p[3*(width*(-(h+mfd->top_space - mfd->slot->bitmap_top)) + 
			    w+mfd->slot->bitmap_left)-0] = c&0xff;

		    }
		}

	    p+=3*((mfd->slot->advance.x >> 6) - (mfd->slot->advance.y >> 6));
	}
    }

    // filter init ok.
    if (verbose) printf("[%s] %s %s %dx%d-%d\n", MOD_NAME, MOD_VERSION, MOD_CAP, 
	    mfd->boundX, mfd->boundY, mfd->top_space);

    
    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

    if (mfd) { 
	FT_Done_Face (mfd->face );
	FT_Done_FreeType (mfd->library);
	free(mfd->font);
	if (!mfd->do_time)
	    free(mfd->string);
	free(mfd);
	free(buf);
	
    }
    mfd=NULL;
    buf=NULL;

    return(0);

  } /* filter close */
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------

    
  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  if((ptr->tag & TC_POST_PROCESS) && (ptr->tag & TC_VIDEO) && !(ptr->attributes & TC_FRAME_IS_SKIPPED))  {

    if (mfd->start <= ptr->id && ptr->id <= mfd->end && ptr->id%mfd->step == mfd->boolstep) {

	if (mfd->start == ptr->id && mfd->fade) {
	    mfd->fade_in = 1;
	    mfd->fade_out= 0;
	    mfd->opaque  = 0;
	    mfd->start_fade_out = mfd->end - MAX_OPACITY/mfd->fade - 1;
	    //if (mfd->start_fade_out < mfd->start) mfd->start_fade_out = mfd->start;
	}

	if (ptr->id == mfd->start_fade_out && mfd->fade) {
	    mfd->fade_in = 0;
	    mfd->fade_out = 1;
	}


	if (codec == CODEC_YUV) {
	    char *U, *V;

	    p = ptr->video_buf + mfd->posy*width + mfd->posx;
	    q =            buf + mfd->posy*width + mfd->posx;
	    U = ptr->video_buf + mfd->posy/2*width/2 + mfd->posx/2 +  ptr->v_width*ptr->v_height;
	    V = U + ptr->v_width*ptr->v_height/4;

	    for (h=0; h<mfd->boundY; h++) {
		for (w=0; w<mfd->boundX; w++)  {

		    unsigned int c = q[h*width+w]&0xff;
		    unsigned int d = p[h*width+w]&0xff;
		    unsigned int e = 0;
		    
		    // transparency
		    if (mfd->transparent && (c <= 16)) continue;

		    // opacity
		    e = ((MAX_OPACITY-mfd->opaque)*d + mfd->opaque*c)/MAX_OPACITY;
		    //e &= (mfd->Y&0xff);

		    // write to image
		    p[h*width+w] = e&0xff;

		    U[h/2*width/2+w/2] = mfd->U&0xff;
		    V[h/2*width/2+w/2] = mfd->V&0xff;
		}
	    }


	} else if (codec == CODEC_RGB) { // FIXME

	    p = ptr->video_buf + 3*(height-mfd->posy)*width + 3*mfd->posx;
	    q =            buf + 3*(height-mfd->posy)*width + 3*mfd->posx;

	    //tc_memcpy(ptr->video_buf, buf, 3*width*height);

	    for (h=0; h>-mfd->boundY; h--) {
		for (w=0; w<mfd->boundX; w++)  {
		    for (i=0; i<3; i++) {
			unsigned int c = q[3*(h*width+w)-(2-i)]&0xff;
			unsigned int d = p[3*(h*width+w)-(2-i)]&0xff;
			unsigned int e;
			if (mfd->transparent && c <= 16) continue;
		
			// opacity
			e = ((MAX_OPACITY-mfd->opaque)*d + mfd->opaque*c)/MAX_OPACITY;

			switch (i){
			    case 0: e &= (mfd->G); break;
			    case 1: e &= (mfd->R); break;
			    case 2: e &= (mfd->B); break;
			}

			// write to image
			p[3*(h*width+w)-(2-i)] =  e&0xff;
		    }
		}
	    }

	}

	if (mfd->fade && mfd->opaque>0 && mfd->fade_out) {
	    mfd->opaque -= mfd->fade;
	    if (mfd->opaque<0) mfd->opaque=0;
	}

	if (mfd->fade && mfd->opaque<MAX_OPACITY && mfd->fade_in) {
	    mfd->opaque += mfd->fade;
	    if (mfd->opaque>MAX_OPACITY) mfd->opaque=MAX_OPACITY;
	}

	if (mfd->flip) {
	    switch (codec) {
		case CODEC_RGB:
		    rgb_flip(ptr->video_buf, ptr->v_width, ptr->v_height);
		    break;
		case CODEC_YUV:
		    yuv_flip(ptr->video_buf, ptr->v_width, ptr->v_height);
		    break;
		case CODEC_YUV422:
		    yuv422_flip(ptr->video_buf, ptr->v_width, ptr->v_height);
		    break;
		default:
		    printf("unsupported\n");
		    break;
	    }
	}


    }
  }
  
  return(0);
}

#else
int tc_filter(vframe_list_t *ptr, char *options)
{
    fprintf(stderr, "[%s] Your freetype installation is missing header files\n");
    return -1;
}
#endif // FT_FREETYPE_H
