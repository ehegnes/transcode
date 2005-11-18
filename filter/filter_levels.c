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
#define MOD_VERSION "v1.0.1 (2005-11-18)"
#define MOD_CAP     "Luminosity level scaler"
#define MOD_AUTHOR  "Bryan Mayland"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#include <math.h>

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#define DEFAULT_IN_BLACK   0
#define DEFAULT_IN_WHITE   255
#define DEFAULT_IN_GAMMA   1.0
#define DEFAULT_OUT_BLACK  0
#define DEFAULT_OUT_WHITE  255
#define DEFAULT_PREFILTER  TC_FALSE

#define OPTION_BUF_SIZE    64
#define MAP_SIZE           256

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

    uint8_t lumamap[MAP_SIZE];
    int is_prefilter;
} LevelsPrivateData;

static LevelsPrivateData levels_private_data[MAX_FILTER];

static void build_map(uint8_t *map, int inlow, int inhigh,
                      float ingamma, int outlow, int outhigh)
{
   int i;
   float f;
   for (i = 0; i < MAP_SIZE; i++) {
       if (i <= inlow) {
           map[i] = outlow;
       } else if (i >= inhigh) {
           map[i] = outhigh;
       } else {
           f = (float)(i - inlow) / (inhigh - inlow);
           map[i] = pow(f, 1/ingamma) * (outhigh - outlow) + outlow;
       }
   }  /* for i 0-255 */
}

static void help_optstr(void)
{
    tc_log_info(MOD_NAME, "(%s) help", MOD_CAP);
    fprintf(stderr, "* Overview\n");
    fprintf(stderr, "  Scales luminosity values in the source image, similar to\n");
    fprintf(stderr, "  VirtualDub's 'levels' filter.  This is useful to scale ITU-R601\n");
    fprintf(stderr, "  video (which limits luma to 16-235) back to the full 0-255 range.\n");
    fprintf(stderr, "* Options\n");
    fprintf(stderr, "   input:   luma range of input (%d-%d)\n", DEFAULT_IN_BLACK, DEFAULT_IN_WHITE);
    fprintf(stderr, "   gamma:   gamma ramp to apply to input luma (%f)\n", DEFAULT_IN_GAMMA);
    fprintf(stderr, "   output:  luma range of output (%d-%d)\n", DEFAULT_OUT_BLACK, DEFAULT_OUT_WHITE);
    fprintf(stderr, "   pre:     act as pre processing filter (0)\n");
}

static void levels_process(LevelsPrivateData *pd, uint8_t *data,
                        int width, int height)
{
    int y_size = width * height;
    uint8_t *map = pd->lumamap;
    
    while (y_size--)  {
        *data = map[*data];
        data++;
    }
}


static void levels_get_config(char *options, int optsize)
{
    char buf[OPTION_BUF_SIZE];
    
    /* optsize actually unused */
    (void)optsize;
    
    optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION, 
                       MOD_AUTHOR, "VYMEO", "1");

    tc_snprintf(buf, sizeof(buf), "%d-%d", DEFAULT_IN_BLACK, 
                DEFAULT_IN_WHITE );
    optstr_param(options, "input", "input luma range (black-white)", 
                 "%d-%d", buf, "0", "255", "0", "255" );

    tc_snprintf(buf, sizeof(buf), "%f", DEFAULT_IN_GAMMA );
    optstr_param(options, "gamma", "input luma gamma", 
                 "%f", buf, "0.5", "3.5" );

    tc_snprintf(buf, sizeof(buf), "%d-%d", 
                DEFAULT_OUT_BLACK, DEFAULT_OUT_WHITE );
    optstr_param(options, "output", "output luma range (black-white)", 
                 "%d-%d", buf, "0", "255", "0", "255" );

    tc_snprintf(buf, sizeof(buf), "%i", DEFAULT_PREFILTER );
    optstr_param(options, "pre", "pre processing filter", 
                 "%i", buf, "0", "1" );
}

static int levels_init(LevelsPrivateData *pd, const vob_t *vob,
                                              const char *options) 
{
    if(!pd || !vob) {
        /* should never happen */
        return -1;
    }
    
    pd->parameter.in_black = DEFAULT_IN_BLACK;
    pd->parameter.in_white = DEFAULT_IN_WHITE;
    pd->parameter.in_gamma = DEFAULT_IN_GAMMA;
    pd->parameter.out_black = DEFAULT_OUT_BLACK;
    pd->parameter.out_white = DEFAULT_OUT_WHITE;
    pd->is_prefilter = DEFAULT_PREFILTER;

    if (options != NULL) {
        if (optstr_lookup(options, "help")) {
            help_optstr();
            return -1;
        }

        optstr_get(options, "input", "%d-%d", &pd->parameter.in_black, &pd->parameter.in_white );
        optstr_get(options, "gamma", "%f", &pd->parameter.in_gamma );
        optstr_get(options, "output", "%d-%d", &pd->parameter.out_black, &pd->parameter.out_white );
        optstr_get(options, "pre", "%d", &pd->is_prefilter);
    }  /* if options */

    if(vob->im_v_codec != CODEC_YUV) {
        tc_log_error(MOD_NAME, "This filter is only capable of YUV mode");
        return -1;
    }

    build_map(pd->lumamap, pd->parameter.in_black, pd->parameter.in_white, pd->parameter.in_gamma,
                 pd->parameter.out_black, pd->parameter.out_white);

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s #%d", MOD_VERSION, MOD_CAP, vframe->filter_id);
        tc_log_info(MOD_NAME, "scaling %d-%d gamma %f to %d-%d",
                    pd->parameter.in_black, pd->parameter.in_white,
                    pd->parameter.in_gamma,
                    pd->parameter.out_black, pd->parameter.out_white);
        tc_log_info(MOD_NAME, "%s-processing filter", MOD_NAME,
                    (pd->is_prefilter) ?"pre" :"post");
    }
    return 0;
}

int tc_filter(frame_list_t *vframe_, char *options)
{
    vframe_list_t *vframe = (vframe_list_t *)vframe_;
    LevelsPrivateData *pd = NULL;
    int tag = vframe->tag;

    if (tag & TC_AUDIO) {
        return 0;
    }
        
    pd = &levels_private_data[vframe->filter_id];

    if (tag & TC_FILTER_GET_CONFIG) {
        levels_get_config(options, -1);
    }

    if (tag & TC_FILTER_INIT) {
        int ret;
        vob_t *vob = tc_get_vob();

        if (vob == NULL) {
            return -1;
        }
    
        ret = levels_init(pd, vob, options);
        if (ret != 0) {
            return ret;
        }
    }  /* if INIT */

    if (!(vframe->attributes & TC_FRAME_IS_SKIPPED)
       && (((tag & TC_POST_PROCESS) && !pd->is_prefilter) 
         || ((tag & TC_PRE_PROCESS) && pd->is_prefilter))) {
        levels_process(pd, vframe->video_buf, 
                       vframe->v_width, vframe->v_height);
    }
    
    return 0;
}
