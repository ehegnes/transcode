/*
 *  tcxpm2rgb.c
 *
 *  Copyright (C) Tilmann Bitterberg - September 2003
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
# include "config.h"
#else
# define PACKAGE "transcode"
# define VERSION "unknown"
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "transcode.h"

/* ParseColor and QueryColorDatabase are from ImageMagick */

static char *ParseColor(char *data)
{
#define NumberTargets  6

  static const char
    *targets[NumberTargets] = { "c ", "g ", "g4 ", "m ", "b ", "s " };

  register char
    *p,
    *r;

  register const char
    *q;

  register int
    i;

  for (i=0; i < NumberTargets; i++)
  {
    r=data;
    for (q=targets[i]; *r != '\0'; r++)
    {
      if (*r != *q)
        continue;
      if (!isspace((int) (*(r-1))))
        continue;
      p=r;
      for ( ; ; )
      {
        if (*q == '\0')
          return(r);
        if (*p++ != *q++)
          break;
      }
      q=targets[i];
    }
  }
  return((char *) NULL);
}

typedef struct color_t {
    int red, green, blue, opacity;
} color_t;


#define TransparentOpacity 255
#define OpaqueOpacity        0
#define BackgroundColor   "#ff"

static unsigned int QueryColorDatabase(const char *name,
  color_t *color)
{
  int
    blue,
    green,
    opacity,
    red;

  register long
    i;

  /*
    Initialize color return value.
  */
  memset(color,0,sizeof(color_t));
  color->opacity=TransparentOpacity;
  if ((name == (char *) NULL) || (*name == '\0'))
    name=BackgroundColor;
  while (isspace((int) (*name)))
    name++;
  if (*name == '#')
    {
      char
        c;

      long
        n;

      green=0;
      blue=0;
      opacity=(-1);
      name++;
      for (n=0; isxdigit((int) name[n]); n++);
      if ((n == 3) || (n == 6) || (n == 9) || (n == 12))
        {
          /*
            Parse RGB specification.
          */
          n/=3;
          do
          {
            red=green;
            green=blue;
            blue=0;
            for (i=n-1; i >= 0; i--)
            {
              c=(*name++);
              blue<<=4;
              if ((c >= '0') && (c <= '9'))
                blue|=c-'0';
              else
                if ((c >= 'A') && (c <= 'F'))
                  blue|=c-('A'-10);
                else
                  if ((c >= 'a') && (c <= 'f'))
                    blue|=c-('a'-10);
                  else
                    return(0);
            }
          } while (isxdigit((int) *name));
        }
      else
        if ((n != 4) && (n != 8) && (n != 16))
          return(0);
        else
          {
            /*
              Parse RGBA specification.
            */
            n/=4;
            do
            {
              red=green;
              green=blue;
              blue=opacity;
              opacity=0;
              for (i=n-1; i >= 0; i--)
              {
                c=(*name++);
                opacity<<=4;
                if ((c >= '0') && (c <= '9'))
                  opacity|=c-'0';
                else
                  if ((c >= 'A') && (c <= 'F'))
                    opacity|=c-('A'-10);
                  else
                    if ((c >= 'a') && (c <= 'f'))
                      opacity|=c-('a'-10);
                    else
                      return(0);
              }
            } while (isxdigit((int) *name));
          }
      n<<=1;
      color->red=red/(1<<n);
      color->green=green/(1<<n);
      color->blue=blue/(1<<n);
      color->opacity=OpaqueOpacity;
      if (opacity >= 0) color->opacity=opacity>>((1 << n));
      return(1);
    }
  return(1);
}

#define EXE "tcxpm2rgb"
#define MAX_BUF     1024

void version(char *exe)
{
    // print id string to stderr
    fprintf(stderr, "%s (%s v%s) (C) 2003 Tilmann Bitterberg\n", exe, PACKAGE, VERSION);
}


void usage(int status)
{
  version(EXE);
   
  fprintf(stderr,"\n%s converts a XPM file to rgb24 format\n", EXE);
  fprintf(stderr,"Usage: %s [options]\n", EXE);

  fprintf(stderr,"\t-i name          input file name [stdin]\n");
  fprintf(stderr,"\t-o name          output file name [stdout]\n");
  fprintf(stderr,"\t-v               print version\n");

  exit(status);
  
}


int main (int argc, char *argv[])
{ 
    char linebuf[MAX_BUF], target[MAX_BUF], key[16];
    FILE *f, *o;
    int width, height, colors, bwidth, n, j, linelen, x, y, ch;
    char **clist, **keys; 
    char *p, *q, *line;
    char *out = NULL, *d;
    char *outfile = NULL, *infile=NULL;
    color_t *colormap;
    long sret;

    while ((ch = getopt(argc, argv, "i:o:vh?")) != -1) {
	
      switch (ch) {
	
      case 'i': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	infile = optarg;
      
	break;

      case 'o': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	outfile = optarg;
      
	break;

      case 'v':
	version(EXE);
	exit(EXIT_SUCCESS);
	break;
      case '?':
      case 'h':
      default:
	usage(EXIT_SUCCESS);
	break;

      }
    }

    if (infile) {
	f = fopen(infile, "r");
	if (!f) {
	    perror ("fopen infile");
	    return 1;
	}
    } else 
	f = stdin;

    if (outfile) {
	o = fopen(outfile, "w");
	if (!o) {
	    perror ("fopen outfile");
	    return 1;
	}
    } else 
	o = stdout;


    if (!fgets(linebuf, MAX_BUF, f)) {
	fprintf(stderr, "No more lines\n");
	return 1;
    }

    if (strncmp ("/* XPM */", linebuf, 9) != 0) {
	fprintf(stderr, "Not an xpm file 1 (%s)\n", linebuf);
	return 1;
    }

    if (!fgets(linebuf, MAX_BUF, f)) {
	fprintf(stderr, "No more lines\n");
	return 1;
    }
    if (strncmp ("static char", linebuf, 11) != 0) {
	fprintf(stderr, "Not an xpm file 2\n");
	return 1;
    }

    fgets(linebuf, MAX_BUF, f);
    n = sscanf(linebuf+1, "%d %d %d %d", &width, &height, &colors, &bwidth);
    if (n != 4 || (bwidth > 2) || (width == 0) || (height == 0) || (colors == 0)) {
	fprintf(stderr, "Error reading header\n");
	return 1;
    }
    //fprintf(stderr, "XPM Image: %dx%d; %d colors, %d byte wide\n", width, height, colors, bwidth);

    clist = (char **)malloc(colors*sizeof(char *));

    if (clist == (char **)NULL) {
	fprintf(stderr, "Error malloc clist\n");
	return 1;
    }

    // color lookup table
    colormap = (color_t *)malloc(colors*sizeof(color_t));

    for (n=0; n<colors; n++) {
	int len;

	if (!fgets(linebuf, MAX_BUF, f)) {
	    fprintf(stderr, "Error reading color table\n");
	    return 1;
	}
	len=strlen(linebuf);
	     
	clist[n] = (char *)malloc(len);
	memcpy(clist[n], linebuf+1, len-3);
	clist[n][len-4] = '\0';
	//printf("[%d] : %s|\n", n, clist[n]);
    }

    keys = (char **)malloc(colors*sizeof(char *));

    for (j=0; j<colors; j++) {
	p = clist[j];

	keys[j] = malloc(bwidth+1); // a bit stupid since bwidth is usually just 1 or 2
	keys[j][bwidth]='\0';

	sret = strlcpy(keys[j], p, bwidth+1);
	tc_test_string(__FILE__, __LINE__, bwidth+1, sret, errno);

	sret = strlcpy(target, "gray", sizeof(target));
	tc_test_string(__FILE__, __LINE__, sizeof(target), sret, errno);

	q = ParseColor (p+bwidth);
	if (q != (char *) NULL)
	{
	    while (!isspace((int) (*q)) && (*q != '\0'))
		q++;
	    (void) strncpy(target,q,sizeof(target)-1);
	    q=ParseColor(target);
	    if (q != (char *) NULL)
		*q='\0';
	}
	QueryColorDatabase(target, &colormap[j]);
    }

    if (j < colors) {
	fprintf(stderr, "Corrupt XPM image file");
	return 1;
    }
    memset (key, '\0', 16);

    linelen = width * bwidth + 16;
    line = malloc(linelen);

    out = malloc (width*height*3);
    if (!out || !line) {
	fprintf(stderr, "Error malloc line\n");
	return 1;
    }

    d = out;
    j=0;
    for (y = 0; y<height; y++) {
	int len;

	if (!fgets(line, linelen, f)) {
	    fprintf(stderr, "Error reading line %d\n", y);
	    return 1;
	}
	len = strlen(line);
	p = line+1;
	p[len-4]='\0';

	for (x = 0; x<width; x++) {

	    strncpy(key,p,bwidth);

	    // can anything be slower?
	    if (strcmp(key,keys[j]) != 0)
		for (j=0; j < colors; j++)
		    if (strcmp(key,keys[j]) == 0)
			break;

	    *d++ = colormap[j].red&0xff;
	    *d++ = colormap[j].green&0xff;
	    *d++ = colormap[j].blue&0xff;
	    p+=bwidth;

	}
    }

    if ( (n = fwrite (out, width*height*3, 1, o))<0) {
	fprintf(stderr, "fwrite failed (should = %d, have = %d)\n", width*height*3, n);
	return 1;
    }

    fclose(o);

    //fprintf(stderr, "Wrote %s\n", outfile);

    free(out);
    free(line);
    for (n=0; n<colors; n++) {
	free(clist[n]);
	free(keys[n]);
    }
    free(clist);
    free(keys);
    free(colormap);
    
    // read };
    if (!fgets(linebuf, MAX_BUF, f)) {
	perror("fgets header");
	return 1;
    }



    fclose (f);

    return 0;
}
