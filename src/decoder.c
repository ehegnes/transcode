/*
 *  decoder.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "transcode.h"
#include "dl_loader.h"
#include "framebuffer.h"
#include "video_trans.h"
#include "audio_trans.h"
#include "encoder.h"
#include "decoder.h"
#include "frame_threads.h"

// stream handle
static FILE *fd_ppm=NULL, *fd_pcm=NULL;

// import module handle
static void *import_ahandle, *import_vhandle;

// import flags (1=import active | 0=import closed)
static volatile int aimport=0;
static volatile int vimport=0;
static pthread_mutex_t import_v_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t import_a_lock=PTHREAD_MUTEX_INITIALIZER;

//exported variables
int max_frame_buffer=TC_FRAME_BUFFER;
int max_frame_threads=TC_FRAME_THREADS;

static pthread_t athread=0, vthread=0;

#define TEST_ON_BUFFER_FULL    0
#define TEST_ON_BUFFER_REQUEST 1
#define TEST_ON_INTERUPT       2

//-------------------------------------------------------------------------
//
// signal handler callback
//
//--------------------------------------------------------------------------

void tc_import_stop_nolock()
{
  vimport = aimport = 0;
  return;
}

//-------------------------------------------------------------------------
//
// callback for external import threads shutdown request
//
//--------------------------------------------------------------------------

void tc_import_stop()
{
  
  vimport_stop();
  aimport_stop();

  frame_threads_notify(TC_VIDEO);
  frame_threads_notify(TC_AUDIO);

  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) import stop requested by client=%ld (main=%ld) import status=%d\n", __FILE__, (unsigned long)pthread_self(), (unsigned long)tc_pthread_main, import_status());

}

//-------------------------------------------------------------------------
//
// cancel import threads
//
// called by transcode (signal handler thread) 
//
//-------------------------------------------------------------------------

#ifdef HAVE_IBP
extern pthread_mutex_t xio_lock;
#endif

void import_threads_cancel()
{
  
  void *status=NULL;

  int cc1, cc2;

#ifdef HAVE_IBP
  pthread_mutex_lock(&xio_lock);
#endif

  if (tc_decoder_delay)
      printf("\n(%s) sleeping for %d seconds to cool down\n", __FILE__, 2*tc_decoder_delay);

  // notify import threads, if not yet done, that task is done
  tc_import_stop();

  cc1=pthread_cancel(vthread);
  cc2=pthread_cancel(athread);
  
  if(verbose & TC_DEBUG) {
    
    if(cc1 == ESRCH) fprintf(stderr, "(%s) video thread already terminated\n", __FILE__);  
    
    if(cc2 == ESRCH) fprintf(stderr, "(%s) audio thread already terminated\n", __FILE__);  
    
    fprintf(stderr, "(%s) A/V import canceled (%ld) (%ld)\n", __FILE__, (unsigned long)pthread_self(), (unsigned long)tc_pthread_main);
    
  }

  //wait for threads to terminate
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
  pthread_cond_signal(&vframe_list_full_cv);
#endif
#ifdef HAVE_IBP
      pthread_mutex_unlock(&xio_lock);
#endif
  cc1=pthread_join(vthread, &status);
  
  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) video thread exit (ret_code=%d) (status_code=%d)\n", __FILE__, cc1, (int) status);
  
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
  pthread_cond_signal(&aframe_list_full_cv);
#endif
#ifdef HAVE_IBP
      pthread_mutex_unlock(&xio_lock);
#endif
  cc2=pthread_join(athread, &status);
  
  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) audio thread exit (ret_code=%d) (status_code=%d)\n", __FILE__, cc2, (int) status);

  cc1 = pthread_mutex_trylock(&vframe_list_lock);
  
  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) vframe_list_lock=%s\n", __FILE__, (cc1==EBUSY)? "BUSY":"0");
  if(cc1 == 0) pthread_mutex_unlock(&vframe_list_lock);
  
  cc2 = pthread_mutex_trylock(&aframe_list_lock);
  
  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) aframe_list_lock=%s\n", __FILE__, (cc2==EBUSY)? "BUSY":"0");
  if(cc2 == 0) pthread_mutex_unlock(&aframe_list_lock);

  return;

}

//-------------------------------------------------------------------------

void import_threads_create(vob_t *vob)
{

  //start import threads
  //flag on, in case we restart the decoder

  aimport_start();

  if(pthread_create(&athread, NULL, (void *) aimport_thread, vob)!=0)
    tc_error("failed to start audio stream import thread");

  vimport_start();

  if(pthread_create(&vthread, NULL, (void *) vimport_thread, vob)!=0)
    tc_error("failed to start video stream import thread");

  return;
}

//-------------------------------------------------------------------------
//
// initialize import by loading modules and checking capabilities
//
// called by transcode (main thread) in transcoder() 
//
//-------------------------------------------------------------------------

int import_init(vob_t *vob, char *a_mod, char *v_mod)
{
	transfer_t import_para;
	int cc;

	memset(&import_para, 0, sizeof(transfer_t));

	// load audio import module

	if((import_ahandle=load_module(((a_mod==NULL)? TC_DEFAULT_IMPORT_AUDIO: a_mod), TC_IMPORT+TC_AUDIO))==NULL)
	{
		fprintf(stderr, "Loading audio import module failed\n");
		fprintf(stderr, "Did you enable this module when you ran configure?\n");
		return(-1);
	}

	aimport_start();

	// load video import module

	if((import_vhandle=load_module(((v_mod==NULL)? TC_DEFAULT_IMPORT_VIDEO: v_mod), TC_IMPORT+TC_VIDEO))==NULL)
	{
		fprintf(stderr, "Loading video import module failed\n");
		fprintf(stderr, "Did you enable this module when you ran configure?\n");
		return(-1);
	}

	vimport_start();

	// check import module capability, inherit verbosity flag

	import_para.flag = verbose;
	tca_import(TC_IMPORT_NAME, &import_para, NULL); 

	if(import_para.flag != verbose)
	{
		// module returned capability flag
		if(verbose & TC_DEBUG) 
			fprintf(stderr, "Audio capability flag 0x%x | 0x%x\n", import_para.flag, vob->im_a_codec);

		switch (vob->im_a_codec)
		{
			case CODEC_PCM: 
			{
				cc=(import_para.flag & TC_CAP_PCM);
				break;
			}
	
			case CODEC_AC3: 
			{
				cc=(import_para.flag & TC_CAP_AC3);
				break;
			}
	
			case CODEC_RAW: 
			{
				cc=(import_para.flag & TC_CAP_AUD);
				break;
			}
	
			default:
			{
				cc=0;
			}
		}
	}
	else
		cc = vob->im_a_codec == CODEC_PCM;

	
	if(!cc)
	{
		fprintf(stderr, "Audio format not supported by import module\n"); 
		return(-1);
	}

	import_para.flag = verbose;
	tcv_import(TC_IMPORT_NAME, &import_para, NULL);

	if(import_para.flag != verbose)
	{
		// module returned capability flag

		if(verbose & TC_DEBUG) 
			fprintf(stderr, "Video capability flag 0x%x | 0x%x\n", import_para.flag, vob->im_v_codec);

		switch (vob->im_v_codec)
		{
			case CODEC_RGB: 
			{
				cc=(import_para.flag & TC_CAP_RGB);
				break;
			}

			case CODEC_YUV: 
			{
				cc=(import_para.flag & TC_CAP_YUV);
				break;
			}

			case CODEC_YUV422: 
			{
				cc=(import_para.flag & TC_CAP_YUV422);
				break;
			}

			case CODEC_RAW_YUV: 
			case CODEC_RAW: 
			{
				cc=(import_para.flag & TC_CAP_VID);
				break;
			}

			default:
			{
				cc=0;
			}
		}
	}
	else
		cc = vob->im_v_codec == CODEC_RGB;

	if(!cc)
	{
		fprintf(stderr, "Video format not supported by import module\n"); 
		fprintf(stderr, "Please try --uyvy or --use_rgb\n");
		return(-1);
	}

	return(0);
}

//-------------------------------------------------------------------------
//
// initialize modules for opening files and decoding etc.  
//
// called by transcode (main thread) 
//
//-------------------------------------------------------------------------

int import_open(vob_t *vob)
{
  
  transfer_t import_para;

  memset(&import_para, 0, sizeof(transfer_t));
  
  // start audio stream
  import_para.flag=TC_AUDIO;
  
  if(tca_import(TC_IMPORT_OPEN, &import_para, vob)<0) {
    fprintf(stderr, "audio import module error: OPEN failed\n");
    return(-1);
  }
  
  fd_pcm = import_para.fd;
  
  memset(&import_para, 0, sizeof(transfer_t));

  // start video stream
  import_para.flag=TC_VIDEO;
  
  if(tcv_import(TC_IMPORT_OPEN, &import_para, vob)<0) {
    fprintf(stderr, "video import module error: OPEN failed\n");
    return(-1);
  }
  
  fd_ppm = import_para.fd;

  // now we can start the import threads, the file handles are valid

  return(0);

}
 

//-------------------------------------------------------------------------
//
// prepare modules for decoder shutdown etc.  
//
// called by transcode (main thread) 
//
//-------------------------------------------------------------------------

int import_close()
{

    int ret;
    transfer_t import_para;

    //TC_VIDEO:
      
    memset(&import_para, 0, sizeof(transfer_t));

    import_para.flag = TC_VIDEO;
    import_para.fd   = fd_ppm;
    
    if((ret=tcv_import(TC_IMPORT_CLOSE, &import_para, NULL))==TC_IMPORT_ERROR) {
      fprintf(stderr, "video import module error: CLOSE failed\n");
      return(-1);
    }
    fd_ppm = NULL;
    
    
    //TC_AUDIO:

    memset(&import_para, 0, sizeof(transfer_t));
    
    import_para.flag = TC_AUDIO;
    import_para.fd   = fd_pcm;
    
    if((ret=tca_import(TC_IMPORT_CLOSE, &import_para, NULL))==TC_IMPORT_ERROR) {
      fprintf(stderr, "audio import module error: CLOSE failed\n");
      return(-1);
    }
    fd_pcm = NULL;
    
    return(0);
}


//-------------------------------------------------------------------------
//
// check for video import flag
//
// called by video import thread, returns 1 on exit request
//
//-------------------------------------------------------------------------


static int vimport_test_shutdown()
{

  pthread_mutex_lock(&import_v_lock);
  
  if(!vimport) {
    pthread_mutex_unlock(&import_v_lock);
    
    if(verbose & TC_DEBUG) {
      fprintf(stderr, "(%s) video import cancelation requested\n", __FILE__);
    }
    
    return(1);  // notify thread to exit immediately

  } else 
    pthread_mutex_unlock(&import_v_lock);
  
  return(0);
}

//-------------------------------------------------------------------------
//
// optimized fread, use with care
//
//-------------------------------------------------------------------------

#ifdef PIPE_BUF
#define BLOCKSIZE PIPE_BUF /* 4096 on linux-x86 */
#else
#define BLOCKSIZE 4096
#endif

static int mfread(char *buf, int size, int nelem, FILE *f)
{
  int fd = fileno(f);
  int n=0, r1 = 0, r2 = 0;
  while (n < size*nelem-BLOCKSIZE) {
    if ( !(r1 = read (fd, &buf[n], BLOCKSIZE))) return 0;
    n += r1;
  }
  while (size*nelem-n) {
    if ( !(r2 = read (fd, &buf[n], size*nelem-n)))return 0;
    n += r2;
  }
  return (nelem);
}

static void
import_lock_cleanup (void *arg)
{
  pthread_mutex_unlock ((pthread_mutex_t *)arg);
}

//-------------------------------------------------------------------------
//
// video import thread
//
//-------------------------------------------------------------------------

void vimport_thread(vob_t *vob)
{
  
  long int i=0;
  
  int ret=0, vbytes;
  
  vframe_list_t *ptr=NULL;

  transfer_t import_para;

  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) video thread id=%ld\n", __FILE__, (unsigned long)pthread_self());

  // bytes per video frame
  vbytes = vob->im_v_size;
  
  for(;;) {

    // init structure

    memset(&import_para, 0, sizeof(transfer_t));
    
    if(verbose & TC_STATS) printf("\n%10s [%ld] V=%d bytes\n", "requesting", i, vbytes);

    pthread_testcancel();

    //check buffer fill level
    pthread_mutex_lock(&vframe_list_lock);
    
    pthread_cleanup_push (import_lock_cleanup, &vframe_list_lock);

    while(vframe_fill_level(TC_BUFFER_FULL)) {
	pthread_cond_wait(&vframe_list_full_cv, &vframe_list_lock);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
	pthread_testcancel();
#endif
	
	// check for pending shutdown via ^C
	if(vimport_test_shutdown()) {
	  pthread_exit( (int *) 11);
	}
    }

    pthread_cleanup_pop (0);

    pthread_mutex_unlock(&vframe_list_lock);
    
    // get a frame buffer or wait
    while((ptr = vframe_register(i))==NULL) {
      
      pthread_testcancel();
      
      // check for pending shutdown via ^C
      if(vimport_test_shutdown()) {
	pthread_exit ( (int *) 12);
      }
      
      //next buffer in row may be still locked
      usleep(tc_buffer_delay_dec);
    }
    
    ptr->attributes=0;
    
    // read video frame
    
    // check if import module reades data

    if(fd_ppm != NULL) {
      
      if(vbytes && (ret = mfread(ptr->video_buf, vbytes, 1, fd_ppm)) != 1)
	ret=-1;

      ptr->video_size = vbytes;

    } else {
	  
      import_para.buffer  = ptr->video_buf;
      import_para.buffer2 = ptr->video_buf2;
      import_para.size    = vbytes;
      import_para.flag    = TC_VIDEO;

      //fprintf(stderr, "DECODE 1\n");
      ret = tcv_import(TC_IMPORT_DECODE, &import_para, vob);
      //fprintf(stderr, "DECODE 2\n");

      // import module return information on true frame size
      // in import_para.size

      ptr->video_size = import_para.size;

      //transcode v.0.5.0-pre8
      ptr->attributes |= import_para.attributes;
    }
    
    if (ret<0) {
      if(verbose & TC_DEBUG) fprintf(stderr, "(%s) video data read failed - end of stream\n", __FILE__);

      // stop decoder	  
      vframe_remove(ptr); 
      
      // set flag
      vimport_stop();

      //exit
      pthread_exit( (int *) 13);
    }
    
    // init frame buffer structure with import frame data
    ptr->v_height   = vob->im_v_height;
    ptr->v_width    = vob->im_v_width;
    ptr->v_bpp      = vob->v_bpp;
    
    ptr->thread_id = (int) getpid();

    pthread_testcancel();

    pthread_mutex_lock(&vbuffer_im_fill_lock);
    ++vbuffer_im_fill_ctr;
    pthread_mutex_unlock(&vbuffer_im_fill_lock);

    //first stage pre-processing - (synchronous)
    preprocess_vid_frame(vob, ptr);    
    
    //plugin pre-processing - (synchronous)
    ptr->tag = TC_VIDEO|TC_PRE_S_PROCESS;
    process_vid_plugins(ptr);

    // done and ready for encoder
    vframe_set_status(ptr, FRAME_WAIT);
    
    //no frame threads?
    if(have_vframe_threads==0) {
      vframe_set_status(ptr, FRAME_READY);
    } else {
      //notify sleeping frame processing threads
      pthread_mutex_lock(&vbuffer_im_fill_lock);
      pthread_cond_signal(&vbuffer_fill_cv);
      pthread_mutex_unlock(&vbuffer_im_fill_lock);
    }
    
    if(verbose & TC_STATS) printf("%10s [%ld] V=%d bytes\n", "received", i, ptr->video_size);
    
    // check for pending shutdown via ^C
    if(vimport_test_shutdown()) pthread_exit( (int *)14);

    ++i; // get next frame
  }
}

//-------------------------------------------------------------------------
//
// check for audio import status flag
//
// called by audio import thread, returns 1 on exit request
//
//-------------------------------------------------------------------------

static int aimport_test_shutdown(int mode)
{

  pthread_mutex_lock(&import_a_lock);
  
  if(!aimport) {
    pthread_mutex_unlock(&import_a_lock);
    
    if(verbose & TC_DEBUG) {
      fprintf(stderr, "(%s) audio import cancelation requested (%d)\n", __FILE__, mode);
    }
    
    return(1);  // notify thread to exit immediately
    
  } else 
    pthread_mutex_unlock(&import_a_lock);
  
  return(0);
}

//-------------------------------------------------------------------------
//
// audio import thread
//
//-------------------------------------------------------------------------

void aimport_thread(vob_t *vob)
{
  
  long int i=0;
  
  int ret=0, abytes;
  
  aframe_list_t *ptr=NULL;

  transfer_t import_para;


  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) audio thread id=%ld\n", __FILE__, (unsigned long)pthread_self());

  // bytes per audio frame
  abytes = vob->im_a_size;
  
  for(;;) {

    // init structure

    memset(&import_para, 0, sizeof(transfer_t));
    
    // audio adjustment for non PAL frame rates:
    
    if(i!= 0 && i%TC_LEAP_FRAME == 0) {
      abytes = vob->im_a_size + vob->a_leap_bytes;
    } else {
      abytes = vob->im_a_size;
    }

    if(verbose & TC_STATS) printf("\n%10s [%ld] A=%d bytes\n", "requesting", i, abytes);

    pthread_testcancel();
    
    //check buffer fill level
    pthread_mutex_lock(&aframe_list_lock);
    
    pthread_cleanup_push (import_lock_cleanup, &aframe_list_lock);

    while(aframe_fill_level(TC_BUFFER_FULL)) {
      pthread_cond_wait(&aframe_list_full_cv, &aframe_list_lock);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
      pthread_testcancel();
#endif

      // check for pending shutdown via ^C
      if(aimport_test_shutdown(TEST_ON_BUFFER_FULL)) {
	pthread_exit( (int *) 11);
      }
    }
    
    pthread_cleanup_pop (0);

    pthread_mutex_unlock(&aframe_list_lock);
    
    // get a frame buffer or wait
    while((ptr = aframe_register(i))==NULL) {
      
      pthread_testcancel();
      
      // check for pending shutdown via ^C
      if(aimport_test_shutdown(TEST_ON_BUFFER_REQUEST)) {
	pthread_exit( (int *) 12);
      } 
      
      //next buffer in row may be still locked
      usleep(tc_buffer_delay_dec);
    }
    
    ptr->attributes=0;
    
    // read audio frame
    
    if(vob->sync>0) {
 
      // discard vob->sync frames

      while (vob->sync--) {

	if(fd_pcm != NULL) {
	  
	  if(abytes && (ret = mfread(ptr->audio_buf, abytes, 1, fd_pcm))!=1) {
	    ret=-1;	 
	    
	    ptr->audio_size = abytes;
	    break;
	  }
	  
	} else {
	  
	  import_para.buffer = ptr->audio_buf;
	  import_para.size   = abytes;
	  import_para.flag   = TC_AUDIO;
	  
	  ret = tca_import(TC_IMPORT_DECODE, &import_para, vob);
	  if(ret==-1) break;

	  ptr->audio_size = import_para.size;
	}
      }
      ++vob->sync;
    }
    
    // default 
    if(vob->sync==0) {

      // check if import module reades data

      if(fd_pcm != NULL) {
      
	if(abytes && (ret = mfread(ptr->audio_buf, abytes, 1, fd_pcm))!=1)
	  ret=-1;	 
	
	ptr->audio_size = abytes;

      } else {
	  
	import_para.buffer = ptr->audio_buf;
	import_para.size   = abytes;
	import_para.flag   = TC_AUDIO;
	
	ret = tca_import(TC_IMPORT_DECODE, &import_para, vob);

	ptr->audio_size = import_para.size;
	// import module returns information on true frame size
	// in import_para.size variable
      }
    }    
    
    // silence
    if(vob->sync<0) {
      if(verbose & TC_DEBUG) 
	fprintf(stderr, "\n\n zero padding %d", vob->sync);
      memset(ptr->audio_buf, 0, abytes);
      ptr->audio_size = abytes;
      ++vob->sync;
    }

    if(ret<0) {
      if(verbose & TC_DEBUG) fprintf(stderr, "(%s) audio data read failed - end of stream\n", __FILE__);

      // stop decoder	  
      aframe_remove(ptr); 
      
      // set flag
      aimport_stop();

      //exit
      pthread_exit((int *) 13);
    }
    
    
    // init frame buffer structure with import frame data
    ptr->a_rate = vob->a_rate;
    ptr->a_bits = vob->a_bits;
    ptr->a_chan = vob->a_chan;

    ptr->thread_id = (int) getpid();

    pthread_testcancel();

    pthread_mutex_lock(&abuffer_im_fill_lock);
    ++abuffer_im_fill_ctr;
    pthread_mutex_unlock(&abuffer_im_fill_lock);

    //first stage pre-processing - (synchronous)
    ptr->tag = TC_AUDIO|TC_PRE_S_PROCESS;
    process_aud_plugins(ptr);

    // done and ready for encoder
    aframe_set_status(ptr, FRAME_WAIT);

    //no frame threads?
    if(have_aframe_threads==0) {
      aframe_set_status(ptr, FRAME_READY);
    } else {
      //notify sleeping frame processing threads
	pthread_mutex_lock(&abuffer_im_fill_lock);
	pthread_cond_signal(&abuffer_fill_cv);
	pthread_mutex_unlock(&abuffer_im_fill_lock);
    }

    if(verbose & TC_STATS) printf("%10s [%ld] A=%d bytes\n", "received", i, ptr->audio_size);

    // check for pending shutdown via ^C
    if(aimport_test_shutdown(TEST_ON_INTERUPT)) pthread_exit((int *)14);
    
    ++i; // get next frame
  }
}

//-------------------------------------------------------------------------
//
// unload import modules
//
// called by transcode (main thread) in transcoder()
//
//-------------------------------------------------------------------------

void import_shutdown()
{

  if(verbose & TC_DEBUG) {
    printf("(%s) unloading audio import module\n", __FILE__);
  }
  
  unload_module(import_ahandle);
  import_ahandle = NULL;

  if(verbose & TC_DEBUG) {
    printf("(%s) unloading video import module\n", __FILE__);
  }
  
  unload_module(import_vhandle);
  import_vhandle = NULL;

}

//-------------------------------------------------------------------------
//
// set video import status flag to OFF
//
//-------------------------------------------------------------------------

void vimport_stop()
{
  
  // thread safe

  pthread_mutex_lock(&import_v_lock);

  // set flag stop reading
  vimport = 0;

  // release lock
  pthread_mutex_unlock(&import_v_lock);

  //cool down
  sleep(tc_decoder_delay);

  return;

}

//-------------------------------------------------------------------------
//
// set video import status flag to ON
//
//-------------------------------------------------------------------------

void vimport_start()
{
  
  // thread safe

  pthread_mutex_lock(&import_v_lock);

  // set flag 
  vimport = 1;

  // release lock
  pthread_mutex_unlock(&import_v_lock);

  return;

}

//-------------------------------------------------------------------------
//
// set audio import status flag to OFF
//
//-------------------------------------------------------------------------

void aimport_stop()
{
  
  // thread safe

  pthread_mutex_lock(&import_a_lock);

  // set flag  stop reading
  aimport = 0;

  // release lock
  pthread_mutex_unlock(&import_a_lock);

  //cool down
  sleep(tc_decoder_delay);

  return;

}


//-------------------------------------------------------------------------
//
// set audio import status flag to ON
//
//-------------------------------------------------------------------------

void aimport_start()
{
  
  // thread safe

  pthread_mutex_lock(&import_a_lock);

  // set flag  
  aimport = 1;

  // release lock
  pthread_mutex_unlock(&import_a_lock);

  return;

}

//-------------------------------------------------------------------------
//
// check audio import status flag and buffer fill level 
//
//-------------------------------------------------------------------------

int aimport_status()
{
  
  int cc;

  pthread_mutex_lock(&import_a_lock);

  cc = aimport;

  pthread_mutex_unlock(&import_a_lock);

  if (cc)
    return 1;

  // decoder finished, check for frame list
  pthread_mutex_lock(&aframe_list_lock);
  
  cc = (aframe_list_tail==NULL) ? 0 : 1;
  
  pthread_mutex_unlock(&aframe_list_lock);
  return(cc);
}

void aimport_notify()
{
  
  //notify sleeping import thread
  pthread_mutex_lock(&aframe_list_lock);
  pthread_cond_signal(&aframe_list_full_cv);
  pthread_mutex_unlock(&aframe_list_lock);

}

//-------------------------------------------------------------------------
//
// check video import status flag and buffer fill level 
//
//-------------------------------------------------------------------------

int vimport_status()
{
  
  int cc;

  pthread_mutex_lock(&import_v_lock);

  cc = vimport;

  pthread_mutex_unlock(&import_v_lock);

  if (cc)
    return 1;
  
  // decoder finished, check for frame list
  pthread_mutex_lock(&vframe_list_lock);
  
  cc = (vframe_list_tail==NULL) ? 0 : 1;
  
  pthread_mutex_unlock(&vframe_list_lock);
  return(cc);
}

//-------------------------------------------------------------------------
//
// major import status query:
//
// 1 = import still active OR some frames still buffered/processed 
// 0 = shutdown as soon as possible
//
//--------------------------------------------------------------------------

int import_status()
{
  if(vimport_status() && aimport_status()) return(1);
  return(0);
}

