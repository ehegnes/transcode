/*
 *  filter_subtitler.c
 *
 *  Copyright (C) Jan Panteltje  2001
 *
 *  This file is part of transcode, a linux video stream processing tool
 *  Font reading etc from Linux mplayer
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

#include "subtitler.h"
//#include "../../src/framebuffer.h"

#define MOD_NAME    "filter_subtitler.so"
#define MOD_VERSION "v0.4 (2002-02-19)"
#define MOD_CAP     "subtitle filter"

/* for YUV to RGB in X11 */
#define LIMIT(x) ((((x)>0xffffff)?0xff0000:(((x)<=0xffff)?0:(x)&0xff0000))>>16)
int write_ppm_flag;

int dcontrast, brightness;
double dsaturation;
double dhue, dhue_line_drift;
int u_shift, v_shift;
int slice_level;

int add_objects_flag;
int time_base_correct_flag;
int help_flag;
int de_stripe_flag;

int movie_id;

/*
subtitle 'filter',
it adds objects as described in a file in .ppml format,
*/
int tc_filter(vframe_list_t *pfl, char *options)
{
int a, c, i, j, xpo, xsh, yy, x;
float fa;
double da, db, dc;
int pre = 0;
int vid = 0;
static FILE *pppm_file;
static FILE *fptr;
char temp[4096];
static int frame_nr;
char *cptr;
static uint8_t *pfm, *opfm, *pfmend, *opfmend;
static uint8_t *pptr, *opptr, *vptr, *mptr;
static uint8_t *pline_start, *pline_end, *opline_start, *opline_end;
static int sum_of_differences, min_sum_of_differences;
static int x_shift, average_x_shift;
static int stripe, stripe_line;
static int start_time, start_time_average, time_correction;
char *running;
char *token;
static int y, u, v, y1, r, g, b;
uint8_t *py, *pu, *pv;
static int cr, cg, cb, cy, cu, cv;
static int have_bottom_margin_flag;
//aframe_list_t *afl;

/*
API explanation:
(1) need more infos, than get pointer to transcode global information
	structure vob_t as defined in transcode.h.
(2) 'tc_get_vob' and 'verbose' are exported by transcode.
(3) filter is called first time with TC_FILTER_INIT flag set.
(4) make sure to exit immediately if context (video/audio) or 
	placement of call (pre/post) is not compatible with the filters 
	intended purpose, since the filter is called 4 times per frame.
(5) see framebuffer.h for a complete list of frame_list_t variables.
(6) filter is last time with TC_FILTER_CLOSE flag set
*/

/* filter init */
if(pfl->tag & TC_FILTER_INIT)
	{
	vob = tc_get_vob();
	if(! vob)
		{
		printf("subtitler(): could not tc_get_vob() failed\n");

		return -1;
		}
	if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

	/* identify */
    printf(\
	"\nPanteltje (c) movie composer%s (alias subtitle-filter)\n\n",\
	SUBTITLER_VERSION);

	/* get home directory */  
	userinfo = getpwuid(getuid() );
	home_dir = strsave(userinfo -> pw_dir);
	user_name = strsave(userinfo -> pw_name);

	/* set some defaults */

	/* for subtitles */

	/* a frame in transcode is 40 ms */
	frame_offset = 0;

	/* this selects some other symbols it seems */
	default_font = 0;	// 1 = strange symbols like stars etc..

	/* this sets the font outline */
	default_font_factor = 10.75;	// outline, was .75

	/* this sets the horizontal space for the subtitles */
	subtitle_h_factor = SUBTITLE_H_FACTOR;

	/* this sets how far the subtitles start from the bottom */
	subtitle_v_factor = SUBTITLE_V_FACTOR;

	/* location where font.descr is */
	sprintf(temp, "%s/.subtitles/font", home_dir);
	default_font_dir = strsave(temp);
	if(! default_font_dir)
		{
		printf("subtitler(): could not allocate space for default_font_dir\n");

		return -1; 
		}

	/*
	the mplayer font seems to overlap getween 'l' and 't', added some
	extra space between characters
	*/
	extra_character_space = EXTRA_CHAR_SPACE;

	/* the ppml file */
	sprintf(temp, "%s/.subtitles/demo.ppml", home_dir);
	subtitle_file = strsave(temp);
	if(! subtitle_file)
		{
		printf(\
		"subtitler(): could not allocate space for subtitle_file\n");

		return -1; 
		}

	/* for picture adjust */
	brightness = 0;			// steps
	dcontrast = 100.0;		// percent
	dsaturation = 100.0;	// percent
	u_shift = 0;			// steps
	v_shift = 0;			// steps

	/* for color correction */
	dhue = 0.0;
	dhue_line_drift = 0.0;	// The rotation in degrees at the start and end
							// of a line.
							// This is for correcting color errors in NTSC
							// tapes.
							// Use in combination with dhue in color
							// correction.


	/* for show output in X11 */ 
	window_open_flag = 0;
	color_depth = 0; /* get from X */

	/* module settings */
	add_objects_flag = 1;
	de_stripe_flag = 0;
	time_base_correct_flag = 0;
	write_ppm_flag = 0;
	show_output_flag = 0;
	center_flag = 1;

	/* uses when we call transcode recursive, to run a movie in a movie */
	movie_id = 0;

	/* for chroma key, better do this not each pixel .. */
	dmax_vector = sqrt( (127.0 * 127.0) + (127.0 * 127.0) );

	/*
	for rotate and shear, the level we cut the remaining parts.
	Note that yuv_to_ppm() also uses this, to write a modified .ppm
	that does NOT have this luminance in it, for use by mogrify.
	This ONLY happens if rotate or shear present.
	*/
	default_border_luminance = LUMINANCE_MASK;
	
	/* end defaults */
	if(debug_flag)
		{
		printf("subtitler(): INIT options=%s\n", options);
		}

	if(temp[0] != 0)
		{
		running = strsave(options);
		if(! running)
			{
			printf("subtitler(): strsave(options) failed\n");

			return -1;
			}
		while(1)
			{
			token = strsep (&running, " ");
			if(token == NULL) break;

			/* avoid empty string */
			if(token[0] == 0) continue;
			
			if(strncmp(token, "no_objects", 10) == 0)
				{
				add_objects_flag = 0;
				}
			else if(strncmp(token, "write_ppm", 9) == 0)
				{
 				write_ppm_flag = 1;
				}
			else if(strncmp(token, "debug", 5) == 0)
				{
				debug_flag = 1;
				}	
			else if(strncmp(token, "help", 4) == 0)
				{
				help_flag = 1;
				print_options();

				/* error exit */
				exit(1);
				}	
			 else if(strncmp(token, "subtitle_file=", 14) == 0)
				{
				a = sscanf(token, "subtitle_file=%s", temp);
				if(a == 1)
					{
					free(subtitle_file);
					subtitle_file = strsave(temp);
					if(! subtitle_file)
						{
						printf(\
			"subtitler(): could not allocate space for subtitle_file\n");

						return -1;
						}
					}
				}
			else if(strncmp(token, "font_dir=", 9) == 0)
				{
				a = sscanf(token, "font_dir=%s", temp);
				if(a == 1)
					{
					free(default_font_dir);
					default_font_dir = strsave(temp);
					if(! default_font_dir)
						{
						printf(\
			"subtitler(): could not allocate space for default_font_dir\n");

						return -1;
						}
					}
				}
			sscanf(token, "color_depth=%d", &color_depth);
			sscanf(token, "font=%d", &default_font);
			sscanf(token, "font_factor=%f", &default_font_factor);
			sscanf(token, "frame_offset=%d", &frame_offset);
			sscanf(token, "movie_id=%d", &movie_id);
			} /* end while parse options */

		free(running);
		} /* end if options */

	if(debug_flag)
		{		
		printf("subtitler(): PARSER RESULT\n\
		write_ppm_flag=%d add_objects_flag=%d show_output_flag=%d\n\
		color_depth=%d frame_offset=%d movie_id=%d\n",\
		write_ppm_flag, add_objects_flag, show_output_flag,\
		color_depth, frame_offset, movie_id\
		);
		}

	if(add_objects_flag)
		{
		/* read in font (also needed for frame counter) */
		sprintf(temp, "%s/font.desc", default_font_dir);
		vo_font =\
		read_font_desc(temp, default_font_factor, 0);
		if(! vo_font)
			{
			printf("subtitler(): Could not load font\n");

			/* return init error */
			return -1;
			}

		subtitle_current_font_descriptor = vo_font;

		/* load ppml file */
		if(! load_ppml_file(subtitle_file) )
			{
			printf("subtitler(): could not load file %s\n",\
			subtitle_file);

			/* return init error */
			return -1;
			}
		} /* end if add_objects_flag */

	/* return init OK */
	return 0;
	} /* end if filter init */

/* filter close */
if(pfl->tag & TC_FILTER_CLOSE)
	{
	/* rely on exit() */

	/* return close OK */
	return 0;
	} /* end if filter close */
  
/*
filter frame routine
tag variable indicates, if we are called before
transcodes internal video/audo frame processing routines
or after and determines video/audio context
*/
if(verbose & TC_STATS)
	{
	printf("[%s] %s/%s %s %s\n",\
	MOD_NAME, vob->mod_path, MOD_NAME, MOD_VERSION, MOD_CAP);
    
	/*
	tag variable indicates, if we are called before
	transcodes internal video/audo frame processing routines
	or after and determines video/audio context
   	*/
 
	if(pfl->tag & TC_PRE_PROCESS) pre = 1;
	if(pfl->tag & TC_POST_PROCESS) pre = 0;
    
	if(pfl->tag & TC_VIDEO) vid = 1;
	if(pfl->tag & TC_AUDIO) vid = 0;
    
	printf("[%s] frame [%06d] %s %16s call\n",\
	MOD_NAME, pfl->id, (vid)?"(video)":"(audio)",\
	(pre)?"pre-process filter":"post-process filter");
	} /* end if verbose and stats */
  
//if( (pfl->tag & TC_POST_PROCESS) && (pfl->tag & TC_AUDIO) )
//	{
//	printf(\
//	"WAS afl->audio_size=%d afl->audio_buf=%lu\n",\
//	afl -> audio_size, afl -> audio_buf);		

//	for(i = 0; i < 16; i++)
//		{
//		printf("%02x ", afl -> audio_buf[i]);
//		}
//	printf("\n");

//	}

/* add the subtitles, after the coding, else edges in text get bad */
if( (pfl->tag & TC_POST_PROCESS) && (pfl->tag & TC_VIDEO) )
	{
	ImageData = pfl->video_buf;
	image_width = pfl->v_width;
	image_height = pfl->v_height;
	frame_nr = pfl->id;	
	if(! have_bottom_margin_flag)
		{
		window_bottom = image_height - window_bottom;
		have_bottom_margin_flag = 1;
		}

	if(debug_flag)
		{
		printf(\
		"frame_nr=%d\n\
		ImageData=%lu image_width=%d image_height=%d\n",\
		frame_nr,\
		ImageData, image_width, image_height);
		}

	/*
	calculate where to put and how to reformat the subtitles.
	These are globals.
	*/
	line_h_start = subtitle_h_factor * image_width;
	line_h_end = image_width - line_h_start;
	window_bottom = image_height - (subtitle_v_factor * image_height);

//printf("WAS PROC h_factor=%.2f v_factor=%.2f\n",\
//subtitle_h_factor, subtitle_v_factor);

	if(de_stripe_flag)
		{
		/*
		create a place to save the current frame,
		going to use it next frame to replace the lines that are all white,
		as caused by severe dropouts in ancient Umatic tapes.
		NOTE!:
		This cannot be done in INIT as then pfl->v_width and pfl->v_height
		are not available yet (zero).
		*/
		if(! frame_memory0)
			{
			/* for RGB */
			frame_memory0 = malloc(pfl->v_width * pfl->v_height * 3);
			if(! frame_memory0)
				{
				printf("de_striper(): could not malloc frame_memory0\n");

				/* return error */
				return -1;
				}
			frame_memory1 = malloc(pfl->v_width * pfl->v_height * 3);
			if(! frame_memory1)
				{
				printf("de_striper(): could not malloc frame_memory1\n");

				/* return error */
				return -1;
				}
			} /* end if ! frame_memory */

		/* save the current frame for later */
		for(i = 0; i < pfl->v_width * pfl->v_height * 3; i++)
			{
			frame_memory0[i] = pfl->video_buf[i];			
			}

		slice_level = 0;
		pfm = pfl->video_buf;
		opfm = frame_memory1;	
		pfmend = ImageData + (pfl->v_height * pfl->v_width * 3);
		opfmend = frame_memory1 + (pfl->v_height * pfl->v_width * 3);
		for(y = 0; y < pfl->v_height; y++)
			{
			/* get line boundaries for video buffer */
			pline_start = pfm;
			pline_end = pfm + pfl->v_width * 3;

			/* get line boundaries for frame_memory1 */
			opline_start = opfm;
			opline_end = opfm + pfl->v_width * 3;
			x_shift = 0;
			/*
			perhaps expand condition for more then one pixel in a line
			*/
			for(x = 0; x < pfl->v_width; x++)
				{
				if(pfm >= pfmend - 3) break;

				/* test if white stripe */ 
				if( (pfm[0] - opfm[0] > slice_level) &&\
				(pfm[1] - opfm[1] > slice_level) &&\
				(pfm[2] - opfm[2] > slice_level) )
					{
					//printf("STRIPE\n");

					/* test for out of range pointers due to x_shift */
					if( (opfm + x_shift >= (uint8_t *)frame_memory1) &&\
					(opfm + x_shift < opfmend) ) 					
						{
						/* replace with data from previous frame */
						pfm[0] = *(opfm + x_shift); 
						pfm[1] = *(opfm + 1 + x_shift);
						pfm[2] = *(opfm + 2 + x_shift);
						} /* end if in range */
					} /* end if white stripe */

				pfm += 3;
				opfm += 3;

				} /* end for all x */

			if(time_base_correct_flag)
				{
				time_base_corrector(y, pfm, pfl->v_width, pfl->v_height);
				}

			if(pfm >= pfmend - 3) break;

			} /* end for all y */

		/* save the current frame for later */
		for(i = 0; i < pfl->v_width * pfl->v_height * 3; i++)
			{
			frame_memory1[i] = frame_memory0[i];			
			}

		} /* end if de_stripe_flag */

	if\
	(\
	(dcontrast != 100.0) ||\
	(dsaturation != 100.0) ||\
	(u_shift) ||\
	(v_shift)\
	)
		{
		/*
		brightness, contrast, saturation, U zero shift, V zero shift.
		*/
		ucptrs = ImageData;
		/* set pointers */
	    py = ImageData;
	    pv = ImageData + image_width * image_height;
	    pu = ImageData + (image_width * image_height * 5) / 4;

		if(vob->im_v_codec == CODEC_RGB)
			{		
			for(y = 0; y < pfl->v_height; y++)
				{
				for(x = 0; x < pfl->v_width * 3; x++)
					{
					/* brightness */
					if( (brightness + *py) > 255) *py = 255;
					else if ( (brightness + *py) < 0) *py = 0;
					else *py += brightness;

					/* contrast */
					da = *py;
					da *= dcontrast / 100.0;
					*py = (int)da;

					} /* end for all x */

				} /* end for y */
			} /* end if color_depth 32 */
		else if(vob->im_v_codec == CODEC_YUV)
			{
			for(y = 0; y < pfl->v_height; y++)
				{
				for(x = 0; x < pfl->v_width; x++)
					{
					/* brightness */
					if( (brightness + *py) > 255) *py = 255;
					else if ( (brightness + *py) < 0) *py = 0;
					else *py += brightness;

					/* contrast */
					da = *py;
					da *= dcontrast / 100.0;
					*py = (int)da;

					/* saturation */
					a = (int)*pu - 128;
					b = (int)*pv - 128;

					a *= dsaturation / 100.0;
					b *= dsaturation / 100.0;

					*pu = (uint8_t)a + 128;
					*pv = (uint8_t)b + 128;
						
					/* u_shift */
					*pu += u_shift;
					*pu &= 0xff;

					/* v_shift */
					*pv += v_shift;
					*pv &= 255;
			
					/* increment Y pointer */
					py++;

					/* increment U and V vector pointers */
					if(x % 2)
						{
						pu++;
						pv++;
						}
					} /* end for all x */

				if( (y + 1) % 2)
					{
					pu -= pfl->v_width / 2;
					pv -= pfl->v_width / 2;
					}

				} /* end for y */
			} /* end if buffer is YUV */
		} /* end if contrast, saturation, u_shift, v_shift */

	if( dhue || dhue_line_drift)
		{
		/*
		UV vector rotation.
		Dynamic UV vector rotation (NTSC line phase error correction).
		*/
		if(vob->im_v_codec == CODEC_RGB)
			{		
			printf(\
			"subtitler(): hue operations only available in YUV 420\n\
			please use -V option in transcode\n");

			exit(1);
			} /* end if CODEC_RGB */
		else if(vob->im_v_codec == CODEC_YUV)
			{
			/* set pointers */
		    py = ImageData;
		    pv = ImageData + image_width * image_height;
		    pu = ImageData + (image_width * image_height * 5) / 4;

			for(y = 0; y < pfl->v_height; y++)
				{
				for(x = 0; x < pfl->v_width; x++)
					{
					/*
					NTSC color correction at start and end of line
					Assuming middle to be correct, most users would have
					adjusted on face color somewhere in the middle.
					*/

					/* the phase drift over one horizontal line */
					da = (double)x / (double)pfl->v_width; // 0 to 1
				
					/* go for middle, now -.5 to +.5 */
					da -= .5;
						
					/* multiply by specified dynamic correction factor */
					db = dhue_line_drift * da;

					/* add the static hue correction specified */
					db += (double)dhue;

					/* hue and saturation*/
					a = (int)*pu - 128;
					b = (int)*pv - 128;
					adjust_color(&a, &b, db, dsaturation);
					*pu = (uint8_t)a + 128;
					*pv = (uint8_t)b + 128;
						
					/* increment Y pointer */
					py++;

					/* increment U and V vector pointers */
					if(x % 2)
						{
						pu++;
						pv++;
						}
					} /* end for all x */
					
				/* 
				2 x 2 color pixels on screen for each Y value,
				repeat each line twice.					

				Orientation on screen Y (*) and U V (o)
				* o
				o o 
				drop shadow :-) color less area below char looks better.
				sink a line.
				*/
				if( (y + 1) % 2)
					{
					pu -= pfl->v_width / 2;
					pv -= pfl->v_width / 2;
					}

				} /* end for y */
			} /* end if buffer is YUV */

		} /* end if some sort of hue */

	if(add_objects_flag)
		{
		/*
		collect any objects from database for this frame
		and add to object list.
		*/
		process_frame_number(frame_nr);

		/* add objects in object list to display, and update params */
		add_objects(frame_nr);

		} /* end if add_objects_flag */

	if(write_ppm_flag)
		{
		if(vob->im_v_codec == CODEC_RGB)
			{		
			printf(\
			"subtitler(): write_ppm only available in YUV 420\n\
			please use -V option in transcode\n");

			exit(1);
			} /* end if CODEC_RGB */
		else if(vob->im_v_codec == CODEC_YUV)
			{
			/* set pointers */
		    py = ImageData;
		    pv = ImageData + image_width * image_height;
		    pu = ImageData + (image_width * image_height * 5) / 4;

			/* open the ppm file for write */
			sprintf(temp, "%s/.subtitles/%d.ppm", home_dir, movie_id);
			pppm_file = fopen(temp, "w");
			if(! pppm_file)
				{
				printf(\
				"subtitler(): could not open file %s for write, aborting\n",\
				 temp);

				exit(1);
				} 

			/* write the ppm header */
			fprintf(pppm_file,\
			"P6\n%i %i\n255\n", pfl->v_width, pfl->v_height);

			for(y = 0; y < pfl->v_height; y++)
				{
				/* get a line from buffer start, to file in RGB */
				for(x = 0; x < pfl->v_width; x++)
					{
					cy = ( (0xff & *py) - 16);
					cy  *= 76310;

					cu = (0xff & *pu) - 128;
					cv = (0xff & *pv) - 128;

					cr = 104635 * cv;
					cg = -25690 * cu + -53294 * cv;
					cb = 132278 * cu;

					fprintf(pppm_file, "%c%c%c",\
					LIMIT(cr + cy), LIMIT(cg + cy), LIMIT(cb + cy) );  

					/* increment Y pointer */
					py++;

					/* increment U and V vector pointers */
					if(x % 2)
						{
						pu++;
						pv++;
						}
					} /* end for all x */
					
				if( (y + 1) % 2)
					{
					pu -= pfl->v_width / 2;
					pv -= pfl->v_width / 2;
					}

				} /* end for y (all lines) */
			} /* end if buffer is YUV */
		fclose(pppm_file);

		/* set the semaphore indicating the .ppm file is ready */
		sprintf(temp, "touch %s/.subtitles/%d.sem", home_dir, movie_id);
		execute(temp);

		/* now wait for the semaphore to be removed, by calling */
		sprintf(temp, "%s/.subtitles/%d.sem", home_dir, movie_id);
		while(1)
			{
			fptr = fopen(temp, "r");
			if(! fptr) break;
				
			fclose(fptr);

			/* reduce processor load */
			usleep(10000); // 10 ms
			} /* end while wait for handshake */

		} /* end if write_ppm_flag */

	if(show_output_flag)
		{
		/* create an xwindows display */
		if(! window_open_flag)
			{
			if(debug_flag)
				{
				printf("subtitler(): opening window\n");
				}

//			openwin(argc, argv, width, height);
			openwin(0, NULL, pfl->v_width, pfl->v_height);
	
			window_size = pfl->v_width * pfl->v_height;
			window_open_flag = 1;
	
			if(color_depth == 0) color_depth = get_x11_bpp();

			} /* end if ! window_open_flag */
		else /* have window */
			{
			if( (pfl->v_width * pfl->v_height) != window_size)
				{
				/* close window and open a new one */
//				closewin(); //crashes
//				resize_window(xsize, ysize); // problem ?
// no problem, now we have 2 windows, use window manager to kill one

//				openwin(argc, argv, xsize, ysize);
				openwin(0, NULL, pfl->v_width, pfl->v_height);

				window_size = pfl->v_width * pfl->v_height;
				} /* end if different window size */

			/* get X11 buffer */
			ucptrd = (unsigned char *)getbuf();

			/* copy data to X11 buffer */
			ucptrs = ImageData;

			if(vob->im_v_codec == CODEC_RGB)
				{		
				/* need vertical flip, but not horizontal flip */
				if(color_depth == 32)
					{
					/* ucptrs points to start buffer, ucptrd to X buffer */
					ucptrd += (window_size - pfl->v_width) * 4;
					for(y = 0; y < pfl->v_height; y++)
						{
						/*
						get a line from buffer start, copy to xbuffer end
						*/
						for(x = 0; x < pfl->v_width; x++)
							{
							*ucptrd++ = *ucptrs++;
							*ucptrd++ = *ucptrs++;
							*ucptrd++ = *ucptrs++;

							ucptrd++; /* nothing in byte 4 */
							}

						/* move back a line, so we V flip */
						ucptrd -= pfl->v_width * 8;
						} /* end for y (all lines) */
					} /* end if color_depth 32 */
				else if(color_depth == 24) // NOT TESTED!!!!!!!!
					{
					/* ucptrs points to start buffer, ucptrd to X buffer */
					ucptrd += (window_size - pfl->v_width) * 3;
					for(y = 0; y < pfl->v_height; y++)
						{
						/*
						get a line from buffer start, copy to xbuffer end
						*/
						for(x = 0; x < pfl->v_width; x++)
							{
							*ucptrd++ = *ucptrs++;
							*ucptrd++ = *ucptrs++;
							*ucptrd++ = *ucptrs++;
							}

						/* move back a line, so we V flip */
						ucptrd -= pfl->v_width * 6;
						} /* end for y (all lines) */
					} /* end if color_depth 32 */
				} /* end if buffer is RGB */	
			else if(vob->im_v_codec == CODEC_YUV)
				{
				/* set pointers */
			    py = ImageData;
			    pv = ImageData + image_width * image_height;
			    pu = ImageData + (image_width * image_height * 5) / 4;
				/* ucptrd is pointer to xbuffer BGR */

				for(y = 0; y < pfl->v_height; y++)
					{
					/* get a line from buffer start, copy to xbuffer BGR */
					for(x = 0; x < pfl->v_width; x++)
						{
						cy = ( (0xff & *py) - 16);
						cy  *= 76310;

						cu = (0xff & *pu) - 128;
						cv = (0xff & *pv) - 128;

						cr = 104635 * cv;
						cg = -25690 * cu + -53294 * cv;
						cb = 132278 * cu;

						if(color_depth == 32) // 4 bytes per pixel							
							{
							*ucptrd++ = LIMIT(cb + cy); // B
							*ucptrd++ = LIMIT(cg + cy); // G
							*ucptrd++ = LIMIT(cr + cy); // R

							/* one more byte */
							*ucptrd++ = 0; // last byte is empty.
							} /* end if color depth 32 */

						/* 24 bpp not tested */
						else if(color_depth == 24) // 3 bytes per pixel
							{
							*ucptrd++ = LIMIT(cb + cy); // B
							*ucptrd++ = LIMIT(cg + cy); // G
							*ucptrd++ = LIMIT(cr + cy); // R
							}
						
						/* increment Y pointer */
						py++;

						/* increment U and V vector pointers */
						if(x % 2)
							{
							pu++;
							pv++;
							}
						} /* end for all x */
					
					/* 
					2 x 2 color pixels on screen for each Y value,
					repeat each line twice.					

					Orientation on screen Y (*) and U V (o)
					* o
					o o 
					drop shadow :-) color less area below char looks better.
					sink a line.
					*/
					if( (y + 1) % 2)
						{
						pu -= pfl->v_width / 2;
						pv -= pfl->v_width / 2;
 						}

					} /* end for y (all lines) */
				} /* end if buffer is YUV */

			/* show X11 buffer */
			putimage(pfl->v_width, pfl->v_height);
			} /* end if window_open_flag */

		} /* end if show_output_flag */

	} /* end if TC_VIDEO && TC_POST_PROCESS */

/* return OK */
return 0;
} /* end function tc_filter */ 


int add_text(\
int x, int y,\
char *text, int u, int v,\
double contrast, double transparency, font_desc_t *pfd, int espace)
{
int a;
signed char *ptr;
char temp[4096];

if(debug_flag)
	{
	printf("subtitler(): add_text(): x=%d y=%d text=%s\n\
	u=%d v=%d contrast=%.2f transparency=%.2f\n\
	font_desc_t=%lu espace=%d\n",\
	x, y, text, u, v, contrast, transparency, pfd, espace);
	} 

ptr = text;
while(*ptr)
	{
	/* convert to signed */
	a = *ptr;
	if(*ptr < 0) a += 256;

	if(a == ' ')
		{
		}
	else
		{
		print_char(x, y, a, u, v, contrast, transparency, pfd);
		}

	x += pfd->width[a] + pfd->charspace;

	x += espace; //extra_character_space;

	ptr++;
	}

return 1;
} /* end function add_text */


int test_char_set(int frame)
{
int a, b, i, j, x, y;
char temp[1024];
int pos;
char t[2];

if(debug_flag)
	{
	printf("subtitler(): test_char_set(): arg frame=%d\n", frame);
	} 

a = 128;
y = 100;
for(i = 0; i < 16; i++)
	{
	if(a > 256) return 1;

	sprintf(temp, "pos=%d", a);
	add_text(0, y, temp, 0, 0, 0.0, 0.0, NULL, extra_character_space);

	x = 200;
	for(j = 0; j < 16; j++)
		{
		print_char(x, y, a, 0, 0, 0.0, 0.0, NULL);

		x += vo_font->width[a] + vo_font->charspace;

		x += extra_character_space;

		a++;
		} /* end for j */

	y += 33;
	} /* end for i */

return 1;
} /* end function test_char_set */


int print_char(\
int x, int y, int c,\
int u, int v,\
double contrast, double transparency, font_desc_t *pfd)
{
if(debug_flag)
	{
	printf("subtiter(): print_char(): arg\n\
	x=%d y=%d c=%d u=%d v=%d contrast=%.2f transparency=%.2f\n\
	pfd=%lu",\
	x, y, c, u, v, contrast, transparency, pfd);
	}

draw_alpha(\
	x,\
	y,\
	pfd->width[c],\
	pfd->pic_a[default_font]->h,\
	pfd->pic_b[default_font]->bmp + pfd->start[c],\
	pfd->pic_a[default_font]->bmp + pfd->start[c],\
	pfd->pic_a[default_font]->w,\
	u, v, contrast, transparency);

return 1;
} /* end function print_char */


void draw_alpha(\
	int x0, int y0,\
	int w, int h,\
	uint8_t *src, uint8_t *srca, int stride,\
	int u, int v, double contrast, double transparency)
{
int a, b, c, x, y, sx, cd;
char *ptr;
uint8_t *py, *pu, *pv;
uint8_t *ps, *psa;
uint8_t *sc, *sa;
double dc, dm, di, dci;
uint8_t uy, ua, uc;
int mu, mv, iu, iv;

if(debug_flag)
	{
	printf(\
	"subtitler(): draw_alpha(): x0=%d y0=%d w=%d h=%d\n\
	src=%lu srca=%lu stride=%d u=%d v=%d\n\
	contrast=%.2f transparency=%.2f\n",\
	x0, y0, w, h,\
	src, srca, stride, u, v,\
	contrast, transparency);

	printf("vob->im_v_codec=%d\n", vob -> im_v_codec);
	printf("image_width=%d image_height=%d\n", image_width, image_height);
	printf("ImageData=%lu\n", ImageData);
	}

/* calculate multiplier for transparency ouside loops */
dm = transparency / 100.0; // main, 1 for 100 % transparent
di = 1.0 - dm; // insert (text here) 0 for 100% transparent

/*
do not multiply color (saturation) with contrast,
saturation could be done in adjust color, but done here for speed
*/
dci = di;

di *= (contrast / 100.0); // adjust contrast insert (text)

if(vob->im_v_codec == CODEC_RGB)
	{
//	printf(\
//	"subtitler ONLY works with YUV 420, please use -V option in transcode\n");

//	exit(1);

	draw_alpha_rgb24(\
		w, h, src, srca, stride,\
		ImageData + 3 * (y0 * image_width + x0),\
		3 * image_width);
	} /* end if RGB */
else if(vob->im_v_codec == CODEC_YUV)
	{
	/* 
	We seem to be in this format I420:
    y = dest;
    v = dest + width * height;
    u = dest + width * height * 5 / 4;

	Orientation of Y (*) relative to chroma U and V (o)
	* o
	o o
	So, an array of 2x2 chroma pixels exists for each luminance pixel
	The consequence of this is that there will be a color-less area
	of one line on the right and on the bottom of each character.
	Dropshadow :-)
	*/

	py = ImageData;
	pv = ImageData + image_width * image_height;
	pu = ImageData + (image_width * image_height * 5) / 4;

	sc = src;
	sa = srca;

	a = y0 * image_width;
	b = image_width / 4;
	c = image_width / 2;

	py += x0 + a;
	a /= 4;

	pu += (x0 / 2) + a;
	pv += (x0 / 2) + a;

	/* on odd lines, need to go a quarter of a 'line' back */
	if(y0 % 2)
		{
		pu -= b;
		pv -= b;				
		}

	for(y = 0; y < h; y++)
		{
		for(x = 0; x < w; x++)
			{
			/* clip right scroll */
			if( (x + x0) > image_width) continue;

			/* clip left scroll */
			if( (x + x0 ) < 0) continue;

			/* clip top scroll */
			if( (y + y0) > image_height) continue;
	
			/* clip bottom scroll */
			if( (y + y0) < 0) continue;

			if(sa[x])
				{
				/* some temp vars */
				uy = py[x];
				ua = sa[x];
				uc = sc[x];

				/* get decision factor before we change anything */
				cd = ( (py[x] * sa[x]) >> 8) < 5;

				/* calculate value insert (character) */
				uy = ( (uy * ua) >> 8) + uc;

				/* attenuate insert (character) the opposite way */
				uy *= di; // di is 0 for 100 % transparent

				/* attenuate main */
				py[x] *= dm; // dm is 1 for 100% transp
							
				/* add what is left of the insert (character) */
				py[x] += uy;
				
				sx = 1;
				if( (x + x0) % 2) sx = 0; 

				/* trailing shadow no */
//				if(x  < (w - 4) ) sx = 1; // hack, looks better :-)
//				else sx = 0;

				if(cd)
					{
					/* some temp vars, integer so we can multiply */
					mu = pu[x / 2 + sx] - 128;
					mv = pv[x / 2 + sx] - 128;

					/* adjust main color saturation */
					mu *= dm;
					mv *= dm;

					/* adjust insert (character) color saturation */
					iu = u * dci;
					iv = v * dci;

					if(sc[x]) /* white part of char */
						{
						/* set U vector */
						pu[x / 2 + sx] = 128 + mu + iu;

						/* set V vector */
						pv[x / 2 + sx] = 128 + mv + iv;
						}
					else /* shadow around char, no color */
						{
						/* set U vector */
						pu[ (x / 2) + sx] = 128 + mu;

						/* set V vector */
						pv[ (x / 2) + sx] = 128 + mv;
						}
					} /* end if cd */
				} /* end if sa[x] */
			} /* end for all x */

		sc += stride;
		sa += stride;

		py += image_width;

		if( (y + y0) % 2)
			{
			pu += c;
			pv += c;
			}

		} /* end for all y */

	} /* end if YUV */
} /* end function draw_alpha */


void draw_alpha_rgb24(\
int w, int h,\
unsigned char* src, unsigned char *srca, int srcstride,\
unsigned char* dstbase, int dststride)
{
int y;

if(debug_flag)
	{
	printf("subtitler(): draw_alpha_rgb24():\n");
	}

for(y = 0; y < h; y++)
	{
	register unsigned char *dst = dstbase;
	register int x;

	/* clip top */
//	if(y > image_height) continue;

	for(x = 0; x < w; x++)
		{
		/* clip right side */
//		if(x > image_width) continue;

#ifdef USE_COLOR
		if(srca[x])
			{
			/* BGR */
			dst[[0] = ((dst[0] * fg_blue) >> 8) + src[x];
			dst[[1] = ((dst[1] * fg_green) >> 8) + src[x];
			dst[[2] = ((dst[2] * fg_red) >> 8) + src[x];
			}
		else
			{
			dst[[0] = ((dst[0] * bg_blue) >> 8) + src[x];
			dst[[1] = ((dst[1] * bg_green) >> 8) + src[x];
			dst[[2] = ((dst[2] * bg_red) >> 8) + src[x];
			}
#else
		if(srca[x])
			{
#ifdef FAST_OSD
			dst[0] = dst[1] = dst[2] = src[x];
#else
			dst[0] = ((dst[0] * srca[x]) >> 8) + src[x];
			dst[1] = ((dst[1] * srca[x]) >> 8) + src[x];
			dst[2] = ((dst[2] * srca[x]) >> 8) + src[x];
#endif /* ! FAST_OSD */
			}
#endif /* else ! USE_COLOR */

		dst += 3; // 24bpp
		} /* end for all x */

	src += srcstride;
	srca += srcstride;
	dstbase -= dststride;
	} /* end for all y */

return;
} /* end function draw_alpha_rgb24 */


int time_base_corrector(int y, uint8_t *pfm, int hsize, int vsize)
{
/*
corects timing errors in head switching Umatic
This assumes a small black lead space, darker then the picture itself in
each line.
*/
int a, b, c, i;
int shift;
int shift_start;
int pic_hstart;
static double dstart_sum, dstart_average = 0.0;
uint8_t *ppa, *ppb, *ppc, *ppd;
int black_level;
int pic_h_reference;
static int count;
	
#define BLACK_LEVEL				10
#define CORRECTION_RANGE		100
#define PRE_HEAD_SWITCH_LINE	20

/* BGR */

//for(i = 0; i < hsize - 3; i += 3)
//	{
//  /* color lines before head switch red */
//	if(y > PRE_HEAD_SWITCH_LINE) pfm[i + 2] = 255;
//	}
//return 1;

black_level =\
pfm[0] + pfm[1] + pfm[2] +\
pfm[3] + pfm[4] + pfm[5] +\
pfm[6] + pfm[7] + pfm[8] +\
pfm[9] + pfm[10] + pfm[11] +\
pfm[12] + pfm[13] + pfm[14];
black_level /= 12;

/* find start picture (end left margin) */
for(i = 0; i < CORRECTION_RANGE; i += 3)
	{
	if( (pfm[i] >  black_level + 3) &&\
	(pfm[i + 1] >  black_level + 3) &&\
	(pfm[i + 2] >  black_level + 3) )
		{
		pic_hstart = i;
		break;
		} /* end if start picture found (end left margin) */
	} /* end for i */

//printf("WAS black_level=%d y=%d pic_hstart=%d\n",\
//black_level, y, pic_hstart);
//return 1;

/* average start for first lines */ 
if(y > PRE_HEAD_SWITCH_LINE)
	{
	dstart_sum += pic_hstart;
	count++;
	dstart_average = dstart_sum / count;
	}

else if(y == PRE_HEAD_SWITCH_LINE)
	{
//printf("WAS dstart_sum=%.2f dstart_average=%.2f\n",\
//dstart_sum, dstart_average);

	/* this is the reference start (left margin) */
	pic_h_reference = (int) dstart_average;

	printf("time_base_corrector(): pic_h_reference=%d\n", pic_h_reference);

	} /* end if last test line */

/* only correct bottom where head switching is */
else if(y < PRE_HEAD_SWITCH_LINE)
	{
	/* id start > pic_h_reference, to far to the right, move left shift */
//	shift = pic_h_reference - pic_hstart;

pic_h_reference = 30;

	shift = pic_hstart - pic_h_reference;	

	printf("time_base_corrector(): y=%d shift=%d\n", y, shift);

	shift = abs(shift);
//return 1;

	if(pic_hstart < pic_h_reference)
		{
//printf("WAS copy down y=%d shift=%d\n", y, shift);
		/* move left, copy down */
		for(i = 0; i < hsize - shift - 3; i += 3)
			{
			pfm[i] = pfm[i + shift];
			pfm[i + 1] = pfm[i + 1 + shift];
			pfm[i + 2] = pfm[i + 2 + shift];
			} /* end for i */
		} /* end if move left */
	else /* move right */
		{
//printf("WAS copy up y=%d shift=%d\n", y, shift);
		/* copy up */
		for(i = hsize - shift - 3; i > 0; i -= 3)
			{
			pfm[i + 2 + shift] = pfm[i + 2];		
			pfm[i + 1 + shift] = pfm[i + 1];		
			pfm[i + shift] = pfm[i];		
			} /* end for i */
		} /* end if move right */
	} /* end if lower part of picture */

return 1;
} /* end function time_base_corrector */


int print_options()
{
if(debug_flag)
	{
	printf("subtitler(): print options(): arg none\n");
	}
/*
From transcode -0.5.1 ChangeLog:
Example: -J my_filter="fonts=3 position=55 -v"
*/

printf("subtitler():\n");
printf(\
"Usage -J subtitler=%c[no_objects] [subtitle_file=s]\n\
[color_depth=n]\n\
[font_dir=s] [font=n] [font_factor=f\n\
[frame_offset=n]\n\
[debug] [help]%c\n", '"', '"');
printf("f is float, h is hex, n is integer, s is string.\n\n");

printf(\
"no_objects           disables subtitles and other objects (off).\n\
color_depth=         32 or 24 (overrides X auto) (32).\n\
font=                0 or 1, 1 gives strange symbols... (0).\n\
font_dir=            place where font.desc is (%s).\n\
font_factor=         .1 to 100 outline characters (10.75).\n\
frame_offset=        positive (text later) or negative (earlier) integer (0).\n\
subtitle_file=       pathfilename.ppml location of ppml file (%s).\n\
debug                prints debug messages (off).\n\
help                 prints this list and exit.\n",\
default_font_dir, subtitle_file);

printf("\n");
return 1;
} /* end function print_options */


add_picture(struct object *pa)
{
/*
reads yuyv in pa -> data into the YUV 420 ImageData buffer.
*/
uint8_t *py, *pu, *pv;
int a, b, c, d, x, y;
char *ps;
char ca;
int u_time;
int in_range;
double da, dc, dd, dm, ds;
int ck_flag;
int odd_line;

if(debug_flag)
	{
	printf("subtitler(): add_picture(): arg pa=%lu\n\
	pa->xsize=%.2f pa->ysize=%.2f pa->ck_color=%.2f\n",\
	pa,\
	pa -> xsize, pa -> ysize,\
	pa -> chroma_key_color);
	}

/* argument check */
if(! ImageData) return 0;
if(! pa) return 0;
if( (int)pa -> xsize == 0) return 1;
if( (int)pa -> ysize == 0) return 1;

/* calculate multiplier for transparency ouside loops */
dm = (100.0 - pa -> transparency) / 100.0;
dd = 1.0 - dm;

dc = dm * (pa -> contrast / 100.0);
ds = (pa -> saturation / 100.0);

/* saturation could be done in adjust color, but done here for speed */
//ds = 1.0;

if(vob->im_v_codec == CODEC_RGB)
	{
	/* ImageData, image_width, image_height */

	printf(\
	"subtitler ONLY works with YUV 420, please use -V option in transcode\n");

	exit(1);
	} /* end if RGB */
else if(vob->im_v_codec == CODEC_YUV)
	{
	b = image_width / 4;
	c = image_width / 2;

	py = ImageData;
	pu = ImageData + (image_width * image_height * 5) / 4;
	pv = ImageData + (image_width * image_height);
	
	a = (int)pa -> ypos * image_width;
	py += (int)pa -> xpos + a;
	a /= 4;
	pu += ( (int)pa -> xpos / 2) + a;
	pv += ( (int)pa -> xpos / 2) + a;

	ps = pa -> data;

	if( (int)pa -> ypos % 2 )
		{
		pu -= b;
		pv -= b;
		}

	// reading sequence is YUYV, so U is first.
	u_time = 1;
	for(y = 0; y < (int)pa -> ysize; y++)
		{
		odd_line = (y + (int)pa -> ypos) % 2;

		for(x = 0; x < (int)pa -> xsize; x++)
			{
			/* find out if OK to display */
			in_range = 1;
			/* clip right scroll */
			if( (x + (int)pa -> xpos) > image_width) in_range = 0;

			/* clip left scroll */
			if( (x + (int)pa -> xpos ) < 0) in_range = 0;

			/* clip top scroll */
			if( (y + (int)pa -> ypos) > image_height) in_range = 0;
	
			/* clip bottom scroll */
			if( (y + (int)pa -> ypos) < 0) in_range = 0;

			/* slice level */
			a = *ps;
			if(a < 0) a += 256;
			if( a < ( (int)pa -> slice_level) ) in_range = 0;

			if(\
			(pa -> zrotation != 0) ||\
			(pa -> xshear != 0) || (pa -> yshear != 0)\
			)
				{
				/*
				for rotate and shear, the luminance value of the border
				to cut away.
				Since this would remove picture data, for this not to
				happen, we add 1 step to the luminance if it happens to
				be the same as border_luminanc in yuv_to_ppm().
				With this trick it is guaranteed border_luminance never happens
				in the .ppm file that mogrify processes.
				*/
				if(pa -> mask_level)
					{
					if(a == pa -> mask_level) in_range = 0;
					}
				else
					{
					if(a == default_border_luminance) in_range = 0;
					}
				} /* end if rotate or shear */

			/* test for chroma key match if color specified */
			if(pa -> chroma_key_saturation)
				{
				if(u_time)
					{
					if(! odd_line)
						{
						a = (int)pu[x / 2] - 128;
						b = (int)pv[x / 2] - 128;
						ck_flag =\
						chroma_key(\
						a, b,\
						pa -> chroma_key_color,\
						pa -> chroma_key_window,\
						pa -> chroma_key_saturation);
						} /* end if even line */
					else
						{
						a = (int)pu[(x / 2) + c] - 128;
						b = (int)pv[(x / 2) + c] - 128;
						ck_flag =\
						chroma_key(\
						a, b,\
						pa -> chroma_key_color,\
						pa -> chroma_key_window,\
						pa -> chroma_key_saturation);
						} /* end if odd line */
					} /* end if u_time */	

				/* transport to next time here ! */
				if(! ck_flag) in_range = 0;
				} /* end if chroma key */

			if(in_range)
				{
				py[x] *= dd;
				py[x] += dc * (uint8_t)*ps;
				} /* end if in_range */

			ps++;

			if(in_range)
				{
				if(u_time)
					{
					ca = *ps;
					ca = 128 + ( ( (uint8_t)*ps - 128 ) * ds);	
					
					pu[x / 2] *= dd;
					pu[x / 2] += dm * (uint8_t)ca; 
					}
				else
					{
					ca = *ps;
					ca = 128 + ( ( (uint8_t)*ps - 128 ) * ds);

					pv[x / 2] *= dd;
					pv[x / 2] += dm * (uint8_t)ca; 
					}

				/* apply hue correction if both U and V set */

//				if(! u_time)
					{
					if(pa -> hue)
						{
						/*
						hue,
						saturation done outside adjust_color() for speed
						*/

						a = (int)pu[x / 2] - 128;
						b = (int)pv[x / 2] - 128;
						adjust_color(&a, &b, pa -> hue, 100.0);
						pu[x / 2] = (uint8_t)a + 128;
						pv[x / 2] = (uint8_t)b + 128;

						} /* end if hue */

					} /* end if ! u_time */
				} /* end if in range */

			ps++;
			u_time = 1 - u_time;

			} /* end for all x */

		if( (int) pa -> xsize % 2) u_time = 1 - u_time;

		py += image_width;

		if(odd_line)
			{
			pu += c;
			pv += c;
			}

		} /* end for all y */

	} /* end if YUV 420 */

return 1;
}/* end function add_picture */ 


int set_main_movie_properties(struct object *pa)
{
if(debug_flag)
	{
	printf("set_main_movie_properties(): arg pa=%lu\n", pa);
	}

if(! pa) return 0;

dcontrast = pa -> contrast;
brightness = (int)pa -> brightness;
dsaturation = pa -> saturation;
dhue = pa -> hue;
dhue_line_drift = pa -> hue_line_drift;
u_shift = (int)pa -> u_shift;
v_shift = (int)pa -> v_shift;
de_stripe_flag = (int)pa -> de_stripe;
time_base_correct_flag = (int)pa -> time_base_correct;
show_output_flag = (int)pa -> show_output;

return 1;
} /* end function set_main_movie_properties */

