/*
 *  frame_threads.c
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

#include "framebuffer.h"
#include "video_trans.h"
#include "audio_trans.h"

#include "decoder.h"
#include "encoder.h"

#include "frame_threads.h"

static pthread_t master_thread_id=0;

static pthread_t afthread[TC_FRAME_THREADS_MAX];
static pthread_t vfthread[TC_FRAME_THREADS_MAX];

pthread_cond_t abuffer_fill_cv=PTHREAD_COND_INITIALIZER;
pthread_mutex_t abuffer_im_fill_lock=PTHREAD_MUTEX_INITIALIZER;
uint32_t abuffer_im_fill_ctr=0;

pthread_mutex_t abuffer_xx_fill_lock=PTHREAD_MUTEX_INITIALIZER;
uint32_t abuffer_xx_fill_ctr=0;

pthread_mutex_t abuffer_ex_fill_lock=PTHREAD_MUTEX_INITIALIZER;
uint32_t abuffer_ex_fill_ctr=0;

pthread_cond_t vbuffer_fill_cv=PTHREAD_COND_INITIALIZER;
pthread_mutex_t vbuffer_im_fill_lock=PTHREAD_MUTEX_INITIALIZER;
uint32_t vbuffer_im_fill_ctr=0;

pthread_mutex_t vbuffer_xx_fill_lock=PTHREAD_MUTEX_INITIALIZER;
uint32_t vbuffer_xx_fill_ctr=0;

pthread_mutex_t vbuffer_ex_fill_lock=PTHREAD_MUTEX_INITIALIZER;
uint32_t vbuffer_ex_fill_ctr=0;

int have_aframe_threads=0, have_aframe_workers=0;
static int aframe_threads_shutdown=0;

int have_vframe_threads=0, have_vframe_workers=0;
static int vframe_threads_shutdown=0;


//video only for thread safe zooming
static pthread_t fthread_id[TC_FRAME_THREADS_MAX];
pthread_mutex_t fth_id_lock=PTHREAD_MUTEX_INITIALIZER;
int fthread_index=0;
fthbuf_t tbuf[TC_FRAME_THREADS_MAX];
fthbuf_t tbuf_DI[TC_FRAME_THREADS_MAX];

/* ------------------------------------------------------------ 
 *
 * frame processing threads
 *
 * ------------------------------------------------------------*/

void frame_threads_init(vob_t *vob, int vworkers, int aworkers)
{
    
    int n;

    master_thread_id=pthread_self();


    //video
    
    have_vframe_workers = vworkers;

    if(vworkers==0) return;

    have_vframe_threads=1;

    if(verbose & TC_DEBUG) printf("[%s] starting %d frame processing thread(s)\n", PACKAGE, vworkers);

    // start the thread pool
    
    for(n=0; n<vworkers; ++n) {
	
	if(pthread_create(&vfthread[n], NULL, (void *) process_vframe, vob)!=0)
	    tc_error("failed to start video frame processing thread");
    }


    //audio
    
    have_aframe_workers = aworkers;

    if(aworkers==0) return;

    have_aframe_threads=1;

    if(verbose & TC_DEBUG) printf("[%s] starting %d frame processing thread(s)\n", PACKAGE, aworkers);

    // start the thread pool
    
    for(n=0; n<aworkers; ++n) {
	
	if(pthread_create(&afthread[n], NULL, (void *) process_aframe, vob)!=0)
	    tc_error("failed to start audio frame processing thread");
    }

}

void frame_threads_notify(int what)
{
  if(what==TC_AUDIO) {
    
    pthread_mutex_lock(&abuffer_im_fill_lock);

    //notify all threads to check for status
    pthread_cond_broadcast(&abuffer_fill_cv);

    pthread_mutex_unlock(&abuffer_im_fill_lock);
    return;
  }
  
  if(what==TC_VIDEO) {

    pthread_mutex_lock(&vbuffer_im_fill_lock);

    //notify all threads to check for status
    pthread_cond_broadcast(&vbuffer_fill_cv);

    pthread_mutex_unlock(&vbuffer_im_fill_lock);
    return;
  }
}


void frame_threads_close()
{
  
    int n;
    void *status;

    //audio
    
    if(have_aframe_threads==0) return;
    
    pthread_mutex_lock(&abuffer_im_fill_lock);
    aframe_threads_shutdown=1;
    pthread_mutex_unlock(&abuffer_im_fill_lock);

    //notify all threads of shutdown    
    frame_threads_notify(TC_AUDIO);
    
    for(n=0; n<have_aframe_workers; ++n) pthread_cancel(afthread[n]);
    
    //wait for threads to terminate
    for(n=0; n<have_aframe_workers; ++n) pthread_join(afthread[n], &status);

    if(verbose & TC_DEBUG) fprintf(stderr, "(%s) audio frame processing threads canceled\n", __FILE__);

    //video
    
    if(have_vframe_threads==0) return;
    
    pthread_mutex_lock(&vbuffer_im_fill_lock);
    vframe_threads_shutdown=1;
    pthread_mutex_unlock(&vbuffer_im_fill_lock);

    //notify all threads of shutdown
    frame_threads_notify(TC_VIDEO);
    for(n=0; n<have_vframe_workers; ++n) pthread_cancel(vfthread[n]);

    //wait for threads to terminate
    for(n=0; n<have_vframe_workers; ++n) pthread_join(vfthread[n], &status);
    
    if(verbose & TC_DEBUG) fprintf(stderr, "(%s) video frame processing threads canceled\n", __FILE__);
    
}

void process_vframe(vob_t *vob)
{
  
  int id;
  vframe_list_t *ptr = NULL;
  
  pthread_mutex_lock(&fth_id_lock);

  id = fthread_index++;
  
  fthread_id[id] = pthread_self();
  
  pthread_mutex_unlock(&fth_id_lock);
  
  memset(&tbuf[id], 0, sizeof(fthbuf_t));
  
  for(;;) {
    
    pthread_testcancel();
    
    pthread_mutex_lock(&vbuffer_im_fill_lock);
    
    while(vbuffer_im_fill_ctr==0) {
    
      pthread_cond_wait(&vbuffer_fill_cv, &vbuffer_im_fill_lock);
      
      //exit
      if(vframe_threads_shutdown) {
	pthread_mutex_unlock(&vbuffer_im_fill_lock);
	pthread_exit(0);
      }
    }
    
    pthread_mutex_unlock(&vbuffer_im_fill_lock);
    
    ptr = vframe_retrieve_status(FRAME_WAIT, FRAME_LOCKED);
    
    if(ptr==NULL) {
      if(verbose & TC_DEBUG) fprintf(stderr, "(%s) internal error (V|%d)\n", __FILE__, vbuffer_im_fill_ctr);
      
      pthread_testcancel();
      continue;
      //goto invalid_vptr; // this shouldn't happen but is non-fatal
    }

    pthread_testcancel();

    // regain lock
    pthread_mutex_lock(&vbuffer_im_fill_lock);

    //adjust counter
    --vbuffer_im_fill_ctr;

    //release lock
    pthread_mutex_unlock(&vbuffer_im_fill_lock);


    // regain lock
    pthread_mutex_lock(&vbuffer_xx_fill_lock);

    //adjust counter
    ++vbuffer_xx_fill_ctr;

    //release lock
    pthread_mutex_unlock(&vbuffer_xx_fill_lock);
    

    
    //------
    // video
    //------
    
    // external plugin pre-processing
    ptr->tag = TC_VIDEO|TC_PRE_M_PROCESS;
    process_vid_plugins(ptr);

    
    if(ptr->attributes & TC_FRAME_IS_SKIPPED) {
      vframe_remove(ptr);  // release frame buffer memory
      
      pthread_mutex_lock(&vbuffer_xx_fill_lock);
      --vbuffer_xx_fill_ctr;
      pthread_mutex_unlock(&vbuffer_xx_fill_lock);

      // notify sleeping import thread
      pthread_mutex_lock(&vframe_list_lock);
      pthread_cond_signal(&vframe_list_full_cv);
      pthread_mutex_unlock(&vframe_list_lock);
      
      continue;
      //goto invalid_vptr; // frame skipped
    }

    // internal processing of video
    ptr->tag = TC_VIDEO;
    process_vid_frame(vob, ptr);

    // external plugin post-processing
    ptr->tag = TC_VIDEO|TC_POST_M_PROCESS;
    process_vid_plugins(ptr);
    
    if(ptr->attributes & TC_FRAME_IS_SKIPPED) {
      vframe_remove(ptr);  // release frame buffer memory
      
      pthread_mutex_lock(&vbuffer_xx_fill_lock);
      --vbuffer_xx_fill_ctr;
      pthread_mutex_unlock(&vbuffer_xx_fill_lock);

      // notify sleeping import thread
      pthread_mutex_lock(&vframe_list_lock);
      pthread_cond_signal(&vframe_list_full_cv);
      pthread_mutex_unlock(&vframe_list_lock);

      continue;
      //goto invalid_vptr; // frame skipped
    }

    pthread_testcancel();
    
    // ready for encoding
    vframe_set_status(ptr, FRAME_READY);
    
    pthread_mutex_lock(&vbuffer_xx_fill_lock);
    --vbuffer_xx_fill_ctr;
    pthread_mutex_unlock(&vbuffer_xx_fill_lock);

    pthread_mutex_lock(&vbuffer_ex_fill_lock);
    ++vbuffer_ex_fill_ctr;
    pthread_mutex_unlock(&vbuffer_ex_fill_lock);
    
    //invalid_vptr:
  }
  
  return;
}


void process_aframe(vob_t *vob)
{
  
  aframe_list_t *ptr = NULL;
  
  for(;;) {
    
    pthread_testcancel();
    
    pthread_mutex_lock(&abuffer_im_fill_lock);
    
    while(abuffer_im_fill_ctr==0) {
      pthread_cond_wait(&abuffer_fill_cv, &abuffer_im_fill_lock);
      
      //exit
      if(aframe_threads_shutdown) {
	pthread_mutex_unlock(&abuffer_im_fill_lock);
	pthread_exit(0);
      }
    }
    
    pthread_mutex_unlock(&abuffer_im_fill_lock);
    
    ptr = aframe_retrieve_status(FRAME_WAIT, FRAME_LOCKED);
    
    if(ptr==NULL) {
      if(verbose & TC_DEBUG) fprintf(stderr, "(%s) internal error (A|%d)\n", __FILE__, abuffer_im_fill_ctr);
      
      pthread_testcancel();
      continue;
      //goto invalid_aptr; // this shouldn't happen but is non-fatal
    }

    pthread_testcancel();
    
    // regain locked during operation
    pthread_mutex_lock(&abuffer_im_fill_lock);

    --abuffer_im_fill_ctr;

    //release lock
    pthread_mutex_unlock(&abuffer_im_fill_lock);

    // regain locked during operation
    pthread_mutex_lock(&abuffer_xx_fill_lock);

    ++abuffer_xx_fill_ctr;
    
    //release lock
    pthread_mutex_unlock(&abuffer_xx_fill_lock);


    //------
    // audio
    //------
    
    // external plugin pre-processing
    ptr->tag = TC_AUDIO|TC_PRE_M_PROCESS;
    process_aud_plugins(ptr);

    if(ptr->attributes & TC_FRAME_IS_SKIPPED) {
      aframe_remove(ptr);  // release frame buffer memory

      pthread_mutex_lock(&abuffer_xx_fill_lock);
      --abuffer_xx_fill_ctr;
      pthread_mutex_unlock(&abuffer_xx_fill_lock);
      
      // notify sleeping import thread
      pthread_mutex_lock(&aframe_list_lock);
      pthread_cond_signal(&aframe_list_full_cv);
      pthread_mutex_unlock(&aframe_list_lock);
      
      continue;
      //goto invalid_aptr; // frame skipped
    }
    
    // internal processing of audio
    ptr->tag = TC_AUDIO;
    process_aud_frame(vob, ptr);
    
    // external plugin post-processing
    ptr->tag = TC_AUDIO|TC_POST_M_PROCESS;
    process_aud_plugins(ptr);
    
    if(ptr->attributes & TC_FRAME_IS_SKIPPED) {
      aframe_remove(ptr);  // release frame buffer memory

      pthread_mutex_lock(&abuffer_xx_fill_lock);
      --abuffer_xx_fill_ctr;
      pthread_mutex_unlock(&abuffer_xx_fill_lock);

      // notify sleeping import thread
      pthread_mutex_lock(&aframe_list_lock);
      pthread_cond_signal(&aframe_list_full_cv);
      pthread_mutex_unlock(&aframe_list_lock);

      continue;
      //goto invalid_aptr; // frame skipped
    }

    pthread_testcancel();

    // ready for encoding
    aframe_set_status(ptr, FRAME_READY);

    pthread_mutex_lock(&abuffer_xx_fill_lock);
    --abuffer_xx_fill_ctr;
    pthread_mutex_unlock(&abuffer_xx_fill_lock);

    pthread_mutex_lock(&abuffer_ex_fill_lock);
    ++abuffer_ex_fill_ctr;
    pthread_mutex_unlock(&abuffer_ex_fill_lock);

    //invalid_aptr:
  }
  
  return;
}


//video only:

int get_fthread_id(int flag)
{
  int n;
  
  pthread_t id;
  
  static int cc1=0, cc2;
  
  if(have_vframe_workers==0) return(0);
  
  id = pthread_self();
  
  if(id==master_thread_id) return(((flag)?cc1++:cc2++));
  
  for(n=0; n<TC_FRAME_THREADS_MAX; ++n) {
    if(fthread_id[n] == id) return(n);
  }
  
  tc_error("frame processing thread not registered");

  return(-1);
}

