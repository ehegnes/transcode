/*
 *  encoder.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#include "dl_loader.h"
#include "framebuffer.h"
#include "counter.h"
#include "video_trans.h"
#include "audio_trans.h"
#include "decoder.h"
#include "encoder.h"

#include "frame_threads.h"

// import export module handles
static void *export_ahandle, *export_vhandle;

long frames_encoded = 0;
long frames_dropped = 0;
long frames_cloned  = 0;
static pthread_mutex_t frame_counter_lock=PTHREAD_MUTEX_INITIALIZER;

static int export = 0;
static pthread_mutex_t export_lock=PTHREAD_MUTEX_INITIALIZER;

static int force_exit=0;
static pthread_mutex_t force_exit_lock=PTHREAD_MUTEX_INITIALIZER;

static int counter_encoding=0;
static int counter_skipping=0;

static long startsec;
static long startusec;

void tc_export_stop_nolock()
{
  force_exit=1;
  return;
}


int export_status()
{
  pthread_mutex_lock(&export_lock);

  if(export==TC_ON) {
    pthread_mutex_unlock(&export_lock);
    return(1);
  }
  
  pthread_mutex_unlock(&export_lock);
  return(0);
}

long tc_get_frames_encoded()
{
  long cc;
  pthread_mutex_lock(&frame_counter_lock);
  cc=frames_encoded;
  pthread_mutex_unlock(&frame_counter_lock);
  return(cc);
}

void tc_update_frames_encoded(long cc)
{
  pthread_mutex_lock(&frame_counter_lock);
  frames_encoded +=cc;
  pthread_mutex_unlock(&frame_counter_lock);
  return;
}

long tc_get_frames_dropped()
{
  long cc;
  pthread_mutex_lock(&frame_counter_lock);
  cc=frames_dropped;
  pthread_mutex_unlock(&frame_counter_lock);
  return(cc);
}

void tc_update_frames_dropped(long cc)
{
  pthread_mutex_lock(&frame_counter_lock);
  frames_dropped +=cc;
  pthread_mutex_unlock(&frame_counter_lock);
  return;
}

long tc_get_frames_cloned()
{
  long cc;
  pthread_mutex_lock(&frame_counter_lock);
  cc=frames_cloned;
  pthread_mutex_unlock(&frame_counter_lock);
  return(cc);
}

void tc_update_frames_cloned(long cc)
{
  pthread_mutex_lock(&frame_counter_lock);
  frames_cloned +=cc;
  pthread_mutex_unlock(&frame_counter_lock);
  return;
}

void tc_set_force_exit()
{
  pthread_mutex_lock(&force_exit_lock);
  force_exit=1;
  pthread_mutex_unlock(&force_exit_lock);
}

int tc_get_force_exit()
{
  int cc=0;

  pthread_mutex_lock(&force_exit_lock);
  cc=force_exit;
  pthread_mutex_unlock(&force_exit_lock);

  return(cc);
}

/* ------------------------------------------------------------ 
 *
 * export init
 *
 * ------------------------------------------------------------*/

int export_init(vob_t *vob, char *a_mod, char *v_mod)
{

   transfer_t export_para;

   // load export modules
   if((export_ahandle = load_module(((a_mod==NULL)? TC_DEFAULT_EXPORT_AUDIO: a_mod), TC_EXPORT+TC_AUDIO))==NULL) {
     fprintf(stderr,"(%s) loading audio export module failed\n", __FILE__);
     return(-1);
   }

   if((export_vhandle = load_module(((v_mod==NULL)? TC_DEFAULT_EXPORT_VIDEO: v_mod), TC_EXPORT+TC_VIDEO))==NULL) {
     fprintf(stderr,"(%s) loading video export module failed\n", __FILE__);
     return(-1);
   }

  export_para.flag = verbose;
  tca_export(TC_EXPORT_NAME, &export_para, NULL); 

  if(export_para.flag != verbose) {
    // module returned capability flag
    
    int cc=0;
    
    if(verbose & TC_DEBUG) 
      fprintf(stderr, "(%s) audio capability flag 0x%x | 0x%x\n", __FILE__, export_para.flag, vob->im_a_codec);    
    
    switch (vob->im_a_codec) {
      
    case CODEC_PCM: 
      cc=(export_para.flag & TC_CAP_PCM);
      break;
    case CODEC_AC3: 
      cc=(export_para.flag & TC_CAP_AC3);
      break;
    case CODEC_RAW: 
      cc=(export_para.flag & TC_CAP_AUD);
      break;
    default:
      cc=0;
    }
    
    if(!cc) {
      fprintf(stderr, "(%s) audio codec not supported by export module\n", __FILE__); 
      return(-1);
    }

  } else { 
   
    if(vob->im_a_codec != CODEC_PCM) {
      fprintf(stderr, "(%s) audio codec not supported by export module\n", __FILE__); 
      return(-1);
    }
  }
  
  export_para.flag = verbose;
  tcv_export(TC_EXPORT_NAME, &export_para, NULL);

  if(export_para.flag != verbose) {
    // module returned capability flag
    
    int cc=0;
    
    if(verbose & TC_DEBUG) 
      fprintf(stderr, "(%s) video capability flag 0x%x | 0x%x\n", __FILE__, export_para.flag, vob->im_v_codec);

    switch (vob->im_v_codec) {
      
    case CODEC_RGB: 
      cc=(export_para.flag & TC_CAP_RGB);
      break;
    case CODEC_YUV: 
      cc=(export_para.flag & TC_CAP_YUV);
      break;
    case CODEC_RAW: 
    case CODEC_RAW_YUV: 
      cc=(export_para.flag & TC_CAP_VID);
      break;
    default:
      cc=0;
    }
    
    if(!cc) {
      fprintf(stderr, "(%s) video codec not supported by export module\n", __FILE__); 
      return(-1);
    }

  } else {
    
    if(vob->im_a_codec != CODEC_RGB) {
      fprintf(stderr, "(%s) video codec not supported by export module\n", __FILE__); 
      return(-1);
    }
  }
  return(0);
}  

/* ------------------------------------------------------------ 
 *
 * export close, unload modules
 *
 * ------------------------------------------------------------*/

void export_shutdown()
{

    if(verbose & TC_DEBUG) {
	printf("unloading export modules\n");
    }

    // unload export modules
    unload_module(export_ahandle);
    unload_module(export_vhandle);
}


/* ------------------------------------------------------------ 
 *
 * encoder init
 *
 * ------------------------------------------------------------*/

int encoder_init(transfer_t *export_para, vob_t *vob)
{
  
  int ret;
  
  // flag
  pthread_mutex_lock(&export_lock);
  export = TC_ON;   
  pthread_mutex_unlock(&export_lock);
  
  export_para->flag = TC_VIDEO;
  if((ret=tcv_export(TC_EXPORT_INIT, export_para, vob))==TC_EXPORT_ERROR) {
    fprintf(stderr, "(%s) video export module error: init failed\n", __FILE__);
    return(-1);
  }
  
  export_para->flag = TC_AUDIO;
  if((ret=tca_export(TC_EXPORT_INIT, export_para, vob))==TC_EXPORT_ERROR) {
    fprintf(stderr, "(%s) audio export module error: init failed\n", __FILE__);
    return(-1);
  }
  
  return(0);
}


/* ------------------------------------------------------------ 
 *
 * encoder open
 *
 * ------------------------------------------------------------*/

int encoder_open(transfer_t *export_para, vob_t *vob)
{
  
  int ret;
  
  export_para->flag = TC_VIDEO;	
  if((ret=tcv_export(TC_EXPORT_OPEN, export_para, vob))==TC_EXPORT_ERROR) {
    fprintf(stderr, "(%s) video export module error: open failed\n", __FILE__);
    return(-1);
  }
  
  export_para->flag = TC_AUDIO;
  if((ret=tca_export(TC_EXPORT_OPEN, export_para, vob))==TC_EXPORT_ERROR) {
    fprintf(stderr, "(%s) audio export module error: open failed\n", __FILE__);
    return(-1);
  }
  
  return(0);
}


/* ------------------------------------------------------------ 
 *
 * encoder close
 *
 * ------------------------------------------------------------*/

int encoder_close(transfer_t *export_para)
{
  
  // close, errors not fatal

  export_para->flag = TC_AUDIO;
  tca_export(TC_EXPORT_CLOSE, export_para, NULL);

  export_para->flag = TC_VIDEO;
  tcv_export(TC_EXPORT_CLOSE, export_para, NULL);
  
  // flag
  pthread_mutex_lock(&export_lock);
  export = TC_OFF;	
  pthread_mutex_unlock(&export_lock);

  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) encoder closed\n", __FILE__);
	
  return(0);
}


/* ------------------------------------------------------------ 
 *
 * encoder stop
 *
 * ------------------------------------------------------------*/

int encoder_stop(transfer_t *export_para)
{
  
  int ret;

  export_para->flag = TC_VIDEO;
  if((ret=tcv_export(TC_EXPORT_STOP, export_para, NULL))==TC_EXPORT_ERROR) {
    fprintf(stderr, "(%s) video export module error: stop failed\n", __FILE__);
    return(-1);
  }
  
  export_para->flag = TC_AUDIO;
  if((ret=tca_export(TC_EXPORT_STOP, export_para, NULL))==TC_EXPORT_ERROR) {
    fprintf(stderr, "(%s) audio export module error: stop failed\n", __FILE__);
    return(-1);
  }
  
  return(0);
}


/* ------------------------------------------------------------ 
 *
 * encoder main loop
 *
 * ------------------------------------------------------------*/


void encoder(vob_t *vob, int frame_a, int frame_b)
{
    
    vframe_list_t *vptr = NULL;
    aframe_list_t *aptr = NULL;
    
    transfer_t export_para;

    int fid=0;

    int exit_on_encoder_error=0;
    int fill_flag=0;

    counter_encoding=0;
    counter_skipping=0;

    do {
      
      // check for ^C signal
      if(tc_get_force_exit()) {
	if(verbose & TC_DEBUG) fprintf(stderr, "(%s) export canceled on user request\n", __FILE__);
	return;
      }
      
    vretry:
      //check buffer fill level
      pthread_mutex_lock(&vframe_list_lock);
      
      if(vframe_fill_level(TC_BUFFER_READY)) {
	
	pthread_mutex_unlock(&vframe_list_lock);
	
	if((vptr = vframe_retrieve())!=NULL) {
	  fid = vptr->id;
	  goto cont1;
	}
	
      } else {
	
	pthread_mutex_unlock(&vframe_list_lock);
	
	//check import status
	if(!vimport_status() || tc_get_force_exit())  {
	  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) import closed - buffer empty (V)\n", __FILE__);
	  return;
	}
	
	if(verbose & TC_STATS) fprintf(stderr, "(%s) waiting for video frames\n", __FILE__);
      }
      
      //no frame available at this time
      
      usleep(tc_buffer_delay_enc);
      goto vretry;
      
      
    cont1:
      
      if(verbose & TC_STATS) fprintf(stderr, "got frame 0x%x (%d)\n", (int) vptr, fid);
      
      //audio
      
    aretry:
      //check buffer fill level
      pthread_mutex_lock(&aframe_list_lock);
      
      if(aframe_fill_level(TC_BUFFER_READY)) {
	
	pthread_mutex_unlock(&aframe_list_lock);
	
	if((aptr = aframe_retrieve())!=NULL) {
	  
	  goto cont2;
	}
	
      } else {
	
	pthread_mutex_unlock(&aframe_list_lock);
	
	//check import status
	if(!aimport_status() || tc_get_force_exit())  {
	  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) import closed - buffer empty (A)\n", __FILE__);	  
	  return;
	}
	if(verbose & TC_STATS) fprintf(stderr, "(%s) waiting for audio frames\n", __FILE__);
	
      }
      
      //no frame available at this time
      usleep(tc_buffer_delay_enc);
      goto aretry;
      
    cont2:
      
      if(verbose & TC_STATS) fprintf(stderr, "got audio frame (%d)\n", aptr->id );
      
      //--------------------------------
      //
      // need a valid pointer to proceed
      //
      //--------------------------------
      
      //cluster mode must take dropped frames into account
      if(tc_cluster_mode && (fid - tc_get_frames_dropped()) == frame_b) return;
      
      // check frame id
      if(frame_a <= fid && fid < frame_b) {
	
	if(!counter_encoding) {
	  counter_init(&startsec, &startusec);
	  ++counter_encoding;
	  if(verbose & TC_INFO && counter_skipping) printf("\n");
	}
	
	//video
	if(have_vframe_threads==0) {
	    
	  pthread_mutex_lock(&vbuffer_im_fill_lock);
	  --vbuffer_im_fill_ctr;
	  pthread_mutex_unlock(&vbuffer_im_fill_lock);
	  
	  pthread_mutex_lock(&vbuffer_xx_fill_lock);
	  ++vbuffer_xx_fill_ctr;
	  pthread_mutex_unlock(&vbuffer_xx_fill_lock);
	  
	  // external plugin pre-processing
	  vptr->tag = TC_VIDEO|TC_PRE_PROCESS;
	  process_vid_plugins(vptr);
	  
	  // internal processing of video
	  vptr->tag = TC_VIDEO;
	  process_vid_frame(vob, vptr);
	  
	  // external plugin post-processing
	  vptr->tag = TC_VIDEO|TC_POST_PROCESS;
	  process_vid_plugins(vptr);
	  postprocess_vid_frame(vob, vptr);
	  
	  pthread_mutex_lock(&vbuffer_xx_fill_lock);
	  --vbuffer_xx_fill_ctr;
	  pthread_mutex_unlock(&vbuffer_xx_fill_lock);
	  
	  pthread_mutex_lock(&vbuffer_ex_fill_lock);
	  ++vbuffer_ex_fill_ctr;
	  pthread_mutex_unlock(&vbuffer_ex_fill_lock);
	  
	}
	
	//second stage post-processing - (synchronous)
	
	vptr->tag = TC_VIDEO|TC_POST_S_PROCESS;
	process_vid_plugins(vptr);
	postprocess_vid_frame(vob, vptr);
	
	// encode and export video frame
	export_para.buffer = vptr->video_buf;
	export_para.size   = vptr->video_size;
	export_para.attributes = vptr->attributes;
	
	if(aptr->attributes & TC_FRAME_IS_KEYFRAME) export_para.attributes |= TC_FRAME_IS_KEYFRAME;
	
	export_para.flag   = TC_VIDEO;
	
	if(tcv_export(TC_EXPORT_ENCODE, &export_para, vob)<0) {
	  fprintf(stderr, "\nerror encoding video frame\n");
	  exit_on_encoder_error=1;
	}

	// maybe clone?
	vptr->attributes = export_para.attributes;

	pthread_mutex_lock(&vbuffer_ex_fill_lock);
	--vbuffer_ex_fill_ctr;
	pthread_mutex_unlock(&vbuffer_ex_fill_lock);

	//audio
	if(have_aframe_threads==0) {
	  
	  pthread_mutex_lock(&abuffer_im_fill_lock);
	  --abuffer_im_fill_ctr;
	  pthread_mutex_unlock(&abuffer_im_fill_lock);
	  
	  pthread_mutex_lock(&abuffer_xx_fill_lock);
	  ++abuffer_xx_fill_ctr;
	  pthread_mutex_unlock(&abuffer_xx_fill_lock);
	  
	  // external plugin pre-processing
	  aptr->tag = TC_AUDIO|TC_PRE_PROCESS;
	  process_aud_plugins(aptr);
	  
	  // internal processing of audio
	  aptr->tag = TC_AUDIO;
	  process_aud_frame(vob, aptr);
	  
	  // external plugin post-processing
	  aptr->tag = TC_AUDIO|TC_POST_PROCESS;
	  process_aud_plugins(aptr);
	  
	  pthread_mutex_lock(&abuffer_xx_fill_lock);
	  --abuffer_xx_fill_ctr;
	  pthread_mutex_unlock(&abuffer_xx_fill_lock);
	  
	  pthread_mutex_lock(&abuffer_ex_fill_lock);
	  ++abuffer_ex_fill_ctr;
	  pthread_mutex_unlock(&abuffer_ex_fill_lock);
	}
	
	//second stage post-processing - (synchronous)
	
	aptr->tag = TC_AUDIO|TC_POST_S_PROCESS;
	process_aud_plugins(aptr);
	
	// encode and export audio frame
	export_para.buffer = aptr->audio_buf;
	export_para.size   = aptr->audio_size;
	export_para.attributes = aptr->attributes;
	export_para.flag   = TC_AUDIO;
	
	if(tca_export(TC_EXPORT_ENCODE, &export_para, vob)<0) {
	  fprintf(stderr, "\nerror encoding audio frame\n");
	  exit_on_encoder_error=1;
	}

	// maybe clone?
	aptr->attributes = export_para.attributes;
	
	pthread_mutex_lock(&abuffer_ex_fill_lock);
	--abuffer_ex_fill_ctr;
	pthread_mutex_unlock(&abuffer_ex_fill_lock);

      
	if(verbose & TC_INFO) {
	  
	  if(!fill_flag) fill_flag=1;
	 
	  counter_print(frame_a, fid, "encoding", startsec, startusec, ((vob->video_out_file==NULL)?vob->audio_out_file:vob->video_out_file), vptr->thread_id);
	}
	
	// on success, increase global frame counter
	tc_update_frames_encoded(1); 
	
      } else {
	
	// finished?
	if(fid >= frame_b) {
	  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) encoder last frame finished\n", __FILE__);
	  
	  return;
	}
	
	if(have_aframe_threads==0) {
	  
	  pthread_mutex_lock(&vbuffer_im_fill_lock);
	  --vbuffer_im_fill_ctr;
	  pthread_mutex_unlock(&vbuffer_im_fill_lock);
	  
	  pthread_mutex_lock(&abuffer_im_fill_lock);
	  --abuffer_im_fill_ctr;
	  pthread_mutex_unlock(&abuffer_im_fill_lock);
	  
	} else {	
	  
	  pthread_mutex_lock(&vbuffer_im_fill_lock);
	  --vbuffer_ex_fill_ctr;
	  pthread_mutex_unlock(&vbuffer_im_fill_lock);
	  
	  pthread_mutex_lock(&abuffer_im_fill_lock);
	  --abuffer_ex_fill_ctr;
	  pthread_mutex_unlock(&abuffer_im_fill_lock);
	}
	
	if(!counter_skipping) {
	  counter_init(&startsec, &startusec);
	  ++counter_skipping;
	}
	
	if(verbose & TC_INFO) {
	  
	  if(!fill_flag) {
	    fill_flag=1;
	  }
	  counter_print(0, fid, "skipping", startsec, startusec, "/dev/null", vptr->thread_id);
	}
	
      } // frame processing loop
      
      // release frame buffer memory
      
      if(vptr!=NULL && !(vptr->attributes & TC_FRAME_IS_CLONED)) {
	
	vframe_remove(vptr);  
	
	//notify sleeping import thread
	pthread_mutex_lock(&vframe_list_lock);
	pthread_cond_signal(&vframe_list_full_cv);
	pthread_mutex_unlock(&vframe_list_lock);
	
	// reset pointer for next retrieve
	vptr=NULL;           
      }
      
      
      if(vptr!=NULL && (vptr->attributes & TC_FRAME_IS_CLONED)) {
	if(verbose & TC_DEBUG) fprintf (stdout, "(%d) V pointer done. Cloned: (%d)\n", vptr->id, (vptr->attributes));
	
	// delete clone flag
	vptr->attributes &= ~TC_FRAME_IS_CLONED;

	// set info for filters
	vptr->attributes |= TC_FRAME_WAS_CLONED;

	// this has to be done here, 
	// frame_threads.c won't see the frame again
	pthread_mutex_lock(&vbuffer_ex_fill_lock);
	++vbuffer_ex_fill_ctr;
	pthread_mutex_unlock(&vbuffer_ex_fill_lock);

	// update counter
	tc_update_frames_cloned(1);
      }
      
      if(aptr!=NULL && !(aptr->attributes & TC_FRAME_IS_CLONED)) {
	
	aframe_remove(aptr);  
	
	//notify sleeping import thread
	pthread_mutex_lock(&aframe_list_lock);
	pthread_cond_signal(&aframe_list_full_cv);
	pthread_mutex_unlock(&aframe_list_lock);
	
	// reset pointer for next retrieve
	aptr=NULL;
      }           

      if(aptr!=NULL && (aptr->attributes & TC_FRAME_IS_CLONED)) {
	if(verbose & TC_DEBUG) fprintf (stdout, "(%d) A pointer done. Cloned: (%d)\n", aptr->id, (aptr->attributes));
	
	// delete clone flag
	aptr->attributes &= ~TC_FRAME_IS_CLONED;

	// set info for filters
	aptr->attributes |= TC_FRAME_WAS_CLONED;

	// this has to be done here, 
	// frame_threads.c won't see the frame again
	pthread_mutex_lock(&abuffer_ex_fill_lock);
	++abuffer_ex_fill_ctr;
	pthread_mutex_unlock(&abuffer_ex_fill_lock);
      }
      
    } while(import_status() && !exit_on_encoder_error); // main frame decoding loop
    
    if(verbose & TC_DEBUG) fprintf(stderr, "(%s) export terminated - buffer(s) empty\n", __FILE__);
    
    return;
}



