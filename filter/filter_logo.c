/*
 *  filter_logo.c
 *
 *  Copyright (C) Tilmann Bitterberg - April 2002
 *
 *  This file is part of transcode, a video stream processing tool
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

    /* TODO: 
        - animated gif/png support -> done
        - sequences of jpgs maybe    
	  would be nice.
     */

#define MOD_NAME    "filter_logo.so"
#define MOD_VERSION "v0.10 (2003-10-16)"
#define MOD_CAP     "render image in videostream"
#define MOD_AUTHOR  "Tilmann Bitterberg"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#include "export/vid_aux.h"


// transcode defines this as well as ImageMagick.
#undef PACKAGE_NAME
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_STRING

#include <magick/api.h>

// basic parameter

enum POS { NONE, TOP_LEFT, TOP_RIGHT, BOT_LEFT, BOT_RIGHT, CENTER };

typedef struct MyFilterData {
    /* public */
	char file[PATH_MAX]; /* input filename */
	int posx;            /* X offset in video */
	int posy;            /* Y offset in video */
	enum POS pos;        /* predifined position */
	int flip;            /* bool if to flip image */
	int ignoredelay;     /* allow the user to ignore delays */
	int rgbswap;         /* bool if swap colors */
	int grayout;         /* only render lume values */
	unsigned int start, end;   /* ranges */

    /* private */
	unsigned int nr_of_images; /* animated: number of images */
	unsigned int cur_seq;      /* animated: current image */
	int cur_delay;             /* animated: current delay */
	char **yuv;                /* buffer for RGB->YUV conversion */
} MyFilterData;
	
static MyFilterData *mfd = NULL;

/* from /src/transcode.c */
extern int rgbswap;
extern int flip;
/* should probably honor the other flags too */ 

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static void help_optstr(void) 
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf("    This filter renders an user specified image into the video.\n");
   printf("    Any image format ImageMagick can read is accepted.\n");
   printf("    Transparent images are also supported.\n");
   printf("    Image origin is at the very top left.\n");

   printf ("* Options\n");
   printf ("        'file' Image filename (required) [logo.png]\n");
   printf ("         'pos' Position (0-width x 0-height) [0x0]\n");
   printf ("      'posdef' Position (0=None, 1=TopL, 2=TopR, 3=BotL, 4=BotR, 5=Center) [0]\n");
   printf ("       'range' Restrict rendering to framerange (0-oo) [0-end]\n");
   printf ("        'flip' Mirror image (0=off, 1=on) [0]\n");
   printf ("     'rgbswap' Swap colors [0]\n");
   printf ("     'grayout' YUV only: don't write Cb and Cr, makes a nice effect [0]\n");
   printf (" 'ignoredelay' Ignore delay specified in animations [0]\n");
}

int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  static vob_t *vob=NULL;

  static ExceptionInfo exception_info;
  static ImageInfo *image_info;
  static Image *image;
  static Image *images;
  static PixelPacket *pixel_packet;

  Image *timg;
  Image *nimg;

  int column, row;
  int rgb_off = 0;
  
  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_GET_CONFIG) {

      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYO", "1");
      // buf, name, comment, format, val, from, to  
      optstr_param (options, "file",   "Image filename",    "%s",    "logo.png");
      optstr_param (options, "posdef", "Position (0=None, 1=TopL, 2=TopR, 3=BotL, 4=BotR, 5=Center)",  "%d", "0", "0", "5");
      optstr_param (options, "pos",    "Position (0-width x 0-height)",  "%dx%d", "0x0", "0", "width", "0", "height");
      optstr_param (options, "range",  "Restrict rendering to framerange",  "%u-%u", "0-0", "0", "oo", "0", "oo");

      // bools
      optstr_param (options, "ignoredelay", "Ignore delay specified in animations", "", "0"); 
      optstr_param (options, "rgbswap", "Swap red/blue colors", "", "0");
      optstr_param (options, "grayout", "YUV only: don't write Cb and Cr, makes a nice effect", "",  "0");
      optstr_param (options, "flip",   "Mirror image",  "", "0");

      return 0;
  }

  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);

    if((mfd = (MyFilterData *)malloc (sizeof(MyFilterData))) == NULL) return (-1);

    //mfd->file = filename;
    strlcpy (mfd->file, "logo.png", PATH_MAX);
    mfd->yuv   = NULL;
    mfd->flip  = 0;
    mfd->pos   = 0;
    mfd->posx  = 0;
    mfd->posy  = 0;
    mfd->start = 0;
    mfd->end   = (unsigned int)-1;
    mfd->ignoredelay = 0;
    mfd->rgbswap     = 0;
    mfd->grayout     = 0;
    
    mfd->nr_of_images = 0;
    mfd->cur_seq      = 0;

    if (options != NULL) {
    
	if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

	optstr_get (options, "file",     "%[^:]",    &mfd->file);
	optstr_get (options, "posdef",   "%d",        &mfd->pos);
	optstr_get (options, "pos",      "%dx%d",     &mfd->posx,  &mfd->posy);
	optstr_get (options, "range",    "%u-%u",     &mfd->start, &mfd->end);

	if (optstr_get (options, "ignoredelay", "") >= 0) mfd->ignoredelay=!mfd->ignoredelay;
	if (optstr_get (options, "flip",  "") >= 0) mfd->flip = !mfd->flip;
	if (optstr_get (options, "rgbswap", "") >= 0) mfd->rgbswap=!mfd->rgbswap;
	if (optstr_get (options, "grayout", "") >= 0) mfd->grayout=!mfd->grayout;

	if (optstr_get (options, "help", "") >= 0)
	    help_optstr();
    }

    if (verbose > 1) {
	printf (" Logo renderer Settings:\n");
	printf ("              file = %s\n", mfd->file);
	printf ("            posdef = %d\n", mfd->pos);
	printf ("               pos = %dx%d\n", mfd->posx, mfd->posy);
	printf ("             range = %u-%u\n", mfd->start, mfd->end);
	printf ("              flip = %d\n", mfd->flip);
	printf ("       ignoredelay = %d\n", mfd->ignoredelay);
	printf ("           rgbswap = %d\n", mfd->rgbswap);
    }

    InitializeMagick("");

    GetExceptionInfo (&exception_info);
    image_info = CloneImageInfo ((ImageInfo *) NULL);
    strlcpy(image_info->filename, mfd->file, MaxTextExtent);

    image = ReadImage (image_info, &exception_info);
    if (image == (Image *) NULL) {
        MagickWarning (exception_info.severity, exception_info.reason, exception_info.description);
	strlcpy(mfd->file, "/dev/null", PATH_MAX);
	return 0;
    }


    if (image->columns > vob->ex_v_width || image->rows > vob->ex_v_height) {
	fprintf(stderr, "[%s] ERROR \"%s\" is too large\n", MOD_NAME, mfd->file);
	return(-1);
    }

    if (vob->im_v_codec == CODEC_YUV) {
	if ( (image->columns&1) || (image->rows&1)) {
	    fprintf(stderr, "[%s] ERROR \"%s\" has odd sizes\n", MOD_NAME, mfd->file);
	    return(-1);
	}
    }

    images = (Image *)GetFirstImageInList(image);
    nimg = NewImageList();

    while ( images != (Image *)NULL) {

	if (mfd->flip || flip) {
	    timg = FlipImage (images, &exception_info);
	    if (timg == (Image *) NULL) {
		MagickError (exception_info.severity, exception_info.reason, 
			exception_info.description);
		return -1;
	    }
	    AppendImageToList(&nimg, timg);
	}

	images = GetNextImageInList(images);
	mfd->nr_of_images++;
    }

    // check for memleaks;
    //DestroyImageList(image);
    if (mfd->flip || flip) {
	image = nimg;
    }
    
    /* initial delay. real delay = 1/100 sec * delay */
    mfd->cur_delay = image->delay*vob->fps/100;

    if (verbose & TC_DEBUG)
    printf("Nr: %d Delay: %d image->del %lu|\n", mfd->nr_of_images,  
	    mfd->cur_delay, image->delay );

    if (vob->im_v_codec == CODEC_YUV) {

	int i;

	if (!mfd->yuv) {
	    mfd->yuv = (char **) malloc(sizeof(char *) * mfd->nr_of_images); 
	    if (!mfd->yuv) {
		fprintf (stderr, "[%s:%d] ERROR out of memory\n", __FILE__, __LINE__);
		return (-1);
	    }
	    for (i=0; i<mfd->nr_of_images; i++) {
		mfd->yuv[i]=(char *)malloc(sizeof(char)*image->columns*image->rows*3);
		if (!mfd->yuv[i]) {
		    fprintf (stderr, "[%s:%d] ERROR out of memory\n", __FILE__, __LINE__);
		    return (-1);
		}
	    }
	}

	if (!tcv_convert_init(image->columns, image->rows)) {
	    fprintf(stderr, "[%s] ERROR image conversion init failed\n", MOD_NAME);
	    return(-1); 
	}

	/* convert Magick RGB format to 24bit RGB */
	if (!(rgbswap || mfd->rgbswap)) {

	    images = image;

	    for (i=0; i<mfd->nr_of_images; i++) {
		pixel_packet = GetImagePixels(images, 0, 0, images->columns, images->rows);
		for (row = 0; row < image->rows; row++) {
		    for (column = 0; column < image->columns; column++) {

			mfd->yuv[i][(row * image->columns + column) * 3 + 0] =
			    pixel_packet[image->columns*row + column].blue;

			mfd->yuv[i][(row * image->columns + column) * 3 + 1] =
			    pixel_packet[image->columns*row + column].green;

			mfd->yuv[i][(row * image->columns + column) * 3 + 2] =
			    pixel_packet[image->columns*row + column].red;
		    }
		}
		images=images->next;
	    }

	} else {

	    images = image;

	    for (i=0; i<mfd->nr_of_images; i++) {
		pixel_packet = GetImagePixels(images, 0, 0, images->columns, images->rows);
		for (row = 0; row < image->rows; row++) {
		    for (column = 0; column < image->columns; column++) {

			mfd->yuv[i][(row * image->columns + column) * 3 + 0] =
			    pixel_packet[image->columns*row + column].red;

			mfd->yuv[i][(row * image->columns + column) * 3 + 1] =
			    pixel_packet[image->columns*row + column].green;

			mfd->yuv[i][(row * image->columns + column) * 3 + 2] =
			    pixel_packet[image->columns*row + column].blue;
		    }
		}
		images=images->next;
	    }
	}

	for (i=0; i<mfd->nr_of_images; i++) {
	    if (!tcv_convert(mfd->yuv[i], IMG_RGB24, IMG_YUV_DEFAULT)) {
		fprintf(stderr, "[%s] ERROR rgb2yuv conversion failed\n", MOD_NAME);
		return(-1);
	    }
	}

    }  else {
	/* for RGB format is origin bottom left */
	rgb_off = vob->ex_v_height - image->rows;
	mfd->posy = rgb_off - mfd->posy;
    }

    switch (mfd->pos) {
	case NONE: /* 0 */
	    break;
	case TOP_LEFT:
	    mfd->posx = 0;
	    mfd->posy = rgb_off;
	    break;
	case TOP_RIGHT:
	    mfd->posx = vob->ex_v_width  - image->columns;
	    break;
	case BOT_LEFT:
	    mfd->posy = vob->ex_v_height - image->rows - rgb_off;
	    break;
	case BOT_RIGHT:
	    mfd->posx = vob->ex_v_width  - image->columns;
	    mfd->posy = vob->ex_v_height - image->rows - rgb_off;
	    break;
	case CENTER:
	    mfd->posx = (vob->ex_v_width - image->columns)/2;
	    mfd->posy = (vob->ex_v_height- image->rows)/2;
	    /* align to not cause color disruption */
	    if (mfd->posx&1) mfd->posx++;
	    if (mfd->posy&1) mfd->posy++;
	    break;
    }

    
    if ( mfd->posy < 0 || mfd->posx < 0 || 
	    mfd->posx+image->columns > vob->ex_v_width ||
	    mfd->posy+image->rows > vob->ex_v_height) {
	fprintf(stderr, "[%s] ERROR invalid position\n", MOD_NAME);
	return (-1);
    }

    /* for running through image sequence */
    images = image;

    // filter init ok.
    if (verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

    
    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

    if (mfd) { 
	int i;
	if (mfd->yuv) {
	    for (i=0; i<mfd->nr_of_images; i++)
		if (mfd->yuv[i]) {
		    free(mfd->yuv[i]);
		    mfd->yuv[i] = NULL;
		}
	    free(mfd->yuv);
	    mfd->yuv = NULL;
	}
	free(mfd);
	mfd = NULL;
    }

    if (image) {
	    DestroyImage(image);
	    DestroyImageInfo(image_info);
    }
    DestroyMagick();

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
      int seq;

      if (ptr->id < mfd->start || ptr->id > mfd->end)
	  return (0);

      if (!strcmp(mfd->file, "/dev/null"))
	      return 0;

      mfd->cur_delay--;

      if (mfd->cur_delay < 0 || mfd->ignoredelay) {
	  mfd->cur_seq = (mfd->cur_seq + 1) % mfd->nr_of_images;

	  images = image;
	  for (seq=0; seq<mfd->cur_seq; seq++)
	      images=images->next;

	  mfd->cur_delay = images->delay*vob->fps/100;
      }

      pixel_packet = GetImagePixels(images, 0, 0, images->columns, images->rows);

      if(vob->im_v_codec == CODEC_RGB) {

	  if (rgbswap || mfd->rgbswap) {
	      for (row = 0; row < image->rows; row++) {
		  for (column = 0; column < image->columns; column++) {
		      if (pixel_packet[(images->rows - row - 1) * images->columns + column].opacity == 0) {

			  int packet_off = (images->rows - row - 1) * images->columns + column;
			  int ptr_off    = ((row+mfd->posy)* vob->ex_v_width + column+mfd->posx) * 3;

			  ptr->video_buf[ptr_off + 0] = pixel_packet[packet_off].red;
			  ptr->video_buf[ptr_off + 1] = pixel_packet[packet_off].green;
			  ptr->video_buf[ptr_off + 2] = pixel_packet[packet_off].blue;
		      } /* !opaque */
		  }
	      }
	  } else { /* !rgbswap */
	      for (row = 0; row < images->rows; row++) {
		  for (column = 0; column < images->columns; column++) {
		      if (pixel_packet[(images->rows - row - 1) * images->columns + column].opacity == 0) {

			  int packet_off = (images->rows - row - 1) * images->columns + column;
			  int ptr_off    = ((row+mfd->posy)* vob->ex_v_width + column+mfd->posx) * 3;

			  ptr->video_buf[ptr_off + 0] = pixel_packet[packet_off].blue;
			  ptr->video_buf[ptr_off + 1] = pixel_packet[packet_off].green;
			  ptr->video_buf[ptr_off + 2] = pixel_packet[packet_off].red;
		      } /* !opaque */
		  }
	      }
	  }

      } else { /* !RGB */

	  int size  = vob->ex_v_width*vob->ex_v_height;
	  int block = images->columns*images->rows;
	  char *p1, *p2;
	  char *y1, *y2;

	  /* Y' */
	  for (row = 0; row < images->rows; row++) {
	      for (column = 0; column < images->columns; column++) {

		  if (pixel_packet[images->columns*row + column].opacity == 0) {

		      *(ptr->video_buf + (row+mfd->posy)*vob->ex_v_width + column + mfd->posx) = 
			  mfd->yuv[mfd->cur_seq][images->columns*row + column];

		  }
	      }
	  }

	  if (mfd->grayout)
	      return(0);

	  /* Cb, Cr */
	  p1 = ptr->video_buf + size + mfd->posy*vob->ex_v_width/4 + mfd->posx/2;
	  y1 = &(mfd->yuv[mfd->cur_seq][block]);
	  p2 = ptr->video_buf + 5*size/4 + mfd->posy*vob->ex_v_width/4 + mfd->posx/2;
	  y2 = &mfd->yuv[mfd->cur_seq][5*block/4];

	  for (row = 0; row < images->rows/2; row++) {
	      for (column = 0; column < images->columns/2; column++) {
		  if (pixel_packet[images->columns*row*2 + column*2].opacity == 0) {
		      p1[column] = y1[column];
		      p2[column] = y2[column];
		  }
	      }

	      p1 += vob->ex_v_width/2;
	      y1 += images->columns/2;

	      p2 += vob->ex_v_width/2;
	      y2 += images->columns/2;
	  }

      }
  }

  return(0);
}

