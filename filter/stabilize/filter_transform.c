/*
 *  filter_transform.c
 *
 *  Copyright (C) Georg Martius - June 2007
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
 * Typical call:
transcode -V -J transform="crop=0" -i inp.m2v -y xdiv,pcm inp_stab.avi
 */

#define MOD_NAME    "filter_transform.so"
#define MOD_VERSION "v0.4 (2008-06-22)"
#define MOD_CAP     "transforms each frame according to transformations given in an input file (e.g. translation, rotate) see also filter stabilize"
#define MOD_AUTHOR  "Georg Martius"

#define MOD_FEATURES \
  TC_MODULE_FEATURE_FILTER|TC_MODULE_FEATURE_VIDEO
#define MOD_FLAGS \
  TC_MODULE_FLAG_RECONFIGURABLE | TC_MODULE_FLAG_DELAY
  
#include "transcode.h"
#include "filter.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"
#include "libtc/tccodecs.h"
#include "libtc/tcmodule-plugin.h"
#include "transform.h"

#define clip(x,mi,ma) ((x)<(mi)) ? mi : ( (x)>(ma) ? ma : x )
#define PIXEL(img,x,y,w,h,def) ((x) < 0 || (y) < 0) ? def : (((x) >=w || (y) >= h) ? def : img[x+y*w]) 

typedef struct {
  size_t framesize_src;  ///< size of frame buffer in bytes (src)
  size_t framesize_dest;    ///< size of frame buffer in bytes (dest)
  unsigned char* src; ///< copy of the current frame buffer
  unsigned char* dest;   ///< pointer to the current frame buffer (to overwrite)

  vob_t* vob;  ///< pointer to information structure
  int width_src,height_src;
  int width_dest,height_dest;
    transform_t* trans; ///< array of transformations
  int current_trans;  ///< index to current transformation
  int trans_len;       ///< length of trans array
 
  // Options
  int maxshift;   ///< maximum number of pixels we will shift
  double maxangle;///< maximum angle in rad
  int relative;   ///< whether to consider transforms as relative (to previous frame) or absolute transforms
  int smoothing;  ///< number of frames (forward and backward to use for smoothing transforms
  int crop;       ///< 1: black bg, 0: keep border from last frame(s)
  int invert;     ///< 1: invert transforms, 0: nothing
  // constants
  double rotation_threshhold; ///< threshhold below which no rotation is performed

  char input[256];
  FILE* f;
} trans_data_t;
 
static const char transform_help[] = ""
	"Overview\n"
	"    Reads a file with transform information for each frame\n"
	"     and applies them\n" 
	"Options\n"
	"    'input'    path to the file used to read the transforms(def: inputfile.stab)\n"
	"    'maxshift'  maximal number of pixels to translate image (def: -1 no limit)\n"
	"    'maxangle'  maximal angle in rad to rotate image (def: -1 no limit)\n"
	"    'crop'      0: keep border (def), 1: black backgr\n"
	"    'invert'    1: invert transforms(def: 0)\n"
	"    'relative'  consider transforms as 0: absolute, 1: relative (def)\n"
	"    'smoothing' number of frames*2+1 used for lowpass filtering \n"
        "                used for stabilizing (def: 10)\n";


void interpolate(unsigned char *rv, float x, float y, 
		 unsigned char* img, int width, int height, unsigned char def);
int transformRGB(trans_data_t* td);
/// applies current transformation to the image
int transformYUV(trans_data_t* td);
int read_input_file(trans_data_t* td);

/**
   Does things like:
    smoothing, 
    relative to absolute transform
*/
int preprocess_transforms(trans_data_t* td);


/*
  quatratic interpolation function. 
  rv is the destination pixel, x and y are the source coordinates in
  the image img 
*/
void interpolate(unsigned char *rv, float x, float y, 
		 unsigned char* img, int width, int height, unsigned char def){
  if(x<-1 || x>width || y<-1 || y>height){
    *rv=def;    
  }else{
    int x_c = (int)ceilf(x);
    int x_f = (int)floorf(x);
    int y_c = (int)ceilf(y);
    int y_f = (int)floorf(y);
    short v1 = PIXEL(img,x_c,y_c,width,height,def);
    short v2 = PIXEL(img,x_c,y_f,width,height,def);
    short v3 = PIXEL(img,x_f,y_c,width,height,def);
    short v4 = PIXEL(img,x_f,y_f,width,height,def);
    float f1 = 1-sqrt(fabs(x_c-x)*fabs(y_c-y));
    float f2 = 1-sqrt(fabs(x_c-x)*fabs(y_f-y));
    float f3 = 1-sqrt(fabs(x_f-x)*fabs(y_c-y));
    float f4 = 1-sqrt(fabs(x_f-x)*fabs(y_f-y));
    float s  = (v1*f1 + v2*f2 + v3*f3+ v4*f4)/(f1+f2+f3+f4);
    *rv=(unsigned char)s;
  }
}

int transformRGB(trans_data_t* td){
  transform_t t;
  int x=0,y=0;
  unsigned char *Y_1,*Y_2, *Cb_1, *Cb_2, *Cr_1, *Cr_2;
  t = td->trans[td->current_trans];
  
  Y_1  = td->src;  
  Y_2  = td->dest;  
  Cb_1 = td->src + td->width_src*td->height_src;
  Cb_2 = td->dest + td->width_dest*td->height_dest;
  Cr_1 = td->src + 5*td->width_src*td->height_src/4;
  Cr_2 = td->dest + 5*td->width_dest*td->height_dest/4;
  float c_s_x = td->width_src/2.0;
  float c_s_y = td->height_src/2.0;
  float c_d_x = td->width_dest/2.0;
  float c_d_y = td->height_dest/2.0;    

  // for each pixel in the destination image we calc the source
  // coordinate and make an interpolation: 
  //      p_d = c_d + M(p_s - c_s) + t 
  // where p are the points, c the center coordinate, 
  //  t the translation, and M the rotation matrix
  //      p_s = M^{-1}(p_d - c_d - t) + c_s
  if(fabs(t.alpha) > td->rotation_threshhold){
    for(x = 0; x < td->width_dest; x++){
      for(y = 0; y < td->height_dest; y++){
	float x_d1 = (x-c_d_x);
	float y_d1 = (y-c_d_y);
	float x_s  =  cos(-t.alpha)*x_d1 + sin(-t.alpha)*y_d1 + c_s_x -t.x;
	float y_s  = -sin(-t.alpha)*x_d1 + cos(-t.alpha)*y_d1 + c_s_y -t.y;
	unsigned char* dest=&Y_2[x+y*td->width_dest];
	interpolate(dest,x_s,y_s, 
		    Y_1,td->width_src, td->height_src, td->crop ? 16 : *dest);
      }
    }
  }else{
    for(x = 0; x < td->width_dest; x++){
      for(y = 0; y < td->height_dest; y++){
	short p = PIXEL(Y_1,myround(x-t.x),myround(y-t.y),
			td->width_src, td->height_src,-1);
	if(p == -1){
	  if(td->crop==1) Y_2[x+y*td->width_dest] = 16;
	}else
	  Y_2[x+y*td->width_dest]=(unsigned char)p;
      }
    }
  }

  int ws2 = td->width_src/2;
  int wd2 = td->width_dest/2;
  int hs2 = td->height_src/2;
  int hd2 = td->height_dest/2;
  if(fabs(t.alpha) > td->rotation_threshhold){
    for(x = 0; x < wd2; x++){
      for(y = 0; y < hd2; y++){
	float x_d1 = x-(c_d_x)/2;
	float y_d1 = y-(c_d_y)/2;
	float x_s  =  cos(-t.alpha)*x_d1 + sin(-t.alpha)*y_d1 + (c_s_x -t.x)/2;
	float y_s  = -sin(-t.alpha)*x_d1 + cos(-t.alpha)*y_d1 + (c_s_y -t.y)/2;
	unsigned char* dest=&Cr_2[x+y*wd2];
	interpolate(dest,x_s, y_s, Cr_1, ws2, hs2, td->crop ? 128 : *dest);
	dest=&Cb_2[x+y*wd2];
	interpolate(dest,x_s, y_s, Cb_1, ws2, hs2, td->crop ? 128 : *dest);      	
      }
    }
  }else{
    for(x = 0; x < wd2; x++){
      for(y = 0; y < hd2; y++){
	short cr = PIXEL(Cr_1,myround(x-t.x/2.0),myround(y-t.y/2.0),
			wd2, hd2,-1);
	short cb = PIXEL(Cb_1,myround(x-t.x/2.0),myround(y-t.y/2.0),
			wd2, hd2,-1);
	if(cr == -1){
	  if(td->crop==1){ 
	    Cr_2[x+y*wd2] = 128;
	    Cb_2[x+y*wd2] = 128;
	  }
	}else{
	  Cr_2[x+y*wd2]=(unsigned char)cr;
	  Cb_2[x+y*wd2]=(unsigned char)cb;
	}
      }
    }
  }
  printf("Not Supported yet!\n"); 
  return 1;
}

/// applies current transformation to the image
int transformYUV(trans_data_t* td){
  transform_t t;
  int x=0,y=0;
  unsigned char *Y_1,*Y_2, *Cb_1, *Cb_2, *Cr_1, *Cr_2;
  t = td->trans[td->current_trans];
  
  Y_1  = td->src;  
  Y_2  = td->dest;  
  Cb_1 = td->src + td->width_src*td->height_src;
  Cb_2 = td->dest + td->width_dest*td->height_dest;
  Cr_1 = td->src + 5*td->width_src*td->height_src/4;
  Cr_2 = td->dest + 5*td->width_dest*td->height_dest/4;
  float c_s_x = td->width_src/2.0;
  float c_s_y = td->height_src/2.0;
  float c_d_x = td->width_dest/2.0;
  float c_d_y = td->height_dest/2.0;    

  // for each pixel in the destination image we calc the source
  // coordinate and make an interpolation: 
  //      p_d = c_d + M(p_s - c_s) + t 
  // where p are the points, c the center coordinate, 
  //  t the translation, and M the rotation matrix
  //      p_s = M^{-1}(p_d - c_d - t) + c_s
  if(fabs(t.alpha) > td->rotation_threshhold){
    for(x = 0; x < td->width_dest; x++){
      for(y = 0; y < td->height_dest; y++){
	float x_d1 = (x-c_d_x);
	float y_d1 = (y-c_d_y);
	float x_s  =  cos(-t.alpha)*x_d1 + sin(-t.alpha)*y_d1 + c_s_x -t.x;
	float y_s  = -sin(-t.alpha)*x_d1 + cos(-t.alpha)*y_d1 + c_s_y -t.y;
	unsigned char* dest=&Y_2[x+y*td->width_dest];
	interpolate(dest,x_s,y_s, 
		    Y_1,td->width_src, td->height_src, td->crop ? 16 : *dest);
      }
    }
  }else{
    for(x = 0; x < td->width_dest; x++){
      for(y = 0; y < td->height_dest; y++){
	short p = PIXEL(Y_1,myround(x-t.x),myround(y-t.y),
			td->width_src, td->height_src,-1);
	if(p == -1){
	  if(td->crop==1) Y_2[x+y*td->width_dest] = 16;
	}else
	  Y_2[x+y*td->width_dest]=(unsigned char)p;
      }
    }
  }

  int ws2 = td->width_src/2;
  int wd2 = td->width_dest/2;
  int hs2 = td->height_src/2;
  int hd2 = td->height_dest/2;
  if(fabs(t.alpha) > td->rotation_threshhold){
    for(x = 0; x < wd2; x++){
      for(y = 0; y < hd2; y++){
	float x_d1 = x-(c_d_x)/2;
	float y_d1 = y-(c_d_y)/2;
	float x_s  =  cos(-t.alpha)*x_d1 + sin(-t.alpha)*y_d1 + (c_s_x -t.x)/2;
	float y_s  = -sin(-t.alpha)*x_d1 + cos(-t.alpha)*y_d1 + (c_s_y -t.y)/2;
	unsigned char* dest=&Cr_2[x+y*wd2];
	interpolate(dest,x_s, y_s, Cr_1, ws2, hs2, td->crop ? 128 : *dest);
	dest=&Cb_2[x+y*wd2];
	interpolate(dest,x_s, y_s, Cb_1, ws2, hs2, td->crop ? 128 : *dest);      	
      }
    }
  }else{
    for(x = 0; x < wd2; x++){
      for(y = 0; y < hd2; y++){
	short cr = PIXEL(Cr_1,myround(x-t.x/2.0),myround(y-t.y/2.0),
			wd2, hd2,-1);
	short cb = PIXEL(Cb_1,myround(x-t.x/2.0),myround(y-t.y/2.0),
			wd2, hd2,-1);
	if(cr == -1){
	  if(td->crop==1){ 
	    Cr_2[x+y*wd2] = 128;
	    Cb_2[x+y*wd2] = 128;
	  }
	}else{
	  Cr_2[x+y*wd2]=(unsigned char)cr;
	  Cb_2[x+y*wd2]=(unsigned char)cb;
	}
      }
    }
  }
  return 1;
}


int read_input_file(trans_data_t* td){
  char l[1024];
  int s=0;
  int i=0;
  int ti; // time (unused)
  transform_t t;
  while( fgets(l,1024,td->f)){
    if(l[0]=='#') continue;
    if(strlen(l)==0) continue;    
    
    if(sscanf(l,"%i %lf %lf %lf %i",&ti, &t.x, &t.y, &t.alpha, &t.extra)!=5){
      fprintf(stderr, "Cannot parse line: %s\n", l);
      return 0;
    }
    
    if(i>=s){ // resize transform list
      if(s==0) s=256;
      else s*=2;
      //fprintf(stderr,"resize: %i\n",s);
	
      if(!(td->trans=realloc(td->trans, sizeof(transform_t)* s))){
	fprintf(stderr,"Cannot allocate memory form transformations: %i\n",s);
	return 0;
      }
    }
    td->trans[i]=t;
    i++;
  }
  td->trans_len=i;

  return i;
}


/**
   Do things like:
    smoothing    
    relative to absolute transform
*/
int preprocess_transforms(trans_data_t* td){
  transform_t* ts = td->trans;
  int i;
  if(verbose & TC_DEBUG){
    printf("Preprocess transforms:\n");
  }
  if(td->smoothing>0){
    //smoothing 
    transform_t* ts2 = NEW(transform_t, td->trans_len);
    memcpy(ts2, ts, sizeof(transform_t) * td->trans_len);

  /*  we will do a sliding average with minimal update
     \hat x_{n/2} = x_1+x_2 + .. + x_n
     \hat x_{n/2+1} = x_2+x_3 + .. + x_{n+1} = x_{n/2} - x_1 + x_{n+1}     
     avg = \hat x / n
   */
    int s = td->smoothing*2+1;
    transform_t null = null_transform();
    // avg is the average over [-smoothing, smoothing] transforms 
    //  around the current point
    transform_t avg;
    // avg2 is a sliding average over the filtered signal (only to past) 
    //  with smoothing * 10 horizont to kill offsets
    transform_t avg2 = null_transform();
    double tau = 1.0/(3*s);
    // initialise sliding sum with hypothetic sum of -1th element
    transform_t s_sum = mult_transform(&null,td->smoothing+1);
    for(i=0; i<td->smoothing; i++){
      s_sum = add_transforms(&s_sum, i<td->trans_len ? &ts2[i] : &null);
    }

    for(i=0; i<td->trans_len; i++){
      transform_t* old = ((i-td->smoothing-1) < 0) 
	? &null : &ts2[(i-td->smoothing-1)];
      transform_t* new = ((i+td->smoothing) >= td->trans_len) 
	? &null : &ts2[(i+td->smoothing)];
      s_sum = sub_transforms(&s_sum, old);
      s_sum = add_transforms(&s_sum, new);

      avg = mult_transform(&s_sum, 1.0/s);

      // lowpass filter: meaning high frequency must be transformed away
      ts[i] = sub_transforms(&ts2[i], &avg);
      avg2 = add_transforms_(mult_transform(&avg2,1-tau),mult_transform(&ts[i],tau));
      ts[i] = sub_transforms(&ts[i],&avg2);

      if(verbose & TC_DEBUG ){
	printf("s_sum: %5lf %5lf %5lf, ts: %5lf,%5lf,%5lf\n",
	       s_sum.x,s_sum.y, s_sum.alpha, ts[i].x, ts[i].y, ts[i].alpha);
	printf("\tavg: %5lf,%5lf,%5lf avg2: %5lf,%5lf,%5lf\n", 
	       avg.x,avg.y,avg.alpha, avg2.x,avg2.y,avg2.alpha);      
      }
    }
    tc_free(ts2);
  }
  
  
  // invert?
  if(td->invert){
    for(i=0; i<td->trans_len; i++){
      ts[i] = mult_transform(&ts[i],-1);      
    }
  }
  
  // relative to absolute
  if(td->relative){
    transform_t t = ts[0];
    for(i=1; i<td->trans_len; i++){
      if(verbose  & TC_DEBUG ){
	printf("shift: %5lf\t %5lf\t %lf \n", t.x, t.y, t.alpha *180/M_PI);
      }
      ts[i] = add_transforms(&ts[i], &t); 
      t=ts[i];
    }
  }
  // crop at maximal shift
  if(td->maxshift!=-1)
    for(i=0; i<td->trans_len; i++){    
      ts[i].x     = clip(ts[i].x,-td->maxshift, td->maxshift);
      ts[i].y     = clip(ts[i].y,-td->maxshift, td->maxshift);
    }
  if(td->maxangle!=- 1.0)
    for(i=0; i<td->trans_len; i++)
      ts[i].alpha = clip(ts[i].alpha,-td->maxangle, td->maxangle);
  
  

  return 1;
}

/**
 * transform_init:  Initialize this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int transform_init(TCModuleInstance *self, uint32_t features)
{

  trans_data_t* td = NULL;
  TC_MODULE_SELF_CHECK(self, "init");
  TC_MODULE_INIT_CHECK(self, MOD_FEATURES, features);
    
  if((td = (trans_data_t*)tc_malloc(sizeof(trans_data_t))) == NULL){
    tc_log_error(MOD_NAME, "init: out of memory!");
    return TC_ERROR;
  }
  self->userdata = td;

  return TC_OK;
}


/**
 * transform_configure:  Configure this instance of the module.  See
 * tcmodule-data.h for function details.
 */
static int transform_configure(TCModuleInstance *self,
			       const char *options, vob_t *vob)
{
  trans_data_t *td = NULL;
  TC_MODULE_SELF_CHECK(self, "configure");

  td = self->userdata;

  if((td->vob = vob)== NULL) return TC_ERROR;

  //// Initialise private data structure

  //    td->framesize = td->vob->im_v_width * MAX_PLANES * sizeof(char) * 2 * td->vob->im_v_height * 2;    
  td->framesize_src = td->vob->im_v_size;    
  if((td->src = tc_malloc(td->framesize_src))!=NULL)
    memset(td->src, 0, td->framesize_src);
  else{
    fprintf(stderr, "tc_malloc failed\n");
    return TC_ERROR;
  }
  
  td->width_src  = td->vob->ex_v_width;
  td->height_src = td->vob->ex_v_height;
  
  // TODO: calc new size later
  td->width_dest  = td->vob->ex_v_width;
  td->height_dest = td->vob->ex_v_height;
  td->framesize_dest = td->vob->im_v_size;    
  td->dest = 0;
  
  td->trans=0;
  td->current_trans=0;
  
  /// Options
  td->maxshift=-1;
  td->maxangle=-1;
  if(strlen(td->vob->video_in_file)<250)
    sprintf(td->input,"%s.trf",td->vob->video_in_file);
    else sprintf(td->input,"transforms.dat");
  td->crop=0;
  td->relative=1;
  td->invert=0;
  td->smoothing=10;
  
  td->rotation_threshhold=0.25/(180/M_PI);
  
  if (options != NULL) {
    if(verbose & TC_DEBUG ){
      fprintf(stderr, "options=%s\n", options);
    }    
    optstr_get(options, "input", "%[^:]", (char*)&td->input);
  }
  if((td->f = fopen(td->input,"r")) == NULL){
    fprintf(stderr, "cannot open input file %s!\n", td->input);
    return (-1);
  }    
  
  // read input file
  if(!read_input_file(td)){
    fprintf(stderr, "error parsing input file %s!\n", td->input);
    return (-1);      
  }
  // process remaining options
  if (options != NULL) {
    
    optstr_get(options, "maxshift",  "%d", &td->maxshift);
    optstr_get(options, "maxangle",  "%lf", &td->maxangle);
    optstr_get(options, "smoothing", "%d", &td->smoothing);
    optstr_get(options, "crop"     , "%d", &td->crop);
    optstr_get(options, "invert"   , "%d", &td->invert);
    optstr_get(options, "relative" , "%d", &td->relative);
    
    if (verbose & TC_INFO) {
      printf("Image Transformation/Stabilization Settings:\n");
      printf("      maxshift = %d\n", td->maxshift);
      printf("      maxangle = %f\n", td->maxangle);
      printf("     smoothing = %d\n", td->smoothing);
      printf("          crop = %s\n", td->crop ? "Black" : "Keep");
      printf("      relative = %s\n", td->relative ? "True": "False");
      printf("        invert = %s\n", td->invert ? "True" : "False");
      printf("         input = %s\n", td->input);
    }		
  }
  
  if(td->maxshift > td->width_dest/2) td->maxshift=td->width_dest/2;
  if(td->maxshift > td->height_dest/2) td->maxshift=td->height_dest/2;
  
  if(!preprocess_transforms(td)){
    fprintf(stderr, "error while preprocessing transforms!\n");
    return TC_ERROR;            
  }
  
  return TC_OK;
}


/**
 * transform_filter_video: performs the transformation of frames
 * See tcmodule-data.h for function details.
 */

static int transform_filter_video(TCModuleInstance *self, vframe_list_t *frame)
{
  trans_data_t *td = NULL;
  
  TC_MODULE_SELF_CHECK(self, "filter_video");
  TC_MODULE_SELF_CHECK(frame, "filter_video");
  
  td = self->userdata;

  td->dest=(unsigned char*)frame->video_buf;    
  memcpy(td->src,frame->video_buf, td->framesize_src);
  if(td->current_trans >= td->trans_len){
    fprintf(stderr,"not enough transforms found!\n");
    return TC_ERROR;
  }
  
  if (td->vob->im_v_codec == CODEC_RGB){
    transformRGB(td);
  }else if(td->vob->im_v_codec == CODEC_YUV){
    transformYUV(td);
  }else{
    fprintf(stderr,"unsupported Codec: %i\n",td->vob->im_v_codec);
    return TC_ERROR;
  }
  td->current_trans++;
  return TC_OK;
}


/**
 * transform_fini:  Clean up after this instance of the module.  See
 * tcmodule-data.h for function details.
 */
static int transform_fini(TCModuleInstance *self)
{
  trans_data_t *td = NULL;
  TC_MODULE_SELF_CHECK(self, "fini");
  td = self->userdata;
  tc_free(td);
  self->userdata = NULL;
  return TC_OK;
}


/**
 * transform_stop:  Reset this instance of the module.  See tcmodule-data.h
 * for function details.
 */

static int transform_stop(TCModuleInstance *self)
{
  trans_data_t *td = NULL;
  TC_MODULE_SELF_CHECK(self, "stop");
  td = self->userdata;
  if(td->src) tc_free(td->src);    
  if(td->trans) tc_free(td->trans);    
  if(td->f) fclose(td->f);
  return TC_OK;
}

/**
 * stabilize_inspect:  Return the value of an option in this instance of
 * the module.  See tcmodule-data.h for function details.
 */

static int transform_inspect(TCModuleInstance *self,
			     const char *param, const char **value)
{
  TC_MODULE_SELF_CHECK(self, "inspect");
  TC_MODULE_SELF_CHECK(param, "inspect");
  TC_MODULE_SELF_CHECK(value, "inspect");
  
  if (optstr_lookup(param, "help")) {
    *value = transform_help;
  }
  return TC_OK;
}



static const TCCodecID transform_codecs_in[] = { 
    TC_CODEC_YUV420P, TC_CODEC_YUV422P, TC_CODEC_RGB, TC_CODEC_ERROR 
};
static const TCCodecID transform_codecs_out[] = { 
    TC_CODEC_YUV420P, TC_CODEC_YUV422P, TC_CODEC_RGB, TC_CODEC_ERROR 
};
TC_MODULE_FILTER_FORMATS(transform);

TC_MODULE_INFO(transform);

static const TCModuleClass transform_class = {
    .info         = &transform_info,

    .init         = transform_init,
    .fini         = transform_fini,
    .configure    = transform_configure,
    .stop         = transform_stop,
    .inspect      = transform_inspect,

    .filter_video = transform_filter_video,
};

TC_MODULE_ENTRY_POINT(transform)

/*************************************************************************/

static int transform_get_config(TCModuleInstance *self, char *options)
{
    TC_MODULE_SELF_CHECK(self, "get_config");

    optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION,
                       MOD_AUTHOR, "VRY4", "1");

    return TC_OK;
}

static int transform_process(TCModuleInstance *self, frame_list_t *frame)
{
    TC_MODULE_SELF_CHECK(self, "process");

    if (frame->tag & TC_PREVIEW && frame->tag & TC_VIDEO) {
        return transform_filter_video(self, (vframe_list_t *)frame);
    }
    return TC_OK;
}

/*************************************************************************/

TC_FILTER_OLDINTERFACE(transform)

/*************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
