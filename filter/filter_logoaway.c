/*
 *  filter_logoaway.c
 *
 *  Copyright (C) Thomas Wehrspann - 2002/2003
 *
 *  This plugin is based on ideas of Krzysztof Wojdon's
 *  logoaway filter for VirtualDub
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

 /*
  * TODO:
  *   -coordinates in RGB=YUV   - DONE
  *   -shape mode               - DONE
  *   -blur                     -
 */

/*
 * ChangeLog:
 * v0.1 (2002-12-04)  Thomas Wehrspann <thomas@wehrspann.de>
 *    -First version
 *
 * v0.1 (2002-12-19) Tilmann Bitterberg
 *    -Support for new filter-API(optstr_param) added
 *
 * v0.2 (2003-01-15) Thomas Wehrspann <thomas@wehrspann.de>
 *    -Fixed RGB-SOLID-mode
 *    -Added alpha channel
 *      now you need ImageMagick
 *    -Documentation added
 *
 * v0.2 (2003-01-21) Tilmann Bitterberg
 *    -More support for new filter-API.
 *
 * v0.2 (2003-04-08) Tilmann Bitterberg
 *    -change include order to avoid warnings from Magick
 *
 * v0.3 (2003-04-24) Thomas Wehrspann <thomas@wehrspann.de>
 *    -Fixed bug with multiple instances
 *    -coordinates in RGB=YUV
 *    -Added SHAPE-mode
 *    -Documentation updated
 *
 * v0.4 (2003-09-03) Tilmann Bitterberg
 *    -add information for shape mode
 */

#define MOD_NAME    "filter_logoaway.so"
#define MOD_VERSION "v0.4 (2003-09-03)"
#define MOD_CAP     "remove an image from the video"
#define MOD_AUTHOR  "Thomas Wehrspann <thomas@wehrspann.de>"

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "transcode.h"
#include "framebuffer.h"
#include "filter.h"
#include "optstr.h"

#include <magick/api.h>

static vob_t *vob=NULL;

static char *modes[] = {"NONE", "SOLID", "XY", "SHAPE"};

typedef struct logoaway_data {
  unsigned int  start, end;
  int           xpos, ypos;
  int           width, height;
  int           mode;
  int           border;
  int           xweight, yweight;
  int           rcolor, gcolor, bcolor;
  int           ycolor, ucolor, vcolor;
  char          file[PATH_MAX];

  int           alpha;

  ExceptionInfo exception_info;
  Image         *image;
  ImageInfo     *image_info;
  PixelPacket   *pixel_packet;
} logoaway_data;

static logoaway_data *data[MAX_FILTER];


/*********************************************************
 * help text
 * this function prints out a small description
 * of this filter and the commandline options,
 * when the "help" option is given
 * @param   void      nothing
 * @return  void      nothing
 *********************************************************/
static void help_optstr(void)
{
   printf ("[%s] (%s) help                                                            \n", MOD_NAME, MOD_CAP);
   printf ("* Overview                                                                \n");
   printf ("    This filter removes an image in a user specified area from the video. \n");
   printf ("    You can choose from different methods.                                \n");
   printf ("                                                                          \n");
   printf ("* Options                                                                 \n");
   printf ("       'range' Frame Range      (0-oo)                        [0-end]     \n");
   printf ("         'pos' Position         (0-width x 0-height)          [0x0]       \n");
   printf ("        'size' Size             (0-width x 0-height)          [10x10]     \n");
   printf ("        'mode' Filter Mode      (0=none,1=solid,2=xy,3=shape) [0]         \n");
   printf ("      'border' Visible Border                                             \n");
   printf ("     'xweight' X-Y Weight       (0%%-100%%)                     [50]        \n");
   printf ("        'fill' Solid Fill Color (RRGGBB)                      [000000]    \n");
   printf ("        'file' Image with alpha/shape information             []          \n");
   printf ("                                                                          \n");
}


/*********************************************************
 * blend two pixel
 * this function blends two pixel with the given
 * weight
 * @param   srcPixel        source pixel value
 *          destPixel       source pixel value
 *          alpha           weight
 * @return  unsigned char   new pixel value
 *********************************************************/
unsigned char alpha_blending(unsigned char srcPixel, unsigned char destPixel, int alpha)
{
  return ( ( ( alpha * ( srcPixel - destPixel ) ) >> 8 ) + destPixel );
}


/*********************************************************
 * processing of the video frame (RGB codec)
 * processes the actual frame depending on the
 * selected mode
 * @param   buffer      video buffer
 *          width       video width
 *          height      video height
 *          instance    filter instance
 * @return  void        nothing
 *********************************************************/
void work_with_rgb_frame(char *buffer, int width, int height, int instance)
{
  int row, col, i;
  int xdistance, ydistance, distance_west, distance_north;
  unsigned char hcalc, vcalc;
  int buf_off, packet_off, buf_off_xpos, buf_off_width, buf_off_ypos, buf_off_height;
  int alpha_hori, alpha_vert;

  switch(data[instance]->mode) {

  case 0: // NONE

      break;

  case 1: // SOLID

      for(row=data[instance]->ypos; row<data[instance]->height; ++row) {
        for(col=data[instance]->xpos; col<data[instance]->width; ++col) {

          buf_off = ((height-row)*width+col) * 3;
          packet_off = (row-data[instance]->ypos) * (data[instance]->width-data[instance]->xpos) + (col-data[instance]->xpos);

          /* R */
          buffer[buf_off +0] = data[instance]->alpha?(alpha_blending( buffer[buf_off +0],  data[instance]->rcolor,  data[instance]->pixel_packet[packet_off].red >> 8 )):data[instance]->rcolor;

          /* G */
          buffer[buf_off +1] = data[instance]->alpha?(alpha_blending( buffer[buf_off +1],  data[instance]->gcolor,  data[instance]->pixel_packet[packet_off].green >> 8 )):data[instance]->gcolor;

          /* B */
          buffer[buf_off +2] = data[instance]->alpha?(alpha_blending( buffer[buf_off +2],  data[instance]->bcolor,  data[instance]->pixel_packet[packet_off].blue >> 8 )):data[instance]->bcolor;
        }
      }

      break;

  case 2: // XY

      xdistance = 256 / (data[instance]->width - data[instance]->xpos);
      ydistance = 256 / (data[instance]->height - data[instance]->ypos);
      for(row=data[instance]->ypos; row<data[instance]->height; ++row) {
        distance_north = data[instance]->height - row;

        alpha_vert = ydistance * distance_north;

        buf_off_xpos = ((height-row)*width+data[instance]->xpos) * 3;
        buf_off_width = ((height-row)*width+data[instance]->width) * 3;

        for(col=data[instance]->xpos; col<data[instance]->width; ++col) {
          distance_west  = data[instance]->width - col;

          alpha_hori = xdistance * distance_west;

          buf_off_ypos = ((height-data[instance]->ypos)*width+col) * 3;
          buf_off_height = ((height-data[instance]->height)*width+col) * 3;
          buf_off = ((height-row)*width+col) * 3;

          packet_off = (row-data[instance]->ypos) * (data[instance]->width-data[instance]->xpos) + (col-data[instance]->xpos);

          /* R */
          hcalc = alpha_blending( buffer[buf_off_xpos +0], buffer[buf_off_width  +0], alpha_hori );
          vcalc = alpha_blending( buffer[buf_off_ypos +0], buffer[buf_off_height +0], alpha_vert );
          buffer[buf_off +0] = data[instance]->alpha?alpha_blending( buffer[buf_off +0], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);

          /* G */
          hcalc = alpha_blending( buffer[buf_off_xpos +1], buffer[buf_off_width  +1], alpha_hori );
          vcalc = alpha_blending( buffer[buf_off_ypos +1], buffer[buf_off_height +1], alpha_vert );
          buffer[buf_off +1] = data[instance]->alpha?alpha_blending( buffer[buf_off +1], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);

          /* B */
          hcalc = alpha_blending( buffer[buf_off_xpos +2], buffer[buf_off_width  +2], alpha_hori );
          vcalc = alpha_blending( buffer[buf_off_ypos +2], buffer[buf_off_height +2], alpha_vert );
          buffer[buf_off +2] = data[instance]->alpha?alpha_blending( buffer[buf_off +2], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);
        }
      }

    break;

  case 3: // SHAPE

      xdistance = 256 / (data[instance]->width - data[instance]->xpos);
      ydistance = 256 / (data[instance]->height - data[instance]->ypos);
      for(row=data[instance]->ypos; row<data[instance]->height; ++row) {
        distance_north = data[instance]->height - row;

        alpha_vert = ydistance * distance_north;

        for(col=data[instance]->xpos; col<data[instance]->width; ++col) {
          distance_west  = data[instance]->width - col;

          alpha_hori = xdistance * distance_west;

          buf_off = ((height-row)*width+col) * 3;
          packet_off = (row-data[instance]->ypos) * (data[instance]->width-data[instance]->xpos) + (col-data[instance]->xpos);

          buf_off_xpos = ((height-row)*width+data[instance]->xpos) * 3;
          buf_off_width = ((height-row)*width+data[instance]->width) * 3;
          buf_off_ypos = ((height-data[instance]->ypos)*width+col) * 3;
          buf_off_height = ((height-data[instance]->height)*width+col) * 3;

          i=0;
          while( (data[instance]->pixel_packet[packet_off-i].red != 65535) && (col-i>data[instance]->xpos) ) i++;   // FIXME : ImageMagick MaxRGB=256 oder 65536
          buf_off_xpos   = ((height-row)*width + col-i) * 3;
          i=0;
          while( (data[instance]->pixel_packet[packet_off+i].red != 65535) && (col+i<data[instance]->width) ) i++;
          buf_off_width  = ((height-row)*width + col+i) * 3;

          i=0;
          while( (data[instance]->pixel_packet[packet_off-i*(data[instance]->width-data[instance]->xpos)].red != 65535) && (row-i>data[instance]->ypos) ) i++;
          buf_off_ypos   = (height*width*3)-((row-i)*width - col) * 3;
          i=0;
          while( (data[instance]->pixel_packet[packet_off+i*(data[instance]->width-data[instance]->xpos)].red != 65535) && (row+i<data[instance]->height) ) i++;
          buf_off_height = (height*width*3)-((row+i)*width - col) * 3;

          /* R */
          hcalc = alpha_blending( buffer[buf_off_xpos +0], buffer[buf_off_width  +0], alpha_hori );
          vcalc = alpha_blending( buffer[buf_off_ypos +0], buffer[buf_off_height +0], alpha_vert );
          buffer[buf_off +0] = data[instance]->alpha?alpha_blending( buffer[buf_off +0], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);

          /* G */
          hcalc = alpha_blending( buffer[buf_off_xpos +1], buffer[buf_off_width  +1], alpha_hori );
          vcalc = alpha_blending( buffer[buf_off_ypos +1], buffer[buf_off_height +1], alpha_vert );
          buffer[buf_off +1] = data[instance]->alpha?alpha_blending( buffer[buf_off +1], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);

          /* B */
          hcalc = alpha_blending( buffer[buf_off_xpos +2], buffer[buf_off_width  +2], alpha_hori );
          vcalc = alpha_blending( buffer[buf_off_ypos +2], buffer[buf_off_height +2], alpha_vert );
          buffer[buf_off +2] = data[instance]->alpha?alpha_blending( buffer[buf_off +2], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);
        }
      }

      break;

  }

  if(data[instance]->border) { // BORDER
    for(row=data[instance]->ypos; row<data[instance]->height; ++row) {
      if((row == data[instance]->ypos) || (row==data[instance]->height-1)) {
        for(col=data[instance]->xpos*3; col<data[instance]->width*3; ++col) if(col&1) buffer[((height-row)*width*3+col)] = 255 & 0xff;
      }
      if(row&1) {
        buf_off = ((height-row)*width+data[instance]->xpos)*3;
        buffer[buf_off +0] = 255;
        buffer[buf_off +1] = 255;
        buffer[buf_off +2] = 255;
      }
      if(row&1) {
        buf_off = ((height-row)*width+data[instance]->width)*3;
        buffer[buf_off +0] = 255;
        buffer[buf_off +1] = 255;
        buffer[buf_off +2] = 255;
      }
    }
  }
}


/*********************************************************
 * processing of the video frame (YUV codec)
 * processes the actual frame depend on the
 * selected mode
 * @param   buffer      video buffer
 *          width       video width
 *          height      video height
 *          instance    filter instance
 * @return  void        nothing
 *********************************************************/
void work_with_yuv_frame(char *buffer, int width, int height, int instance)
{
  int row, col, i;
  int craddr, cbaddr;
  int xdistance, ydistance, distance_west, distance_north;
  unsigned char hcalc, vcalc;
  int buf_off, packet_off=0, buf_off_xpos, buf_off_width, buf_off_ypos, buf_off_height;
  int alpha_hori, alpha_vert;

  craddr = (width * height);
  cbaddr = (width * height) * 5 / 4;

  switch(data[instance]->mode) {

  case 0: // NONE

      break;

  case 1: // SOLID //FIXME : "R�nder" entfernen

      /* Y */
      for(row=data[instance]->ypos; row<data[instance]->height; ++row) {
        for(col=data[instance]->xpos; col<data[instance]->width; ++col) {

          buf_off = row*width+col;
          packet_off = (row-data[instance]->ypos) * (data[instance]->width-data[instance]->xpos) + (col-data[instance]->xpos);

          buffer[buf_off] = data[instance]->alpha?alpha_blending( buffer[buf_off], data[instance]->ycolor, data[instance]->pixel_packet[packet_off].red >> 8):data[instance]->ycolor;
        }
      }

      /* Cb, Cr */
      for(row=data[instance]->ypos/2+1; row<data[instance]->height/2; ++row) {
        for(col=data[instance]->xpos/2+1; col<data[instance]->width/2; ++col) {

          buf_off = row*width/2+col;
          packet_off = (row*2-data[instance]->ypos) * (data[instance]->width-data[instance]->xpos) + (col*2-data[instance]->xpos);

          buffer[craddr + buf_off] = data[instance]->alpha?alpha_blending( buffer[craddr + buf_off], data[instance]->ucolor, data[instance]->pixel_packet[packet_off].red >> 8):data[instance]->ucolor;
          buffer[cbaddr + buf_off] = data[instance]->alpha?alpha_blending( buffer[cbaddr + buf_off], data[instance]->vcolor, data[instance]->pixel_packet[packet_off].red >> 8):data[instance]->vcolor;
        }
      }

      break;

  case 2: // XY

      /* Y' */
      xdistance = 256 / (data[instance]->width - data[instance]->xpos);
      ydistance = 256 / (data[instance]->height - data[instance]->ypos);
      for(row=data[instance]->ypos; row<data[instance]->height; ++row) {
        distance_north = data[instance]->height - row;

        alpha_vert = ydistance * distance_north;

        buf_off_xpos = row*width+data[instance]->xpos;
        buf_off_width = row*width+data[instance]->width;

        for(col=data[instance]->xpos; col<data[instance]->width; ++col) {
          distance_west  = data[instance]->width - col;

          alpha_hori = xdistance * distance_west;

          buf_off = row*width+col;
          buf_off_ypos = data[instance]->ypos*width+col;
          buf_off_height = data[instance]->height*width+col;

          packet_off = (row-data[instance]->ypos) * (data[instance]->width-data[instance]->xpos) + (col-data[instance]->xpos);

          hcalc = alpha_blending( buffer[buf_off_xpos], buffer[buf_off_width],  alpha_hori );
          vcalc = alpha_blending( buffer[buf_off_ypos], buffer[buf_off_height], alpha_vert );
          buffer[buf_off] = data[instance]->alpha?alpha_blending( buffer[buf_off], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);
        }
      }

      /* Cb, Cr */
      xdistance = 512 / (data[instance]->width - data[instance]->xpos);
      ydistance = 512 / (data[instance]->height - data[instance]->ypos);
      for (row=data[instance]->ypos/2+1; row<data[instance]->height/2; ++row) {
        distance_north = data[instance]->height/2 - row;

        alpha_vert = ydistance * distance_north;

        buf_off_xpos = row*width/2+data[instance]->xpos/2;
        buf_off_width = row*width/2+data[instance]->width/2;

        for (col=data[instance]->xpos/2+1; col<data[instance]->width/2; ++col) {
          distance_west  = data[instance]->width/2 - col;

          alpha_hori = xdistance * distance_west;

          buf_off = row*width/2+col;
          buf_off_ypos = data[instance]->ypos/2*width/2+col;
          buf_off_height = data[instance]->height/2*width/2+col;

          packet_off = (row*2-data[instance]->ypos) * (data[instance]->width-data[instance]->xpos) + (col*2-data[instance]->xpos);

          hcalc = alpha_blending( buffer[craddr + buf_off_xpos], buffer[craddr + buf_off_width],  alpha_hori );
          vcalc = alpha_blending( buffer[craddr + buf_off_ypos], buffer[craddr + buf_off_height], alpha_vert );
          buffer[craddr + buf_off] = data[instance]->alpha?alpha_blending( buffer[craddr + buf_off], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);

          hcalc = alpha_blending( buffer[cbaddr + buf_off_xpos], buffer[cbaddr + buf_off_width],  alpha_hori );
          vcalc = alpha_blending( buffer[cbaddr + buf_off_ypos], buffer[cbaddr + buf_off_height], alpha_vert );
          buffer[cbaddr + buf_off] = data[instance]->alpha?alpha_blending( buffer[cbaddr + buf_off], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);
        }
      }

      break;

  case 3: // SHAPE

      xdistance = 256 / (data[instance]->width - data[instance]->xpos);
      ydistance = 256 / (data[instance]->height - data[instance]->ypos);
      for(row=data[instance]->ypos; row<data[instance]->height; ++row) {
        distance_north = data[instance]->height - row;

        alpha_vert = ydistance * distance_north;

        for(col=data[instance]->xpos; col<data[instance]->width; ++col) {
          distance_west  = data[instance]->width - col;

          alpha_hori = xdistance * distance_west;

          buf_off = (row*width+col);
          packet_off = (row-data[instance]->ypos) * (data[instance]->width-data[instance]->xpos) + (col-data[instance]->xpos);

          i=0;
          while( (data[instance]->pixel_packet[packet_off-i].red != 65535) && (col-i>data[instance]->xpos) ) i++;   // FIXME : ImageMagick MaxRGB=256 oder 65536
          buf_off_xpos   = (row*width + col-i);
          i=0;
          while( (data[instance]->pixel_packet[packet_off+i].red != 65535) && (col+i<data[instance]->width) ) i++;
          buf_off_width  = (row*width + col+i);

          i=0;
          while( (data[instance]->pixel_packet[packet_off-i*(data[instance]->width-data[instance]->xpos)].red != 65535) && (row-i>data[instance]->ypos) ) i++;
          buf_off_ypos   = ((row-i)*width + col);
          i=0;
          while( (data[instance]->pixel_packet[packet_off+i*(data[instance]->width-data[instance]->xpos)].red != 65535) && (row+i<data[instance]->height) ) i++;
          buf_off_height = ((row+i)*width + col);

          hcalc = alpha_blending( buffer[buf_off_xpos], buffer[buf_off_width],  alpha_hori );
          vcalc = alpha_blending( buffer[buf_off_ypos], buffer[buf_off_height], alpha_vert );
          buffer[buf_off] = data[instance]->alpha?alpha_blending( buffer[buf_off], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);
        }
      }

      /* Cb, Cr */
      xdistance = 512 / (data[instance]->width - data[instance]->xpos);
      ydistance = 512 / (data[instance]->height - data[instance]->ypos);
      for (row=data[instance]->ypos/2+1; row<data[instance]->height/2; ++row) {
        distance_north = data[instance]->height/2 - row;

        alpha_vert = ydistance * distance_north;

        for (col=data[instance]->xpos/2+1; col<data[instance]->width/2; ++col) {
          distance_west  = data[instance]->width/2 - col;

          alpha_hori = xdistance * distance_west;

          i=0;
          while( (data[instance]->pixel_packet[packet_off-i].red != 65535) && (col-i>data[instance]->xpos) ) i++;   // FIXME : ImageMagick MaxRGB=256 oder 65536
          buf_off_xpos   = (row*width/2 + col-i);
          i=0;
          while( (data[instance]->pixel_packet[packet_off+i].red != 65535) && (col+i<data[instance]->width) ) i++;
          buf_off_width  = (row*width/2 + col+i);

          i=0;
          while( (data[instance]->pixel_packet[packet_off-i*(data[instance]->width-data[instance]->xpos)].red != 65535) && (row-i>data[instance]->ypos) ) i++;
          buf_off_ypos   = ((row-i)*width/2 + col);
          i=0;
          while( (data[instance]->pixel_packet[packet_off+i*(data[instance]->width-data[instance]->xpos)].red != 65535) && (row+i<data[instance]->height) ) i++;
          buf_off_height = ((row+i)*width/2 + col);

          buf_off = row*width/2+col;
          buf_off_ypos = data[instance]->ypos/2*width/2+col;
          buf_off_height = data[instance]->height/2*width/2+col;

          packet_off = (row*2-data[instance]->ypos) * (data[instance]->width-data[instance]->xpos) + (col*2-data[instance]->xpos);

          hcalc = alpha_blending( buffer[craddr + buf_off_xpos], buffer[craddr + buf_off_width],  alpha_hori );
          vcalc = alpha_blending( buffer[craddr + buf_off_ypos], buffer[craddr + buf_off_height], alpha_vert );
          buffer[craddr + buf_off] = data[instance]->alpha?alpha_blending( buffer[craddr + buf_off], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);

          hcalc = alpha_blending( buffer[cbaddr + buf_off_xpos], buffer[cbaddr + buf_off_width],  alpha_hori );
          vcalc = alpha_blending( buffer[cbaddr + buf_off_ypos], buffer[cbaddr + buf_off_height], alpha_vert );
          buffer[cbaddr + buf_off] = data[instance]->alpha?alpha_blending( buffer[cbaddr + buf_off], (hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100, data[instance]->pixel_packet[packet_off].red >> 8):((hcalc*data[instance]->xweight + vcalc*data[instance]->yweight)/100);
        }
      }

      break;
          buf_off_xpos = (row*width+data[instance]->xpos);
          buf_off_width = (row*width+data[instance]->width);
          buf_off_ypos = (data[instance]->ypos*width+col);
          buf_off_height = (data[instance]->height*width+col);

  }

  if(data[instance]->border) { // BORDER
    for(row=data[instance]->ypos; row<data[instance]->height; ++row) {
      if((row == data[instance]->ypos) || (row==data[instance]->height-1)) {
        for(col=data[instance]->xpos; col<data[instance]->width; ++col) if(col&1) buffer[row*width+col] = 255 & 0xff;
      }
      if(row&1) buffer[row*width+data[instance]->xpos] = 255 & 0xff;
      if(row&1) buffer[row*width+data[instance]->width] = 255 & 0xff;
    }
  }

}


/*-------------------------------------------------
 *
 * main
 *
 *-------------------------------------------------*/


int tc_filter(vframe_list_t *ptr, char *options)
{

  int instance=ptr->filter_id;


  //----------------------------------
  //
  // filter get config
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_GET_CONFIG && options) {

    char buf[255];
    optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYOM", "1");

    sprintf (buf, "%u-%u", data[instance]->start, data[instance]->end);
    optstr_param (options, "range",  "Frame Range",                         "%d-%d",     buf, "0", "oo",    "0", "oo");

    sprintf (buf, "%dx%d", data[instance]->xpos, data[instance]->ypos);
    optstr_param (options, "pos",    "Position of logo",                    "%dx%d",     buf, "0", "width", "0", "height");

    sprintf (buf, "%dx%d", data[instance]->width, data[instance]->height);
    optstr_param (options, "size",   "Size of logo",                        "%dx%d",     buf, "0", "width", "0", "height");

    sprintf (buf, "%d", data[instance]->mode);
    optstr_param (options, "mode",   "Filter Mode (0=none,1=solid,2=xy,3=shape)", "%d",  buf, "0", "3");

    sprintf (buf, "%d",  data[instance]->border);
    optstr_param (options, "border", "Visible Border",                      "",          buf);

    sprintf (buf, "%d", data[instance]->xweight);
    optstr_param (options, "xweight","X-Y Weight(0%-100%)",                 "%d",        buf, "0", "100");

    sprintf (buf, "%x%x%x", data[instance]->rcolor, data[instance]->gcolor, data[instance]->bcolor);
    optstr_param (options, "fill",   "Solid Fill Color(RGB)",               "%2x%2x%2x", buf, "00", "FF",   "00", "FF", "00", "FF");

    sprintf (buf, "%s",  data[instance]->file);
    optstr_param (options, "file",   "Image with alpha/shape information",  "%s",        buf);

    return 0;
  }


  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);

    if((data[instance] = (logoaway_data *)malloc (sizeof(logoaway_data))) == NULL) return (-1);

    data[instance]->start    = 0;
    data[instance]->end      = (unsigned int)-1;
    data[instance]->xpos     = 0;
    data[instance]->ypos     = 0;
    data[instance]->width    = 10;
    data[instance]->height   = 10;
    data[instance]->mode     = 0;
    data[instance]->border   = 0;
    data[instance]->xweight  = 50;
    data[instance]->yweight  = 50;
    data[instance]->rcolor   = 0;
    data[instance]->gcolor   = 0;
    data[instance]->bcolor   = 0;
    data[instance]->ycolor   = 16;
    data[instance]->ucolor   = 128;
    data[instance]->vcolor   = 128;
    data[instance]->alpha    = 0;

    // filter init ok.

    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

    if(options!=NULL) {
      optstr_get     (options,  "range",   "%d-%d",     &data[instance]->start,  &data[instance]->end);
      optstr_get     (options,  "pos",     "%dx%d",     &data[instance]->xpos,   &data[instance]->ypos);
      optstr_get     (options,  "size",    "%dx%d",     &data[instance]->width,  &data[instance]->height);
        data[instance]->width += data[instance]->xpos; data[instance]->height += data[instance]->ypos;
      optstr_get     (options,  "mode",    "%d",        &data[instance]->mode);
      if (optstr_get (options,  "border",  "") >= 0)
        data[instance]->border = 1;
      if (optstr_get (options,  "help",    "") >= 0)
        help_optstr();
      optstr_get     (options,  "xweight", "%d",        &data[instance]->xweight);
        data[instance]->yweight = 100 - data[instance]->xweight;
      optstr_get     (options,  "fill",    "%2x%2x%2x", &data[instance]->rcolor, &data[instance]->gcolor, &data[instance]->bcolor);
        data[instance]->ycolor =  (0.257 * data[instance]->rcolor) + (0.504 * data[instance]->gcolor) + (0.098 * data[instance]->bcolor) + 16;
        data[instance]->ucolor =  (0.439 * data[instance]->rcolor) - (0.368 * data[instance]->gcolor) - (0.071 * data[instance]->bcolor) + 128;
        data[instance]->vcolor = -(0.148 * data[instance]->rcolor) - (0.291 * data[instance]->gcolor) + (0.439 * data[instance]->bcolor) + 128;
      if (optstr_get (options,  "file",    "%[^:]",     &data[instance]->file) >= 0)
        data[instance]->alpha = 1;
    }

    if(verbose) printf("[%s] instance(%d) options=%s\n", MOD_NAME, instance, options);
    if(verbose > 1) {
      printf (" LogoAway Filter Settings: \n");
      printf ("            pos = %dx%d    \n", data[instance]->xpos, data[instance]->ypos);
      printf ("           size = %dx%d    \n", data[instance]->width-data[instance]->xpos, data[instance]->height-data[instance]->ypos);
      printf ("           mode = %d(%s)   \n", data[instance]->mode, modes[data[instance]->mode]);
      printf ("         border = %d       \n", data[instance]->border);
      printf ("     x-y weight = %d:%d    \n", data[instance]->xweight, data[instance]->yweight);
      printf ("     fill color = %2X%2X%2X\n", data[instance]->rcolor, data[instance]->gcolor, data[instance]->bcolor);
      printf ("           file = %s       \n", data[instance]->file);
    }

    if( (data[instance]->xpos > vob->ex_v_width) || (data[instance]->ypos > vob->ex_v_height) ) {
      fprintf(stderr, "[%s] ERROR invalid position\n", MOD_NAME);
      return(-1);
    }
    if( (data[instance]->width > vob->ex_v_width) || (data[instance]->height > vob->ex_v_height) ) {
      fprintf(stderr, "[%s] ERROR invalid size\n", MOD_NAME);
      return(-1);
    }
    if( (data[instance]->xweight > 100) || (data[instance]->xweight < 0) ) {
      fprintf(stderr, "[%s] ERROR invalid x weight\n", MOD_NAME);
      return(-1);
    }
    if( (data[instance]->mode < 0) || (data[instance]->mode > 3) ) {
      fprintf(stderr, "[%s] ERROR invalid mode\n", MOD_NAME);
      return(-1);
    }
    if( (data[instance]->mode == 3) && (data[instance]->alpha == 0) ) {
      fprintf(stderr, "[%s] ERROR alpha/shape file needed for SHAPE-mode\n", MOD_NAME);
      return(-1);
    }

    if(data[instance]->alpha) {
      InitializeMagick("");
      GetExceptionInfo(&data[instance]->exception_info);
      data[instance]->image_info = CloneImageInfo((ImageInfo *) NULL);

      strcpy(data[instance]->image_info->filename, data[instance]->file);
      data[instance]->image = ReadImage(data[instance]->image_info, &data[instance]->exception_info);
      if (data[instance]->image == (Image *) NULL) {
        fprintf(stderr, "[%s] ERROR: ", MOD_NAME);
        MagickWarning (data[instance]->exception_info.severity, data[instance]->exception_info.reason, data[instance]->exception_info.description);
        return(-1);
      }

      if ((data[instance]->image->columns != (data[instance]->width-data[instance]->xpos)) || (data[instance]->image->rows != (data[instance]->height-data[instance]->ypos))) {
        fprintf(stderr, "[%s] ERROR \"%s\" has incorrect size\n", MOD_NAME, data[instance]->file);

        return(-1);
      }

      data[instance]->pixel_packet = GetImagePixels(data[instance]->image, 0, 0, data[instance]->image->columns, data[instance]->image->rows);
    }

    return(0);
  }


  //----------------------------------
  //
  // filter close
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_CLOSE) {

    if (data[instance]->image) {
      DestroyImage(data[instance]->image);
      DestroyImageInfo(data[instance]->image_info);
    }
    DestroyMagick();

    if(data[instance]) free(data[instance]);
    data[instance] = NULL;

    return(0);
  }


  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------


  if(ptr->tag & TC_POST_PROCESS && ptr->tag & TC_VIDEO && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {

    if (ptr->id < data[instance]->start || ptr->id > data[instance]->end) return (0);

    if(vob->im_v_codec==CODEC_RGB) {
      work_with_rgb_frame(ptr->video_buf, vob->ex_v_width, vob->ex_v_height, instance);
    } else {
      work_with_yuv_frame(ptr->video_buf, vob->ex_v_width, vob->ex_v_height, instance);
    }
  }
  return(0);
}
