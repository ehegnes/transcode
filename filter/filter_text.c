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
#define MOD_VERSION "v0.1.1 (2003-06-03)"
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

// basic parameter

enum POS { NONE, TOP_LEFT, TOP_RIGHT, BOT_LEFT, BOT_RIGHT, CENTER, BOT_CENTER };

#define MAX_OPACITY 100

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

  static int width, height;
  static int size, codec;
  int w, h, i;
  int error;
  time_t mytime = time(NULL);
  static char *buf = NULL;
  char *p, *q;
  char *default_font = "/usr/X11R6/lib/X11/fonts/TrueType/arial.ttf";
  
  if (ptr->tag & TC_AUDIO)
      return 0;

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[128];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYO", "1");

      snprintf(buf, 128, "%u-%u/%d", mfd->start, mfd->end, mfd->step);
      optstr_param (options, "range", "apply filter to [start-end]/step frames", 
	      "%u-%u/%d", buf, "0", "oo", "0", "oo", "1", "oo");

      optstr_param (options, "string", "text to display (no ':') [defaults to `date`]", 
	      "%s", mfd->string);
      
      optstr_param (options, "font", "full path to font file [defaults to arial.ttf]", 
	      "%s", mfd->font);

      snprintf(buf, 128, "%d", mfd->points);
      optstr_param (options, "points", "size of font (in points)", 
	      "%d", buf, "1", "100");

      snprintf(buf, 128, "%d", mfd->dpi);
      optstr_param (options, "dpi", "resolution of font (in dpi)", 
	      "%d", buf, "72", "300");

      snprintf(buf, 128, "%d", mfd->fade);
      optstr_param (options, "fade", "fade in/out (0=off, 1=slow, 10=fast)", 
	      "%d", buf, "0", "10");

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

    mfd->do_time=1;
    mfd->opaque=MAX_OPACITY;
    mfd->fade_in = 0;
    mfd->fade_out = 0;
    mfd->start_fade_out=0;
    mfd->top_space = 0;
    mfd->boundX=0;
    mfd->boundY=0;


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


    if (verbose > 1) {
	printf (" Text Settings:\n");
	printf ("             range = %u-%u\n", mfd->start, mfd->end);
	printf ("              step = %u\n", mfd->step);
	printf ("               dpi = %u\n", mfd->dpi);
	printf ("            points = %u\n", mfd->points);
	printf ("              font = %s\n", mfd->font);
	printf ("            posdef = %d\n", mfd->pos);
	printf ("               pos = %dx%d\n", mfd->posx, mfd->posy);
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
	if (mfd->boundY < 2*mfd->slot->bitmap.rows - mfd->slot->bitmap_top)
	    mfd->boundY = 2*mfd->slot->bitmap.rows - mfd->slot->bitmap_top;
	
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
	    int n = 0;

	    error = FT_Load_Char( mfd->face, mfd->string[i], FT_LOAD_RENDER);
	    mfd->slot = mfd->face->glyph;

	    if (verbose > 1) {
		// see http://www.freetype.org/freetype2/docs/tutorial/metrics.png
		printf ("`%c\': rows(%2d) width(%2d) pitch(%2d) left(%2d) top(%2d) "
			"METRIC: width(%2d) height(%2d) bearX(%2d) bearY(%2d)\n",
			mfd->string[i], mfd->slot->bitmap.rows, mfd->slot->bitmap.width, 
			mfd->slot->bitmap.pitch, mfd->slot->bitmap_left, mfd->slot->bitmap_top,
			mfd->slot->metrics.width>>6, mfd->slot->metrics.height>>6,
			mfd->slot->metrics.horiBearingX>>6, mfd->slot->metrics.horiBearingY>>6);
	    }

	    for (h=0; h<mfd->slot->bitmap.rows; h++) {
		for (w=0; w<mfd->slot->bitmap.width; w++)  {
		    unsigned char c = mfd->slot->bitmap.buffer[h*mfd->slot->bitmap.width+w];
		    c = c>254?254:c;
		    c = c<20?20:c;
		    // make it transparent
		    if (mfd->transparent && c==20) continue;

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
  
  if((ptr->tag & TC_POST_PROCESS) && (ptr->tag & TC_VIDEO))  {

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

	    p = ptr->video_buf + mfd->posy*width + mfd->posx;
	    q =            buf + mfd->posy*width + mfd->posx;

	    for (h=0; h<mfd->boundY; h++) {
		for (w=0; w<mfd->boundX; w++)  {

		    unsigned int c = q[h*width+w]&0xff;
		    unsigned int d = p[h*width+w]&0xff;
		    unsigned int e;
		    
		    // transparency
		    if (mfd->transparent && c < 20) continue;

		    // opacity
		    e = ((MAX_OPACITY-mfd->opaque)*d + mfd->opaque*c)/MAX_OPACITY;

		    // write to image
		    p[h*width+w] = e&0xff;
		}
	    }



	} else if (codec == CODEC_RGB) { // FIXME

	    p = ptr->video_buf + 3*(height-mfd->posy)*width + 3*mfd->posx;
	    q =            buf + 3*(height-mfd->posy)*width + 3*mfd->posx;

	    //memcpy(ptr->video_buf, buf, 3*width*height);

	    for (h=0; h>-mfd->boundY; h--) {
		for (w=0; w<mfd->boundX; w++)  {
		    for (i=0; i<3; i++) {
			unsigned int c = q[3*(h*width+w)-(2-i)]&0xff;
			unsigned int d = p[3*(h*width+w)-(2-i)]&0xff;
			unsigned int e;
			if (mfd->transparent && c < 16) continue;
		
			// opacity
			e = ((MAX_OPACITY-mfd->opaque)*d + mfd->opaque*c)/MAX_OPACITY;

			// write to image
			p[3*((h)*width+w)-(2-i)] =  e&0xff;
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
