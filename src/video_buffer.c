/*
 *  video_buffer.c
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
#include "frame_threads.h"

pthread_mutex_t vframe_list_lock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t vframe_list_full_cv=PTHREAD_COND_INITIALIZER;

vframe_list_t *vframe_list_head;
vframe_list_t *vframe_list_tail;

static int vid_buf_max = 0;
static int vid_buf_next = 0;

static int vid_buf_fill  =0;
static int vid_buf_ready =0;
static int vid_buf_locked=0;
static int vid_buf_empty =0;
static int vid_buf_wait  =0;

static vframe_list_t **vid_buf_ptr; char *vid_buf_mem, **vid_buf_vid_0, **vid_buf_vid_1;

/* ------------------------------------------------------------------ */

static unsigned char *bufalloc(int n, int id, size_t size)
{

#ifdef HAVE_GETPAGESIZE
   int buffer_align=getpagesize();
#else
   int buffer_align=0;
#endif

   char *buf = malloc(size + buffer_align);

   int adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((int) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   if(id==0) vid_buf_vid_0[n] = buf;
   if(id==1) vid_buf_vid_1[n] = buf;

   return (unsigned char *) (buf + adjust);
}


/* ------------------------------------------------------------------ */

static int vid_buf_alloc(int ex_num)
{
    
    /* objectives: 
       ===========
       
       allocate memory for ringbuffer structure
       return -1 on failure, 0 on success
       
    */
    
    int n, num;

    if(ex_num < 0) return(-1);
    
    num = ex_num + 1; //alloc at least one buffer
    
    if((vid_buf_ptr = (vframe_list_t **) calloc(num, sizeof(vframe_list_t *)))==NULL) {
      perror("out of memory");
      return(-1);
    }
    
    if((vid_buf_mem = (char *) calloc(num, sizeof(vframe_list_t)))==NULL) {
      perror("out of memory");
      return(-1);
    }

    if((vid_buf_vid_0 = (char **) calloc(num, sizeof(char *)))==NULL) {
      perror("out of memory");
      return(-1);
    }

    if((vid_buf_vid_1 = (char **) calloc(num, sizeof(char *)))==NULL) {
      perror("out of memory");
      return(-1);
    }
    
    // init ringbuffer
    for (n=0; n<num; ++n) {
	vid_buf_ptr[n] = (vframe_list_t *) (vid_buf_mem + n * sizeof(vframe_list_t));
	
	vid_buf_ptr[n]->status = FRAME_NULL;
	vid_buf_ptr[n]->bufid = n;

	//allocate extra video memory:
	if((vid_buf_ptr[n]->internal_video_buf_0=bufalloc(n, 0, SIZE_RGB_FRAME))==NULL) {
	    perror("out of memory");
	    return(-1);
	}
	
	if((vid_buf_ptr[n]->internal_video_buf_1=bufalloc(n, 1, SIZE_RGB_FRAME))==NULL) {
	    perror("out of memory");
	    return(-1);
	}
	
	//RGB
	vid_buf_ptr[n]->video_buf_RGB[0]=vid_buf_ptr[n]->internal_video_buf_0;
	vid_buf_ptr[n]->video_buf_RGB[1]=vid_buf_ptr[n]->internal_video_buf_1;
	
	//YUV
	vid_buf_ptr[n]->video_buf_Y[0] = vid_buf_ptr[n]->internal_video_buf_0;
	vid_buf_ptr[n]->video_buf_Y[1] = vid_buf_ptr[n]->internal_video_buf_1;

	vid_buf_ptr[n]->video_buf_U[0] = vid_buf_ptr[n]->video_buf_Y[0]
	  + TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT;
	vid_buf_ptr[n]->video_buf_U[1] = vid_buf_ptr[n]->video_buf_Y[1]
	  + TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT;

	vid_buf_ptr[n]->video_buf_V[0] = vid_buf_ptr[n]->video_buf_U[0]
	  + (TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT)/4;
	vid_buf_ptr[n]->video_buf_V[1] = vid_buf_ptr[n]->video_buf_U[1]
	  + (TC_MAX_V_FRAME_WIDTH * TC_MAX_V_FRAME_HEIGHT)/4;

	//default pointer
	vid_buf_ptr[n]->video_buf  = vid_buf_ptr[n]->internal_video_buf_0;
	vid_buf_ptr[n]->video_buf2 = vid_buf_ptr[n]->internal_video_buf_1;
	vid_buf_ptr[n]->free=1;
    }
    
    // assign to static
    vid_buf_max = num;
    
    return(0);
}

    
    
    
/* ------------------------------------------------------------------ */

static void vid_buf_free()
{
    
    /* objectives: 
       ===========
       
       free memory for ringbuffer structure
       
    */

  int n;
  
  if(vid_buf_max > 0) {

    for (n=0; n<vid_buf_max; ++n) {
      free(vid_buf_vid_0[n]);
      free(vid_buf_vid_1[n]);
    }
    free(vid_buf_mem);
    free(vid_buf_ptr);
  }
}
    
/* ------------------------------------------------------------------ */

static vframe_list_t *vid_buf_retrieve()
{
    
    /* objectives: 
       ===========

       retrieve a valid pointer to a vframe_list_t structure
       return NULL on failure, valid pointer on success

       thread safe
       
    */
    
    vframe_list_t *ptr;

    ptr = vid_buf_ptr[vid_buf_next];

    // check, if this structure is really free to reuse

    if(ptr->status != FRAME_NULL) {
      if(verbose & TC_FLIST) fprintf(stderr, "(%s) buffer=%d not empty\n", __FILE__, ptr->status);
      return(NULL);
    }
    
    // ok
    if(verbose & TC_FLIST) printf("alloc = %d [%d]\n", vid_buf_next, ptr->bufid);

    ++vid_buf_next;
    vid_buf_next %= vid_buf_max;
    
    return(ptr);
}



/* ------------------------------------------------------------------ */

static int vid_buf_release(vframe_list_t *ptr)
{
    
    /* objectives: 
       ===========

       release a valid pointer to a vframe_list_t structure
       return -1 on failure, 0 on success

       thread safe
       
    */

    // instead of freeing the memory and setting the pointer
    // to NULL we only change a flag

    if(ptr == NULL) return(-1);
    
    if(ptr->status != FRAME_EMPTY) {
	return(-1);
    } else {
	
	if(verbose & TC_FLIST) printf("release=%d [%d]\n", vid_buf_next, ptr->bufid);
	ptr->status = FRAME_NULL;
	
    }

    return(0);
}


/* ------------------------------------------------------------------ */

int vframe_alloc(int ex_num)
{
  return(vid_buf_alloc(ex_num));
}

void vframe_free()
{
  vid_buf_free();
}

/* ------------------------------------------------------------------ */

vframe_list_t *vframe_register(int id)

{
  
  /* objectives: 
     ===========

     register new frame

     allocate space for frame buffer and establish backward reference
     
     requirements:
     =============

     thread-safe

     global mutex: vframe_list_lock
     
  */

  vframe_list_t *ptr;

  pthread_mutex_lock(&vframe_list_lock);

  // retrive a valid pointer from the pool
  
#ifdef STATBUFFER
  if(verbose & TC_FLIST) printf("frameid=%d\n", id);
  if((ptr = vid_buf_retrieve()) == NULL) {
    pthread_mutex_unlock(&vframe_list_lock);
    return(NULL);
  }
#else 
  if((ptr = malloc(sizeof(vframe_list_t))) == NULL) {
    pthread_mutex_unlock(&vframe_list_lock);
    return(NULL);
  }
#endif
  
  ptr->status = FRAME_EMPTY;
  ++vid_buf_empty;

  ptr->next = NULL;
  ptr->prev = NULL;
  
  ptr->id  = id;
  
  ptr->clone_flag = 0;

 if(vframe_list_tail != NULL)
    {
      vframe_list_tail->next = ptr;
      ptr->prev = vframe_list_tail;
    }
  
  vframe_list_tail = ptr;

  /* first frame registered must set vframe_list_head */

  if(vframe_list_head == NULL) vframe_list_head = ptr;

  // adjust fill level
  ++vid_buf_fill;
  
  pthread_mutex_unlock(&vframe_list_lock);
  
  return(ptr);

}


/* ------------------------------------------------------------------ */

 
void vframe_remove(vframe_list_t *ptr)

{
  
  /* objectives: 
     ===========

     remove frame from chained list

     requirements:
     =============

     thread-safe
     
  */

  
  if(ptr == NULL) return;         // do nothing if null pointer

  pthread_mutex_lock(&vframe_list_lock);
  
  if(ptr->prev != NULL) (ptr->prev)->next = ptr->next;
  if(ptr->next != NULL) (ptr->next)->prev = ptr->prev;
  
  if(ptr == vframe_list_tail) vframe_list_tail = ptr->prev;
  if(ptr == vframe_list_head) vframe_list_head = ptr->next;

  if(ptr->status == FRAME_READY)  --vid_buf_ready;
  if(ptr->status == FRAME_LOCKED) --vid_buf_locked;
  
  // release valid pointer to pool
  ptr->status = FRAME_EMPTY;
  ++vid_buf_empty;

#ifdef STATBUFFER
  vid_buf_release(ptr);
#else
  free(ptr);
#endif
  
  // adjust fill level
  --vid_buf_fill;
  --vid_buf_empty;
  
  pthread_mutex_unlock(&vframe_list_lock); 
  
}

/* ------------------------------------------------------------------ */

 
void vframe_flush()

{
  
  /* objectives: 
     ===========

     remove all frame from chained list

     requirements:
     =============

     thread-safe
     
  */

  vframe_list_t *ptr;
  
  int cc=0;

  while((ptr=vframe_retrieve())!=NULL) {
      if(verbose & TC_DEBUG) fprintf(stderr, "flushing video buffers (%d)\n", ptr->id); 
      vframe_remove(ptr);
      ++cc;
  }
  
  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) flushing %d video buffer\n", __FILE__, cc); 

  pthread_mutex_lock(&vbuffer_im_fill_lock);
  vbuffer_im_fill_ctr=0;
  pthread_mutex_unlock(&vbuffer_im_fill_lock);  
  
  pthread_mutex_lock(&vbuffer_ex_fill_lock);
  vbuffer_ex_fill_ctr=0;
  pthread_mutex_unlock(&vbuffer_ex_fill_lock);

  pthread_mutex_lock(&vbuffer_xx_fill_lock);
  vbuffer_xx_fill_ctr=0;
  pthread_mutex_unlock(&vbuffer_xx_fill_lock);

  return;

}


/* ------------------------------------------------------------------ */


vframe_list_t *vframe_retrieve()

{

  /* objectives: 
     ===========

     get pointer to next frame for rendering
     
     requirements:
     =============
     
     thread-safe
     
  */
  
  vframe_list_t *ptr;

  pthread_mutex_lock(&vframe_list_lock);

  ptr = vframe_list_head;

  /* move along the chain and check for status */

  while(ptr != NULL)
  {
      // we cannot skip a locked frame, since
      // we have to preserve order in which frames are encoded
      if(ptr->status == FRAME_LOCKED)
      {
	  pthread_mutex_unlock(&vframe_list_lock);
	  return(NULL);
      }
  
      //this frame is ready to go
      if(ptr->status == FRAME_READY) 
      {
	  pthread_mutex_unlock(&vframe_list_lock);
	  return(ptr);
      }
      ptr = ptr->next;
  }
  
  pthread_mutex_unlock(&vframe_list_lock);
  
  return(NULL);
}

/* ------------------------------------------------------------------ */


vframe_list_t *vframe_retrieve_status(int old_status, int new_status)

{

  /* objectives: 
     ===========

     get pointer to next frame for rendering
     
     requirements:
     =============
     
     thread-safe
     
  */
  
  vframe_list_t *ptr;

  pthread_mutex_lock(&vframe_list_lock);

  ptr = vframe_list_head;

  /* move along the chain and check for status */

  while(ptr != NULL)
    {
      if(ptr->status == old_status) 
	{
	  
	  // found matching frame
	  
	  if(ptr->status==FRAME_READY)  --vid_buf_ready;
	  if(ptr->status==FRAME_LOCKED) --vid_buf_locked;	  
	  if(ptr->status==FRAME_WAIT)   --vid_buf_wait;

	  ptr->status = new_status;

	  if(ptr->status==FRAME_READY)  ++vid_buf_ready;
	  if(ptr->status==FRAME_LOCKED) ++vid_buf_locked;	  
	  if(ptr->status==FRAME_WAIT)   ++vid_buf_wait;
	  
	  pthread_mutex_unlock(&vframe_list_lock);
	  
	  return(ptr);
	}
      ptr = ptr->next;
    }
  
  pthread_mutex_unlock(&vframe_list_lock);
  
  return(NULL);
}


/* ------------------------------------------------------------------ */


void vframe_set_status(vframe_list_t *ptr, int status)

{

  /* objectives: 
     ===========

     get pointer to next frame for rendering
     
     requirements:
     =============
     
     thread-safe
     
  */

    if(ptr == NULL) return;
  
    pthread_mutex_lock(&vframe_list_lock);
    
    if(ptr->status==FRAME_READY)  --vid_buf_ready;
    if(ptr->status==FRAME_LOCKED) --vid_buf_locked;
    if(ptr->status==FRAME_EMPTY)  --vid_buf_empty;
    if(ptr->status==FRAME_WAIT)   --vid_buf_wait;

    ptr->status = status;
    
    if(ptr->status==FRAME_READY)  ++vid_buf_ready;
    if(ptr->status==FRAME_LOCKED) ++vid_buf_locked;
    if(ptr->status==FRAME_EMPTY)  ++vid_buf_empty;
    if(ptr->status==FRAME_WAIT)   ++vid_buf_wait;

    pthread_mutex_unlock(&vframe_list_lock);
	
    return;
}


/* ------------------------------------------------------------------ */

void vframe_fill_print(int r)
{
  fprintf(stderr, "(V) fill=%d/%d, empty=%d wait=%d locked=%d, ready=%d, tag=%d\n", vid_buf_fill, vid_buf_max, vid_buf_empty, vid_buf_wait, vid_buf_locked, vid_buf_ready, r);

}     

/* ------------------------------------------------------------------ */


int vframe_fill_level(int status)
{

  if(verbose & TC_STATS) vframe_fill_print(status);
    
  //user has to lock vframe_list_lock to obtain a proper result
  
  if(status==TC_BUFFER_FULL  && vid_buf_fill==vid_buf_max) return(1);
  if(status==TC_BUFFER_READY && vid_buf_ready>0) return(1);
  if(status==TC_BUFFER_EMPTY && vid_buf_fill==0) return(1);
  if(status==TC_BUFFER_LOCKED && vid_buf_locked>0) return(1);

  return(0);
}

