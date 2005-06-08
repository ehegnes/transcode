/*
    Copyright (C) 2004 Bryan Mayland <bmayland@leoninedev.com>
    For use in transcode by Tilmann Bitterberg <transcode@tibit.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define MOD_NAME    "filter_levels.so"
#define MOD_VERSION "v1.0.0 (2004-06-09)"
#define MOD_CAP     "Luminosity level scaler"
#define MOD_AUTHOR  "Bryan Mayland"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#include <math.h>

#define module "[" MOD_NAME "]: "

#define DEFAULT_IN_BLACK   0
#define DEFAULT_IN_WHITE   255
#define DEFAULT_IN_GAMMA   1.0
#define DEFAULT_OUT_BLACK  0
#define DEFAULT_OUT_WHITE  255
#define DEFAULT_PRE        0

typedef struct
{
    struct
	{
		int in_black;
		int in_white;
        float in_gamma;

		int out_black;
		int out_white;
	} parameter;

    unsigned char lumamap[256];
    int prefilter;
} levels_private_data_t;

static levels_private_data_t levels_private_data[MAX_FILTER];

static void doColorScale(levels_private_data_t *pd, unsigned char *data,
                        int width, int height)
{
    int y_size = width * height;
    unsigned char *map = pd->lumamap;
    while (y_size--)  {
        *data = map[*data];
        data++;
    }
}

void build_map(unsigned char *map, int inlow, int inhigh, float ingamma,
              int outlow, int outhigh)
{
   int i;
   float f;
   for (i=0; i<256; i++) {
       if (i <= inlow)
           map[i] = outlow;
       else if (i >= inhigh)
           map[i] = outhigh;
       else {
           f = (float)(i - inlow) / (inhigh - inlow);
           map[i] = pow(f, 1/ingamma) * (outhigh - outlow) + outlow;
       }
   }  /* for i 0-255 */
}

static void help_optstr()
{
    fprintf(stderr, "[%s] (%s) help\n", MOD_NAME, MOD_CAP);
    fprintf(stderr, "* Overview\n");
    fprintf(stderr, "  Scales luminosity values in the source image, similar to\n");
    fprintf(stderr, "  VirtualDub's 'levels' filter.  This is useful to scale ITU-R601\n");
    fprintf(stderr, "  video (which limits luma to 16-253) back to the full 0-255 range.\n");
    fprintf(stderr, "* Options\n");
    fprintf(stderr, "   input:   luma range of input (%d-%d)\n", DEFAULT_IN_BLACK, DEFAULT_IN_WHITE);
    fprintf(stderr, "   gamma:   gamma ramp to apply to input luma (%f)\n", DEFAULT_IN_GAMMA);
    fprintf(stderr, "   output:  luma range of output (%d-%d)\n", DEFAULT_OUT_BLACK, DEFAULT_OUT_WHITE);
    fprintf(stderr, "   pre:     act as pre processing filter (0)\n");
}

int tc_filter(vframe_list_t *vframe, char *options)
{
	levels_private_data_t *pd;
    int tag = vframe->tag;

	if(tag & TC_AUDIO)
		return(0);

    pd = &levels_private_data[vframe->filter_id];

	if(tag & TC_FILTER_GET_CONFIG)
	{
		char buf[64];
		optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VYMEO", "1");

		snprintf(buf, 64, "%d-%d", DEFAULT_IN_BLACK, DEFAULT_IN_WHITE );
		optstr_param(options, "input", "input luma range (black-white)", "%d-%d", buf, "0", "255", "0", "255" );

        snprintf(buf, 64, "%f", DEFAULT_IN_GAMMA );
        optstr_param(options, "gamma", "input luma gamma", "%f", buf, "0.5", "3.5" );

		snprintf(buf, 64, "%d-%d", DEFAULT_OUT_BLACK, DEFAULT_OUT_WHITE );
		optstr_param(options, "output", "output luma range (black-white)", "%d-%d", buf, "0", "255", "0", "255" );
        
		snprintf(buf, 64, "%i", DEFAULT_PRE );
	        optstr_param(options, "pre", "pre processing filter", "%i", buf, "0", "1" );
	}

	if(tag & TC_FILTER_INIT)
	{
        vob_t *vob;

		if(!(vob = tc_get_vob()))
			return(TC_IMPORT_ERROR);

		pd->parameter.in_black   = DEFAULT_IN_BLACK;
		pd->parameter.in_white   = DEFAULT_IN_WHITE;
		pd->parameter.in_gamma   = DEFAULT_IN_GAMMA;
		pd->parameter.out_black  = DEFAULT_OUT_BLACK;
		pd->parameter.out_white  = DEFAULT_OUT_WHITE;
		pd->prefilter = DEFAULT_PRE;

        if (options) {
            if(optstr_lookup(options, "help")) {
                help_optstr();
                return(TC_IMPORT_ERROR);
            }

            optstr_get(options, "input",   "%d-%d", &pd->parameter.in_black, &pd->parameter.in_white );
            optstr_get(options, "gamma",   "%f",    &pd->parameter.in_gamma );
            optstr_get(options, "output",  "%d-%d", &pd->parameter.out_black, &pd->parameter.out_white );
	    optstr_get(options, "pre",     "%d", &pd->prefilter);
        }  /* if options */

		if(vob->im_v_codec != CODEC_YUV)
		{
			fprintf(stderr, "[%s] This filter is only capable of YUV mode\n", MOD_NAME);
	  		return(TC_IMPORT_ERROR);
		}

        build_map(pd->lumamap, pd->parameter.in_black, pd->parameter.in_white, pd->parameter.in_gamma,
                 pd->parameter.out_black, pd->parameter.out_white);

		if(verbose) 
    		fprintf(stderr, "[%s]: %s %s #%d\n", MOD_NAME, MOD_VERSION, MOD_CAP, vframe->filter_id);
                fprintf(stderr, "[%s]: scaling %d-%d gamma %f to %d-%d\n", MOD_NAME,
                        pd->parameter.in_black, pd->parameter.in_white,
                        pd->parameter.in_gamma,
                        pd->parameter.out_black, pd->parameter.out_white
                       );
		fprintf(stderr, "[%s]: %s-processing filter\n", MOD_NAME, 
			(pd->prefilter) ?"pre" :"post");
	}  /* if INIT */

    if((((tag & TC_POST_PROCESS) && !pd->prefilter) && !(vframe->attributes & TC_FRAME_IS_SKIPPED)) ||
       (((tag & TC_PRE_PROCESS) && pd->prefilter) && !(vframe->attributes & TC_FRAME_IS_SKIPPED)))
        doColorScale(pd, vframe->video_buf, vframe->v_width, vframe->v_height);

	return(0);
}
