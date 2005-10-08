/*
 *  video_buffer.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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
 */

#include "transcode.h"
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
   unsigned long buffer_align=getpagesize();
#else
   unsigned long buffer_align=0;
#endif

   char *buf = malloc(size + buffer_align);

   unsigned long adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((unsigned long) buf) % buffer_align;

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


    int frame_size_max = (tc_frame_width_max + tc_frame_width_max%32) * 
	                 (tc_frame_height_max+ tc_frame_height_max%32) * BPP/8;

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
	if((vid_buf_ptr[n]->internal_video_buf_0=bufalloc(n, 0, frame_size_max))==NULL) {
	    perror("out of memory");
	    return(-1);
	}
	
	if((vid_buf_ptr[n]->internal_video_buf_1=bufalloc(n, 1, frame_size_max))==NULL) {
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
	  + tc_frame_width_max * tc_frame_height_max;
	vid_buf_ptr[n]->video_buf_U[1] = vid_buf_ptr[n]->video_buf_Y[1]
	  + tc_frame_width_max * tc_frame_height_max;

	vid_buf_ptr[n]->video_buf_V[0] = vid_buf_ptr[n]->video_buf_U[0]
	  + (tc_frame_width_max * tc_frame_height_max)/4;
	vid_buf_ptr[n]->video_buf_V[1] = vid_buf_ptr[n]->video_buf_U[1]
	  + (tc_frame_width_max * tc_frame_height_max)/4;

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

static void vid_buf_free(void)
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

static vframe_list_t *vid_buf_retrieve(void)
{
    
    /* objectives: 
       ===========

       retrieve a valid pointer to a vframe_list_t structure
       return NULL on failure, valid pointer on success

       thread safe
       
    */
    
    vframe_list_t *ptr;
    int i;

    ptr = vid_buf_ptr[vid_buf_next];

    i = 0;
    // find an unused ptr, the next one may already be busy
    while (ptr->status != FRAME_NULL && i < vid_buf_max) {
	++i;
	++vid_buf_next;
	vid_buf_next %= vid_buf_max;
	ptr = vid_buf_ptr[vid_buf_next];
    }

    // check, if this structure is really free to reuse

    if(ptr->status != FRAME_NULL) {
      if(verbose & TC_FLIST) {
	  fprintf(stderr, "(%s) buffer=%d (at %p) not empty\n", __FILE__, ptr->status, ptr);
	  // dump ptr list
	  for (i=0; i<vid_buf_max; i++) {
	      fprintf(stderr, "  (%02d) %p<-%p->%p %d %03d\n", i,
		      vid_buf_ptr[i]->prev, vid_buf_ptr[i], vid_buf_ptr[i]->next, 
		      vid_buf_ptr[i]->status,
		      vid_buf_ptr[i]->id);
	  }
	  
      }

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

static void vframe_copy_payload(vframe_list_t *dst, vframe_list_t *src)
{
    if (!dst || !src)
	return;

    // we can't use memcpy here because we don't want
    // to overwrite the pointers to alloc'ed mem

    dst->bufid = src->bufid;
    dst->tag = src->tag;
    dst->filter_id = src->filter_id;
    dst->v_codec = src->v_codec;
    dst->id = src->id;
    dst->status = src->status;
    dst->attributes = src->attributes;
    dst->thread_id = src->thread_id;
    dst->clone_flag = src->clone_flag;
    dst->deinter_flag = src->deinter_flag;
    dst->v_width = src->v_width;
    dst->v_height = src->v_height;
    dst->v_bpp = src->v_bpp;
    dst->video_size = src->video_size;
    dst->plane_mode = src->plane_mode;
    dst->free = src->free;

    // copy video data
    ac_memcpy(dst->video_buf, src->video_buf, dst->video_size);
    ac_memcpy(dst->video_buf2, src->video_buf2, dst->video_size);
}

/* ------------------------------------------------------------------ */

vframe_list_t *vframe_dup(vframe_list_t *f)

{
  
  /* objectives: 
     ===========

     duplicate a frame (for cloning) 

     insert a ptr after f;
     
     requirements:
     =============

     thread-safe

     global mutex: vframe_list_lock
     
  */

  vframe_list_t *ptr;


  pthread_mutex_lock(&vframe_list_lock);

  //fprintf(stderr, "Duplicating (%d)\n", f->id);
  if (!f) {
      if(verbose & TC_FLIST)fprintf(stderr, "Hmm, 1 cannot find a free slot (%d)\n", f->id);
      pthread_mutex_unlock(&vframe_list_lock);
      return (NULL);
  }

  // retrive a valid pointer from the pool
  
#ifdef STATBUFFER
  if((ptr = vid_buf_retrieve()) == NULL) {
    pthread_mutex_unlock(&vframe_list_lock);
    if(verbose & TC_FLIST)fprintf(stderr, "(%s) cannot find a free slot (%d)\n", __FILE__, f->id);
    return(NULL);
  }
#else 
  if((ptr = malloc(sizeof(vframe_list_t))) == NULL) {
    pthread_mutex_unlock(&vframe_list_lock);
    return(NULL);
  }
#endif
  
  vframe_copy_payload (ptr, f);

  ptr->status = FRAME_WAIT;
  ++vid_buf_wait;

  ptr->next = NULL;
  ptr->prev = NULL;
  
  // currently noone cares about this
  ptr->clone_flag = f->clone_flag+1;

  // insert after ptr
  ptr->next = f->next;
  f->next = ptr;
  ptr->prev = f;

 if(!ptr->next) {
     // must be last ptr in the list
     vframe_list_tail = ptr;
 }
  
  // adjust fill level
  ++vid_buf_fill;
  
  pthread_mutex_unlock(&vframe_list_lock);
  
  return(ptr);

}


/* ------------------------------------------------------------------ */


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
      if(verbose & TC_STATS) fprintf(stderr, "flushing video buffers (%d)\n", ptr->id); 
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
  
  // we return "full" (to the decoder) even if there is one framebuffer
  // left so that frames can be cloned without running out of buffers.
  if(status==TC_BUFFER_FULL  && vid_buf_fill>=vid_buf_max-1) return(1);
  if(status==TC_BUFFER_READY && vid_buf_ready>0) return(1);
  if(status==TC_BUFFER_EMPTY && vid_buf_fill==0) return(1);
  if(status==TC_BUFFER_LOCKED && vid_buf_locked>0) return(1);

  return(0);
}

//2003-01-13
void tc_adjust_frame_buffer(int height, int width)
{
  if(height > tc_frame_height_max) tc_frame_height_max=height; 
  if(width > tc_frame_width_max) tc_frame_width_max=width; 
}
